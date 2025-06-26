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

static bool checkUsers(llvm::GlobalVariable& G){
    for(auto *U:G.users()){
        if (auto CI = llvm::dyn_cast<llvm::CallInst>(U)) {
            // llvm::outs() << "\t[CALL] "; CI->dump();
            continue;
        }
        if (auto SI = llvm::dyn_cast<llvm::SelectInst>(U)) {
            llvm::outs() << "\t[SLCT] "; SI->dump();
            continue;
        }
        if (auto CD = llvm::dyn_cast<llvm::ConstantArray>(U)){
            llvm::outs() << "\t[KARR] "; CD->dump();
            continue;
        }
        if (auto CS = llvm::dyn_cast<llvm::ConstantStruct>(U)){
            llvm::outs() << "\t[STRU] "; CS->dump();
            continue;
        }
        llvm::outs() << "\t[????] "; U->dump();
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
                    // llvm::outs() << "G:";
                    // G.dump();    
                    cstrings.insert({&G, shortStringToI32List(s)});
                    // checkUsers(G);
                }
            }
        }
    }
    bool EstrPass::obfuscate(llvm::Module& mod, llvm::Function& func){
        collectRemovables(mod);
        auto& Context = mod.getContext();
        auto Int8PtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(mod.getContext()));
        auto Int32Ty = llvm::Type::getInt32Ty(Context);
        bool changed = false;
        for(auto& BB: func){
            for(auto& I: BB){
                
                for(auto& Op: I.operands()){
                    auto *G = llvm::dyn_cast<llvm::GlobalVariable>(Op->stripPointerCasts());
                    if (cstrings.contains(G)){
                        #if 0
                        if (!llvm::dyn_cast<llvm::CallInst>(&I)) {
                            llvm::outs() << "UNKNOWN INST:" ; I.dump();
                            continue;
                        }
                        #endif
                        /*
                        Test result on lua 5.4.8:
                        [o] CallInst
                        [o] LoadInst
                        [x] StoreInst
                        [x] SelectInst
                        [x] PhiNode
                        */
                        if (llvm::dyn_cast<llvm::CallInst>(&I) || llvm::dyn_cast<llvm::LoadInst>(&I))
                        {
                            llvm::IRBuilder<> IRB(&I);
                            auto AOR = IRB.CreatePtrToInt(
                                IRB.CreateIntrinsic(Int8PtrTy, llvm::Intrinsic::addressofreturnaddress, {}, {}),
                                Int32Ty
                            );
                            auto V = cstrings[G];
                            auto SS = IRB.CreateAlloca(Int32Ty, llvm::ConstantInt::get(Int32Ty, V.size()));
                            for(size_t i = 0; i < V.size(); ++i){
                                switch(i % 3){
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
                            // llvm::outs() << "  [I] "; I.dump();
                            I.replaceUsesOfWith(G, SS);
                            // llvm::outs() << "  [M] "; G->dump();
                            G->removeDeadConstantUsers();
                            if (G->use_empty()){
                                // llvm::outs() << "  OK TO REMOVE\n";
                                G->removeFromParent();
                            }
                            changed |= true;
                        } else {
                            llvm::outs() << "UNHANDLED: "; G->dump();
                        }
                    }
                }
            }
        }
        if (changed){
            // llvm::outs() << "[ESTR]" << func.getName() << "\n";
        }
        return true;
    }
}