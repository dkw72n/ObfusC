#include "EstrPass.hpp"
#include <llvm/IR/Module.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>

#include <random>

static std::random_device rd; // random device engine, usually based on /dev/random on UNIX-like systems
// initialize Mersennes' twister using rd to generate the seed
static std::mt19937 rng{rd()}; 

// https://github.com/DreamSoule/ollvm17/blob/main/llvm-project/llvm/lib/Passes/Obfuscation/IndirectBranch.cpp


static OBfsRegister<obfusc::EstrPass> sRegEstr("estr");

static bool isArgOfKnownCalls(llvm::CallInst* CI){
    return !!CI;
}

static bool safeToRemove(llvm::GlobalVariable& G){
    for(auto *U:G.users()){
        if (!isArgOfKnownCalls(llvm::dyn_cast<llvm::CallInst>(U))) {
            llvm::outs() << "\t[X] "; U->dump();
            return false;
        }
    }
    return true;
}

static int64_t shortStringToI64(llvm::StringRef& s){
    auto cs = s.str();
    auto ret = *(int64_t*)cs.c_str();
    return ret;
}
namespace obfusc {
    EstrPass::EstrPass():touched(false) {}
    EstrPass::~EstrPass() {}

    void EstrPass::collectRemovables(llvm::Module& mod){
        if (touched) return;
        touched = true;
        for(auto& G: mod.globals()){
            if (!G.isConstant())
                continue;
            if (G.hasExternalLinkage())
                continue;
            if (!G.hasInitializer())
                continue;
            if (G.getSection() == "llvm.metadata")
                continue;
            auto Init = G.getInitializer();
            if (!Init)
                continue;
            if (auto CDS = llvm::dyn_cast<llvm::ConstantDataSequential>(Init)){
                if (CDS->isCString()){
                    auto s = CDS->getAsCString();
                    llvm::outs() << "G:";
                    G.dump();    
                    if (safeToRemove(G)){
                        if (s.size() >= 8){
                            llvm::outs() << "\t[X] [TODO] (size == " << s.size() << ") >= 8\n";
                            continue;
                        }
                        llvm::outs() << "\t[O] REMOVING " << s.size() << "\n";
                        removing.insert({&G, shortStringToI64(s)});
                    }
                    
                }
            }
        }
    }
    bool EstrPass::obfuscate(llvm::Module& mod, llvm::Function& func){
        collectRemovables(mod);
        llvm::outs() << "[ESTR]" << func.getName() << "\n";
        auto Int64Ty = llvm::Type::getInt64Ty(mod.getContext());
        bool changed = false;
        for(auto& BB: func){
            for(auto& I: BB){
                for(auto& Op: I.operands()){
                    auto *G = llvm::dyn_cast<llvm::GlobalVariable>(Op->stripPointerCasts());
                    if (removing.contains(G)){
                        if (auto CI = llvm::dyn_cast<llvm::CallInst>(&I)){
                            llvm::IRBuilder<> IRB(&I);
                            auto SS = IRB.CreateAlloca(Int64Ty);
                            IRB.CreateStore(llvm::ConstantInt::get(Int64Ty, removing[G]), SS);
                            CI->replaceUsesOfWith(G, SS);
                            llvm::outs() << "[M] "; G->dump();
                            changed |= true;
                        }
                    }
                }
            }
        }

        return true;
    }
}