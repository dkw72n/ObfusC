#include "UcdPass.hpp"
#include <llvm/IR/Module.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>
#include <llvm/Transforms/Utils/Cloning.h>

static OBfsRegister<obfusc::UcdPass> sRegUcd("ucd");

llvm::Value* MakeN(llvm::LLVMContext& Context, llvm::IRBuilder<>& IRB, llvm::Value* Value, int32_t N);

static llvm::Value* make_n(llvm::LLVMContext& Context, llvm::IRBuilder<>& IRB, int32_t N)
{
    auto Int32Ty = llvm::Type::getInt32Ty(Context); 
    auto Int8PtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(Context));
    auto AOR = IRB.CreatePtrToInt(
        IRB.CreateIntrinsic(Int8PtrTy, llvm::Intrinsic::addressofreturnaddress, {}, {}),
        Int32Ty
    );
    return MakeN(Context, IRB, AOR, N);
}
static std::string make_flag_name(llvm::Value* v){
    char name[64];
    sprintf(name, "_%p_ucd_inited", v);
    return {name};
}

static std::string make_store_name(llvm::Value* v){
    char name[64];
    sprintf(name, "_%p_ucd_store", v);
    return {name};
}
static const uint64_t fnv1a_offset = 0xcbf29ce484222325;
static const uint64_t fnv1a_prime = 0x100000001b3;
uint64_t fnv1a(llvm::StringRef s){
    auto h = fnv1a_offset;
    for(auto c: s){
        h ^= c;
        h *= fnv1a_prime;
    }
    return h;
}

static std::string make_flag_name(llvm::StringRef s){
    auto h = fnv1a(s);
    char name[64];
    sprintf(name, "_fnv1a_%lx_ucd_inited", h);
    return {name};
}

static std::string make_store_name(llvm::StringRef s){
    auto h = fnv1a(s);
    char name[64];
    sprintf(name, "_fnv1a_%lx_ucd_store", h);
    return {name};
}

static std::string make_func_name(llvm::StringRef s){
    auto h = fnv1a(s);
    char name[64];
    sprintf(name, "_fnv1a_%lx_ucd_func", h);
    return {name};
}

static std::vector<int32_t> shortStringToI32List(llvm::StringRef& s){
    auto cs = s.str();
    std::vector<int32_t> ret;
    ret.resize((s.size() + sizeof(ret[0])) / sizeof(ret[0]));
    memcpy(&ret[0], cs.c_str(), cs.size() + 1);
    return ret;
}

namespace obfusc {
    static constexpr bool log_level = 2;
    UcdPass::UcdPass():touched(false) {}
    UcdPass::~UcdPass() {}

    void UcdPass::collectRemovables(llvm::Module& mod){
        if (touched) return;
        touched = true;
        llvm::outs() << "[UCD]\n";
        for(auto& G: mod.globals()){
            // if (!G.isConstant())
            //    continue;
            if (G.hasExternalLinkage())
                continue;
            if (!G.hasInitializer())
                continue;
            if (G.getSection() == "llvm.metadata")
                continue;
            auto Init = G.getInitializer();
            if (!Init)
                continue;
            // llvm::outs() << "G:";
            // G.dump();    
            // llvm::outs() << " init: ";
            // Init->dump();
            
            bool accepted = true;
            for(auto* user: G.users()){
                if (auto I = llvm::dyn_cast<llvm::Instruction>(user)){
                    if constexpr(log_level >=2 ) {llvm::outs() << "[.] Inst:" << I->getOpcodeName() << "\n";}
                } else if(auto CE = llvm::dyn_cast<llvm::ConstantExpr>(user)){
                    if constexpr(log_level >=2 ) {llvm::outs() << "[.] CExp:" << CE->getOpcodeName() << "\n";}
                    accepted = false; // FIXME:
                    break;
                } else {
                    if constexpr(log_level >=2 ) {
                        llvm::outs() << "[.] Uknw:"; user->dump();
                    }
                    accepted = false;
                    break;
                }
            }
            if (accepted){
                llvm::outs() << "INSERT " <<  G.getName() << "\n";
                targets.insert(&G);
            }
        }
    }
    bool UcdPass::obfuscate(llvm::Module& mod, llvm::Function& func){
        collectRemovables(mod);
        std::set<llvm::CallInst*> ci_to_inline;
        bool changed = false;
        for(auto& BB: func){
            for(auto& I: BB){
                for(auto& Op: I.operands()){
                    if (I.getOpcode() == llvm::Instruction::PHI) continue;
                    if (auto G  = llvm::dyn_cast<llvm::GlobalVariable>(Op->stripPointerCasts())){
                        
                        llvm::outs() << "found g: " << G->getName() << ", "; Op->getType()->dump();
                        G->getValueType()->dump();
                        if (targets.contains(G)){
                            // 1. insert lazy init code in beginning
                            // 2. replace op with inited ptr
                            llvm::outs() << "[-] [" << func.getName() << "] MODIFY " << G->getName() << " IN "; I.dump();
                            if (auto v = insertLazyInit(mod, func, G)){
                                if (auto F = llvm::dyn_cast<llvm::Function>(v)){
                                    llvm::IRBuilder<> IRB(&I);
                                    
                                    auto CI = IRB.CreateCall(F->getFunctionType(), F, {});
                                    
                                    ci_to_inline.insert(CI);
                                    I.replaceUsesOfWith(G, CI);
                                    changed |= true;
                                }
                            }

                            #if 0
                            if (G->use_empty()){
                                llvm::outs() << "[=] [ERASE] " << G->getName() << "\n";
                                // G->dropAllReferences();
                                G->eraseFromParent();
                            }
                            #endif
                        }
                    }
                    
                }
            }
        }
        
        for(auto ci: ci_to_inline){
            llvm::InlineFunctionInfo IFI;
            auto Res = llvm::InlineFunction(*ci, IFI);
            if (!Res.isSuccess()){
                llvm::errs() << "[!] " << Res.getFailureReason() << "\n";
            }
        }
        return changed;
    }

