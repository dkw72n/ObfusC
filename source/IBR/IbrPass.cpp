#include "IbrPass.hpp"
#include <llvm/IR/Module.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>

// https://github.com/DreamSoule/ollvm17/blob/main/llvm-project/llvm/lib/Passes/Obfuscation/IndirectBranch.cpp


namespace obfusc {
    IbrPass::IbrPass() {}
    IbrPass::~IbrPass() {}

    bool IbrPass::obfuscate(llvm::Module& mod, llvm::Function& func){
        

        if (func.empty() || func.hasLinkOnceLinkage() || func.getSection() == ".text.startup") {
            return false;
        }

        auto& Ctx = func.getContext();

        // Init member fields
        BBNumbering.clear();
        BBTargets.clear();

        // llvm cannot split critical edge from IndirectBrInst
        llvm::SplitAllCriticalEdges(func, llvm::CriticalEdgeSplittingOptions(nullptr, nullptr));
        number_basic_blocks(func);

        if (BBNumbering.empty()) {
            return false;
        }

        uint64_t V = 0x1234; // RandomEngine.get_uint64_t();
        llvm::IntegerType *intType = llvm::Type::getInt32Ty(Ctx);
        unsigned pointerSize =
            func.getEntryBlock().getModule()->getDataLayout().getTypeAllocSize(
                llvm::PointerType::getUnqual(func.getContext())); // Soule
        if (pointerSize == 8) {
            intType = llvm::Type::getInt64Ty(Ctx);
        }
        auto EncKey = llvm::ConstantInt::get(intType, V, false);
        auto EncKey1 = llvm::ConstantInt::get(intType, -V, false);

        auto MySecret = llvm::ConstantInt::get(intType, 0, true);

        auto Zero = llvm::ConstantInt::get(intType, 0);
        auto DestBBs = get_indirect_targets(func, EncKey1);

        for (auto &BB : func) {
            auto *BI = dyn_cast<llvm::BranchInst>(BB.getTerminator());
            if (BI && BI->isConditional()) {
                
                llvm::IRBuilder<> IRB(BI);

                llvm::Value *Cond = BI->getCondition();
                llvm::Value *Idx;
                llvm::Value *TIdx, *FIdx;

                TIdx = llvm::ConstantInt::get(intType, BBNumbering[BI->getSuccessor(0)]);
                FIdx = llvm::ConstantInt::get(intType, BBNumbering[BI->getSuccessor(1)]);
                Idx = IRB.CreateSelect(Cond, TIdx, FIdx);

                llvm::Value *GEP =
                    IRB.CreateGEP(DestBBs->getValueType(), DestBBs, {Zero, Idx});
                llvm::Value *EncDestAddr =
                    IRB.CreateLoad(GEP->getType(), GEP, "EncDestAddr");
                // -EncKey = X - FuncSecret
                llvm::Value *DecKey = IRB.CreateAdd(EncKey, MySecret);
                llvm::Value *DestAddr =
                    IRB.CreateGEP(llvm::Type::getInt8Ty(Ctx), EncDestAddr, DecKey);

                auto IBI = llvm::IndirectBrInst::Create(DestAddr, 2);
                IBI->addDestination(BI->getSuccessor(0));
                IBI->addDestination(BI->getSuccessor(1));
                llvm::ReplaceInstWithInst(BI, IBI);
            }
        }
        return true;
    }

    llvm::GlobalVariable * IbrPass::get_indirect_targets(llvm::Function &F, llvm::ConstantInt *EncKey) {
        std::string GVName(F.getName().str() + "_IndirectBrTargets");
        auto GV = F.getParent()->getNamedGlobal(GVName);
        if (GV)
            return GV;

        // encrypt branch targets
        std::vector<llvm::Constant *> Elements;
        for (const auto BB : BBTargets) {
            llvm::Constant *CE = llvm::ConstantExpr::getBitCast(llvm::BlockAddress::get(BB),
                                                    llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(F.getContext())));
            CE = llvm::ConstantExpr::getGetElementPtr(llvm::Type::getInt8Ty(F.getContext()), CE,
                                                EncKey);
            Elements.push_back(CE);
        }

        llvm::ArrayType *ATy =
            llvm::ArrayType::get(llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(F.getContext())), Elements.size());
        llvm::Constant *CA = llvm::ConstantArray::get(ATy, llvm::ArrayRef<llvm::Constant *>(Elements));
        GV =
            new llvm::GlobalVariable(*F.getParent(), ATy, false,
                                llvm::GlobalValue::LinkageTypes::PrivateLinkage, CA, GVName);
        llvm::appendToCompilerUsed(*F.getParent(), {GV});
        return GV;
    }

    void IbrPass::number_basic_blocks(llvm::Function& F){
        for (auto &BB : F) {
            if (auto *BI = dyn_cast<llvm::BranchInst>(BB.getTerminator())) {
            if (BI->isConditional()) {
                unsigned N = BI->getNumSuccessors();
                for (unsigned I = 0; I < N; I++) {
                auto Succ = BI->getSuccessor(I);
                if (BBNumbering.count(Succ) == 0) {
                    BBTargets.push_back(Succ);
                    BBNumbering[Succ] = 0;
                }
                }
            }
            }
        }

        long seed = 0x11; // RandomEngine.get_uint32_t();
        std::default_random_engine e(seed);
        std::shuffle(BBTargets.begin(), BBTargets.end(), e);

        unsigned N = 0;
        for (auto BB : BBTargets) {
            BBNumbering[BB] = N++;
        }
    }
}