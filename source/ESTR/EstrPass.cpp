#include "EstrPass.hpp"
#include <llvm/IR/Module.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>

#include <random>

static std::random_device rd; // random device engine, usually based on /dev/random on UNIX-like systems
// initialize Mersennes' twister using rd to generate the seed
static std::mt19937 rng{rd()}; 

// https://github.com/DreamSoule/ollvm17/blob/main/llvm-project/llvm/lib/Passes/Obfuscation/IndirectBranch.cpp


static OBfsRegister<obfusc::EstrPass> sRegEstr("estr");

llvm::Value* MakeN(llvm::LLVMContext& Context, llvm::IRBuilder<>& IRB, llvm::Value* Value, int32_t N);

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

static std::vector<int32_t> shortStringToI32List(llvm::StringRef& s){
    auto cs = s.str();
    std::vector<int32_t> ret;
    ret.resize((s.size() + sizeof(ret[0])) / sizeof(ret[0]));
    memcpy(&ret[0], cs.c_str(), cs.size() + 1);
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
                        
                        if (s.size() >= 4){
                            llvm::outs() << "\t[X] [TODO] (size == " << s.size() << ") >= 8\n";
                            continue;
                        }
                        
                        llvm::outs() << "\t[O] REMOVING " << s.size() << "\n";
                        removing.insert({&G, shortStringToI32List(s)});
                    }
                    
                }
            }
        }
    }
    bool EstrPass::obfuscate(llvm::Module& mod, llvm::Function& func){
        collectRemovables(mod);
        llvm::outs() << "[ESTR]" << func.getName() << "\n";
        auto& Context = mod.getContext();
        auto Int8PtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(mod.getContext()));
        auto Int32Ty = llvm::Type::getInt32Ty(Context);
        bool changed = false;
        for(auto& BB: func){
            for(auto& I: BB){
                for(auto& Op: I.operands()){
                    auto *G = llvm::dyn_cast<llvm::GlobalVariable>(Op->stripPointerCasts());
                    if (removing.contains(G)){
                        if (auto CI = llvm::dyn_cast<llvm::CallInst>(&I)){
                            llvm::IRBuilder<> IRB(&I);
                            auto AOR = IRB.CreatePtrToInt(
                                IRB.CreateIntrinsic(Int8PtrTy, llvm::Intrinsic::addressofreturnaddress, {}, {}),
                                Int32Ty
                            );
                            auto V = removing[G];
                            auto SS = IRB.CreateAlloca(Int32Ty, llvm::ConstantInt::get(Int32Ty, V.size()));
                            for(size_t i = 0; i < V.size(); ++i){
                                switch(i % 3 + 3){
                                    case 0:
                                        IRB.CreateStore(
                                            MakeN(Context, IRB, AOR, V[i]), 
                                            IRB.CreateGEP(Int32Ty, SS, llvm::ConstantInt::get(Int32Ty, i))
                                        );
                                        break;
                                    case 1:
                                        IRB.CreateStore(
                                            llvm::ConstantInt::get(Int32Ty, V[i]), 
                                            IRB.CreateGEP(Int32Ty, SS, MakeN(Context, IRB, AOR, i))
                                        );
                                        break;
                                    default:
                                        IRB.CreateStore(
                                            llvm::ConstantInt::get(Int32Ty, V[i]), 
                                            IRB.CreateGEP(Int32Ty, SS, llvm::ConstantInt::get(Int32Ty, i))
                                        );
                                        break;
                                }
                            }
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