    llvm::Value* UcdPass::insertLazyInitCString(llvm::Module& mod, llvm::Function& func, llvm::StringRef s){
        auto& Context = mod.getContext();
        auto Int32Ty = llvm::Type::getInt32Ty(Context); 
        auto Int8PtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(Context));
        auto Int32PtrTy = llvm::PointerType::getUnqual(Int32Ty);

        std::string FlagName = make_flag_name(s);
        std::string StoreName = make_store_name(s);
        std::string FuncName = make_func_name(s);
        // 创建标记
        
        if (auto F = mod.getFunction(FuncName)){
            return F;
        }
        auto FlagVar = mod.getNamedGlobal(FlagName);
        if (!FlagVar){
            FlagVar = new llvm::GlobalVariable(mod, Int32Ty, false, llvm::GlobalValue::LinkageTypes::PrivateLinkage, llvm::ConstantInt::get(Int32Ty, 0), FlagName);
            llvm::appendToCompilerUsed(mod, {FlagVar});
        }
        // 创建数组
        auto vec_i32 = shortStringToI32List(s);
        llvm::ArrayType *ArrayTy = llvm::ArrayType::get(Int32Ty, vec_i32.size());
        auto StoreVar = mod.getNamedGlobal(StoreName);
        if (!StoreVar){
            StoreVar = new llvm::GlobalVariable(mod, 
                ArrayTy,
                false, 
                llvm::GlobalValue::LinkageTypes::PrivateLinkage, 
                llvm::Constant::getNullValue(ArrayTy),
                StoreName
            );
            StoreVar->setAlignment(llvm::Align(4));
            llvm::appendToCompilerUsed(mod, {StoreVar});
        }
        // 创建函数
        auto FuncType = llvm::FunctionType::get(
            Int8PtrTy, // Return type: ptr (i8*)
            false       // isVarArg: no variadic arguments
        );

