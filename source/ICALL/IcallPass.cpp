#include "IcallPass.hpp"
#include <llvm/IR/Module.h>
#include <llvm/IR/AbstractCallSite.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>

#include <random>
#include <vector>
#include <map>

static constexpr bool TRACE_ICALL = false;

// https://github.com/DreamSoule/ollvm17/blob/main/llvm-project/llvm/lib/Passes/Obfuscation/IndirectCall.cpp



static void number_callees(llvm::Function& F, obfusc::CalleeMap& M){
    for(auto& BB: F){
        for(auto& I: BB){
            if (auto CI = llvm::dyn_cast<llvm::CallInst>(&I)){
                // llvm::outs() << "Found CallInst:" << CI << "\n";
                auto Callee = CI->getCalledFunction();
                if (!Callee) {
                    continue;
                }

                if (Callee->isIntrinsic()){
                    continue;
                }

                M.insert(CI);
                // llvm::outs() << "  calling (" << Callee << ")" << Callee->getName() << "\n";
            }
        }
    }
    M.shuffle();
}


static llvm::GlobalVariable* make_function_list(llvm::Module& M, obfusc::CalleeMap& CM){
    std::string GVName = CM.name("icall_gv_");
    llvm::GlobalVariable *GV = M.getNamedGlobal(GVName);
    // llvm::outs() << "GVName: " << GVName << "\n";
    if (GV) return GV;
    std::vector<llvm::Constant *> Elements;
    auto Int8PtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(M.getContext()));
    for (size_t i = 0; i < CM.size(); ++i){
        auto F = CM.getFunc(i);
        llvm::Constant *CE = nullptr;
        if (F){
            CE = llvm::ConstantExpr::getBitCast(CM.getFunc(i), Int8PtrTy);
        } else {
            CE = llvm::ConstantPointerNull::get(Int8PtrTy);
        }
        // CE = llvm::ConstantExpr::getGetElementPtr(Int8PtrTy, CE, EncKey);
        Elements.push_back(CE);
    }
    llvm::ArrayType *ATy = llvm::ArrayType::get(Int8PtrTy, CM.size());
    auto CArr = llvm::ConstantArray::get(ATy, llvm::ArrayRef<llvm::Constant *>(Elements));
    GV = new llvm::GlobalVariable(M, ATy, true, llvm::GlobalValue::LinkageTypes::PrivateLinkage, CArr, GVName);
    llvm::appendToCompilerUsed(M, {GV});
    return GV;
}




// static OBfsRegister<obfusc::IcallPass> sRegIcall("icall");


namespace obfusc {
    IcallPass::IcallPass() {}
    IcallPass::~IcallPass() {}

    void IcallPass::collectCallables(llvm::Module& mod){
        if (touched) return;
        touched = true;
        dispatchTable = nullptr;
        for(auto& F: mod){
            for(auto& BB: F){
                for(auto& I: BB){
                    if (auto CI = llvm::dyn_cast<llvm::CallInst>(&I)){
                        // llvm::outs() << "Found CallInst:" << CI << "\n";
                        auto Callee = CI->getCalledFunction();
                        if (!Callee) {
                            continue;
                        }

                        if (Callee->isIntrinsic()){
                            continue;
                        }

                        M.insert(CI);
                        if constexpr (TRACE_ICALL){
                            llvm::outs() << "  calling (" << Callee << ")" << Callee->getName() << "\n";
                        }
                    }
                    if (auto II = llvm::dyn_cast<llvm::InvokeInst>(&I)){
                        auto Callee = II->getCalledFunction();
                        if (!Callee) {
                            continue;
                        }
                        if (Callee->isIntrinsic()){
                            continue;
                        }
                        M.insert(II);
                        if constexpr (TRACE_ICALL){
                            llvm::outs() << "  invoking (" << Callee << ")" << Callee->getName() << "\n";
                        }
                    }
                }
            }
        }
        M.shuffle();
        if (M.empty())
            return;
        llvm::outs() << "[MOD] [" << mod.getName() << "] TOTAL " << M.size() << "Callees\n";
        dispatchTable = make_function_list(mod, M);
    }
    bool IcallPass::obfuscate(llvm::Module& mod, llvm::Function& func){
        collectCallables(mod);
        // CalleeMap CM;
        // llvm::outs() << "[-] in icall\n";
        auto& ctx = func.getContext();
        const auto &DL = mod.getDataLayout();
        auto Int8PtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(mod.getContext()));
        auto Int32Ty = llvm::Type::getInt32Ty(mod.getContext());
        auto Int64Ty = llvm::Type::getInt64Ty(mod.getContext());
        auto IntPtrTy = DL.getIntPtrType(ctx); 
        for(auto CI: M.callsites){
            if (CI->getParent()->getParent() != &func) continue;
            llvm::IRBuilder<> IRB(CI);
            auto AOR = IRB.CreatePtrToInt(
                IRB.CreateIntrinsic(Int8PtrTy, llvm::Intrinsic::addressofreturnaddress, {}, {}),
                Int32Ty
            );
            int32_t Shift = rng() % M.size();
            auto TargetBase = llvm::ConstantExpr::getGetElementPtr(
                Int8PtrTy,
                dispatchTable,
                llvm::ConstantInt::get(Int32Ty, Shift)
            );
            auto IOR = rng() % 3 ? IRB.CreateLoad(Int32Ty, TargetBase) : AOR;
            CI->setCalledOperand(IRB.CreateBitCast(
                IRB.CreateLoad(
                    Int8PtrTy,
                    IRB.CreateIntToPtr(
                        IRB.CreateAdd(
                            IRB.CreatePtrToInt(
                                TargetBase,
                                IntPtrTy
                            ),
                            IRB.CreateSExt(
                                MakeN64(mod.getContext(), IRB, IOR, (-Shift + M.getIdx(CI->getCalledFunction())) * (IntPtrTy->getBitWidth() / 8)),
                                IntPtrTy
                            )
                            // llvm::ConstantInt::get(IntPtrTy, (-Shift + M.getIdx(CI->getCalledFunction())) * (IntPtrTy->getBitWidth() / 8))
                        ),
                        Int8PtrTy
                    )
                ),
                CI->getFunctionType()->getPointerTo()
            ));
        }
        return true;
    }
    bool IcallPass::fini() {
        M.clear();
        touched = false;
        dispatchTable = nullptr;
        return false;
    }
}