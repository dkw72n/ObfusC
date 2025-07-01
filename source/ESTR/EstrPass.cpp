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

struct TaintAnalysis{
    static constexpr bool verbose = false;
    TaintAnalysis(llvm::Function& Fun, llvm::Value* V): func(Fun){
        tainted.insert(V);
    }

    bool Check(){
        while(_run());
        for(auto& a: func.args()){
            if (tainted.contains(&a)){
                if constexpr (verbose){
                    llvm::outs() << "[-] [" << func.getName() << "]  FOUND TAINTED ARGS:"; a.dump();
                }
                return false;
            }
        }
        for(auto r: retval){
            if (tainted.contains(r)){
                if constexpr (verbose){
                    llvm::outs() << "[-] [" << func.getName() << "]  FOUND TAINTED RETS:"; r->dump();
                }
                return false;
            }
        }
        for(auto c: cmpval){
            if (tainted.contains(c)){
                if constexpr (verbose){
                    llvm::outs() << "[-] [" << func.getName() << "]  FOUND TAINTED CMP:"; c->dump();
                }
                return false;
            }
        }
        for(auto t: tailcall){
            if (tainted.contains(t)){
                if constexpr (verbose){
                    llvm::outs() << "[-] [" << func.getName() << "]  FOUND TAINTED TAILCALL:"; t->dump();
                }
                return false;
            }
        }
        return true;
        // return false;
    }
private:
    bool _run(){
        bool updated = false;
        for (auto& BB: func){
            for(auto& I: BB){
                if (auto D = llvm::dyn_cast<llvm::StoreInst>(&I)){
                    auto dst = D->getPointerOperand();
                    auto src = D->getValueOperand();
                    if (tainted.contains(src)){
                        auto res = tainted.insert(dst);
                        updated |= res.second;
                        if (res.second){if constexpr (verbose){llvm::outs() << "\t # "; D->dump();}}
                    }
                    if (tainted.contains(dst)){
                        auto res = tainted.insert(src);
                        updated |= res.second;
                        if (res.second){if constexpr (verbose){llvm::outs() << "\t # "; D->dump();}}
                    }
                    continue;
                }
                if (auto D = llvm::dyn_cast<llvm::SelectInst>(&I)){
                    auto tval = D->getTrueValue();
                    auto fval = D->getFalseValue();
                    if (tainted.contains(tval) || tainted.contains(fval)){
                        auto res = tainted.insert(D);
                        updated |= res.second;
                        if (res.second){if constexpr (verbose){llvm::outs() << "\t # "; D->dump();}}
                    }
                    continue;
                }
                if (auto D = llvm::dyn_cast<llvm::GetElementPtrInst>(&I)){
                    auto ptr = D->getPointerOperand();
                    if (tainted.contains(ptr)){
                        auto res = tainted.insert(D);
                        updated |= res.second;
                        if (res.second){if constexpr (verbose){llvm::outs() << "\t # "; D->dump();}}
                    }
                    if (tainted.contains(D)){
                        auto res = tainted.insert(ptr);
                        updated |= res.second;
                        if (res.second){if constexpr (verbose){llvm::outs() << "\t # "; D->dump();}}
                    }
                    continue;
                }
                if (auto D = llvm::dyn_cast<llvm::LoadInst>(&I)){
                    auto src = D->getPointerOperand();
                    if (tainted.contains(src)){
                        auto res = tainted.insert(D);
                        updated |= res.second;
                        if (res.second){if constexpr (verbose){llvm::outs() << "\t # "; D->dump();}}
                    }
                    if (tainted.contains(D)){
                        auto res = tainted.insert(src);
                        updated |= res.second;
                        if (res.second){if constexpr (verbose){llvm::outs() << "\t # "; D->dump();}}
                    }
                    continue;
                }
                if (auto D = llvm::dyn_cast<llvm::CallInst>(&I)){
                    bool arg_tainted = false;
                    for(auto& arg: D->args()){
                        if (tainted.contains(arg)){
                            arg_tainted = true;
                            break;
                        }
                    }
                    if (D->isTailCall()){
                        tailcall.insert(D);
                        if (arg_tainted){
                            auto res = tainted.insert(D);
                            updated |= res.second;
                            if (res.second){if constexpr (verbose){llvm::outs() << "\t # "; D->dump();}}
                        }
                    }
                    
                    continue;
                }
                if (auto D = llvm::dyn_cast<llvm::PHINode>(&I)){
                    for (auto &v: D->incoming_values()){
                        if (tainted.contains(v.get())){
                            auto res = tainted.insert(D);
                            updated |= res.second;
                            if (res.second){if constexpr (verbose){llvm::outs() << "\t # "; D->dump();}}
                            break;
                        }
                    }
                    continue;
                }
                if (auto D = llvm::dyn_cast<llvm::ReturnInst>(&I)){
                    if (auto ret = D->getReturnValue()){
                        retval.insert(ret);
                    }
                    continue;
                }
                if (auto D = llvm::dyn_cast<llvm::ICmpInst>(&I)){
                    for(auto& op: D->operands()){
                        cmpval.insert(op.get());
                    }
                    continue;
                }
                // llvm::outs() << "\t SKIP "; I.dump();
            }
        }
        return updated;
    }

    std::set<llvm::Value*> tainted;
    std::set<llvm::Value*> retval;
    std::set<llvm::Value*> cmpval;
    std::set<llvm::Value*> tailcall;
    llvm::Function& func;
};
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
                        if (llvm::dyn_cast<llvm::PHINode>(&I)){
                            llvm::outs() << "[+] skip PhiNode: "; I.dump();
                            continue;
                        }
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
                        [?] StoreInst   : -ldebug.c
                        [?] SelectInst  : -ldebug.c
                        [x] PhiNode
                        TODO: 
                            跟踪其去向，看是否传到外面了
                            PhiNode
                        */
                        if (!TaintAnalysis(func, G).Check()) continue;
                        // if (llvm::dyn_cast<llvm::CallInst>(&I) || llvm::dyn_cast<llvm::LoadInst>(&I) || llvm::dyn_cast<llvm::SelectInst>(&I) || llvm::dyn_cast<llvm::StoreInst>(&I))
                        if(true)
                        {
                            //if (llvm::dyn_cast<llvm::SelectInst>(&I) || llvm::dyn_cast<llvm::StoreInst>(&I)){
                            //    llvm::outs() << "[WARN][" << mod.getName() << "][" << func.getName(); I.dump();G->dump();
                            //}
                            llvm::IRBuilder<> IRB(&I);
                            auto AOR = IRB.CreatePtrToInt(
                                IRB.CreateIntrinsic(Int8PtrTy, llvm::Intrinsic::addressofreturnaddress, {}, {}),
                                Int32Ty
                            );
                            auto V = cstrings[G];
                            auto SS = IRB.CreateAlloca(Int32Ty, llvm::ConstantInt::get(Int32Ty, V.size()));
                            for(size_t i = 0; i < V.size(); ++i){
                                switch(rng() % 3 + 3){
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
                                llvm::outs() << "[ERASE]" << G->getName() << "\n";
                                // G->dropAllReferences();
                                G->eraseFromParent();
                            }
                            changed |= true;
                        } else {
                            llvm::outs() << "UNHANDLED: "; G->dump();
                            I.dump();
                        }
                    }
                }
            }
        }
        return changed;
    }
}