        auto F = llvm::Function::Create(
            FuncType,
            llvm::GlobalValue::PrivateLinkage,   // Start with external linkage
            FuncName,                            // Name
            &mod                                 // Module to add the function to
        );
        F->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Local);

        // c) noundef 和 nonnull (这些是返回值的属性)
        //    我们需要创建一个 AttributeList 来应用它们
        llvm::AttributeList AttrList;

        // 将这个属性集合应用到返回值上 (索引为 ReturnIndex)
        AttrList = AttrList.addRetAttribute(Context, llvm::Attribute::get(mod.getContext(), llvm::Attribute::NoUndef));
        AttrList = AttrList.addRetAttribute(Context, llvm::Attribute::get(mod.getContext(), llvm::Attribute::NonNull));

        // 将完整的属性列表应用到函数上
        F->setAttributes(AttrList);
        F->addFnAttr(llvm::Attribute::get(mod.getContext(), llvm::Attribute::AlwaysInline));
        auto EntryBB = llvm::BasicBlock::Create(Context, "entry", F);
        auto BB_if_then = llvm::BasicBlock::Create(Context, "if.then", F);
        auto BB_while_preheader = llvm::BasicBlock::Create(Context, "while.cond.preheader", F);
        auto BB_while_cond = llvm::BasicBlock::Create(Context, "while.cond", F);
        auto BB_while_end = llvm::BasicBlock::Create(Context, "while.end", F);
        llvm::IRBuilder<> IRB(Context);
        IRB.SetInsertPoint(EntryBB);
        auto CmpXchg = IRB.CreateAtomicCmpXchg(
            FlagVar,                              // ptr
            llvm::ConstantInt::get(Int32Ty, 0),      // cmp
            llvm::ConstantInt::get(Int32Ty, 2),      // new
            llvm::MaybeAlign(4),                       // align
            llvm::AtomicOrdering::SequentiallyConsistent, // success ordering
            llvm::AtomicOrdering::SequentiallyConsistent  // failure ordering
        );
        // 给指令命名，使其在IR中更可读
        CmpXchg->setName("cmpxchg.res");

        auto SuccessFlag = IRB.CreateExtractValue(CmpXchg, 1, "tobool.not");
        IRB.CreateCondBr(SuccessFlag, BB_if_then, BB_while_preheader);

         // --- 5. 填充 if.then 块 ---
        IRB.SetInsertPoint(BB_if_then);
        // GVar_t 是 [4 x i8]* 类型，我们需要 i8* 类型来存储
        // 用 GEP 获取指向数组第一个元素的指针
        auto t_ptr_base = IRB.CreateBitCast(StoreVar, Int32PtrTy, "t_ptr_base");
        
        for(size_t idx = 0; idx < vec_i32.size(); ++idx){
            auto t_ptr_i = IRB.CreateGEP(Int32Ty, t_ptr_base, IRB.getInt64(idx));
            // IRB.CreateStore(IRB.getInt32(vec_i32[idx]), t_ptr_i)->setAlignment(llvm::Align(4));
            
            IRB.CreateStore(make_n(Context, IRB, vec_i32[idx]), t_ptr_i)->setAlignment(llvm::Align(4));
        }
        IRB.CreateStore(IRB.getInt32(1), FlagVar, /*isVolatile=*/true);
        
        IRB.CreateBr(BB_while_preheader);

        // --- 6. 填充 while.cond.preheader 块 ---
        IRB.SetInsertPoint(BB_while_preheader);
        IRB.CreateBr(BB_while_cond);

        // --- 7. 填充 while.cond 块 ---
        IRB.SetInsertPoint(BB_while_cond);
        auto LoadV = IRB.CreateLoad(Int32Ty, FlagVar, /*isVolatile=*/true, "load.v");
        LoadV->setAlignment(llvm::Align(4));

        auto Cmp = IRB.CreateICmpEQ(LoadV, llvm::ConstantInt::get(Int32Ty, 1), "cmp.not");
        IRB.CreateCondBr(Cmp, BB_while_end, BB_while_cond);

        // --- 8. 填充 while.end 块 ---
        IRB.SetInsertPoint(BB_while_end);
        // The return type is i8*, so we return a pointer to the start of the global array.
        auto RetVal = IRB.CreateBitCast(StoreVar, Int8PtrTy, "retval.ptr");
        IRB.CreateRet(RetVal);
        
        funcs.insert(F);
        return F;
    }
    llvm::Value* UcdPass::insertLazyInit(llvm::Module& mod, llvm::Function& func, llvm::GlobalVariable* g){
        auto Init = g->getInitializer();
        if (auto CDS = llvm::dyn_cast<llvm::ConstantDataSequential>(Init)){
            if (CDS->isCString()){
                auto s = CDS->getAsCString();
                return insertLazyInitCString(mod, func, s);
            }
        }
        return nullptr;
    }

    bool UcdPass::fini(){
        bool changed = false;
        #if 0
        for(auto g: targets){
            if (g->use_empty()){
                llvm::outs() << "[=] [ERASE] " << g->getName() << "\n";
                g->removeFromParent();
                changed += true;
            }
        }
        #endif
        for(auto f: funcs){
            if (f->use_empty()){
                llvm::outs() << "[=] [ERASE] " << f->getName() << "\n";
                f->removeFromParent();
                changed += true;
            }
        }
        return changed;
    }
}