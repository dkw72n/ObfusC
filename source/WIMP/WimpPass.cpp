#include "WimpPass.hpp"

#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/StripNonLineTableDebugInfo.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Verifier.h>
#include <llvm/Transforms/IPO/ExtractGV.h>
#include <llvm/Transforms/IPO/StripSymbols.h>
#include <llvm/Transforms/IPO/StripDeadPrototypes.h>
#include <llvm/IRPrinter/IRPrintingPasses.h>


using DWORD = uint32_t;
static DWORD toLower(DWORD ch)
{
    if (ch >= 'A' && ch <= 'Z'){
        return ch - 'A' + 'a';
    }
	return ch;
}

static constexpr uint64_t DJB2_INIT = 5381;
static uint64_t djb2(const char* s, bool caseless = false, uint64_t cur = DJB2_INIT, const char* e = nullptr){
    uint64_t hash = cur;
	DWORD val;
    while(e ? s != e : s[0]){
        val = (DWORD)s[0];
        if (caseless) val = toLower(val);
        hash = ((hash << 5) + hash) + val;
        s++;
    }
    return hash;
}

static std::map<std::string, std::set<std::string>> dllmaps = {
    {"kernel32.dll", {"CloseHandle", "OpenProcess", "Sleep", "GetLastError"}}
};
static bool checkKnownImport(std::string func, std::string& mod){
    for(auto& p: dllmaps){
        if (p.second.contains(func)){
            mod = p.first;
            return true;
        }
    }
    return false;
}
// windows iat 加密
namespace obfusc {
    WimpPass::WimpPass(): current(nullptr){

    }
    WimpPass::~WimpPass(){

    }


    bool WimpPass::obfuscate(llvm::Module& mod, llvm::Function& func){
        if (&mod != current){
            current = &mod;
            return runOnModule(mod);
        }
        if (!(func.isDeclaration() && func.hasDLLImportStorageClass())){
            return false;
        }
        
        llvm::outs() << func.getName() << " :: " << func << "\n";
        for(auto u: func.users()){
            llvm::outs() << "\t" << *u << "\n";
        }

        std::string modName;
        if (checkKnownImport(func.getName().str(), modName)){
            auto Int8PtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(mod.getContext()));
            auto Int64Ty = llvm::Type::getInt64Ty(mod.getContext());
            std::string strFunc = func.getName().str();
            auto& ctx = mod.getContext();
            llvm::Type* TyI64 = llvm::Type::getInt64Ty(mod.getContext());
            auto funcType = func.getFunctionType();
            auto funcTypeTo = funcType->getPointerTo();
            auto modHash = djb2(modName.c_str(), true);
            auto funcHash = djb2(strFunc.c_str());
            for(auto* U: func.users()){
                if (auto CI = llvm::dyn_cast<llvm::CallInst>(U)) {
                    llvm::IRBuilder<> IRB(CI);
                    auto AOR = IRB.CreatePtrToInt(
                                IRB.CreateIntrinsic(Int8PtrTy, llvm::Intrinsic::addressofreturnaddress, {}, {}),
                                Int64Ty
                            );
                    auto modHashV = MakeN64(mod.getContext(), IRB, AOR, modHash);
                    auto funcHashV = MakeN64(mod.getContext(), IRB, AOR, funcHash);
                    auto rawAddr = IRB.CreateCall(_wimp_lookup, {modHashV, funcHashV});
                    auto funcPtr = IRB.CreateBitCast(
                        rawAddr,       // 原始指针 (i8*)
                        funcTypeTo       // 目标类型 (void ()*)
                    );
                    CI->setCalledFunction(funcType, funcPtr);
                    return true;
                } else if (auto II = llvm::dyn_cast<llvm::InvokeInst>(U)) {
                    llvm::IRBuilder<> IRB(II);
                    auto AOR = IRB.CreatePtrToInt(
                                IRB.CreateIntrinsic(Int8PtrTy, llvm::Intrinsic::addressofreturnaddress, {}, {}),
                                Int64Ty
                            );
                    auto modHashV = MakeN64(mod.getContext(), IRB, AOR, modHash);
                    auto funcHashV = MakeN64(mod.getContext(), IRB, AOR, funcHash);
                    auto rawAddr = IRB.CreateCall(_wimp_lookup, {modHashV, funcHashV});
                    auto funcPtr = IRB.CreateBitCast(
                        rawAddr,       // 原始指针 (i8*)
                        funcTypeTo       // 目标类型 (void ()*)
                    );
                    II->setCalledFunction(funcType, funcPtr);
                    return true;
                }
            }
#if 0
            auto newFunc = llvm::Function::Create(funcType, llvm::GlobalValue::PrivateLinkage, "", mod);
            func.replaceAllUsesWith(newFunc);

            auto BB = llvm::BasicBlock::Create(ctx, "entry", newFunc);
            llvm::IRBuilder<> IRB(BB);
			std::vector<llvm::Value*> argvs;
            for (size_t i = 0; i < newFunc->arg_size(); i++)
			{
				argvs.push_back(newFunc->getArg(i));
			}
			auto CI = IRB.CreateCall(funcType, &func, argvs);
			CI->setTailCall(true);
			if (funcType->getReturnType()->isVoidTy()) {
				IRB.CreateRetVoid();
			}
			else {
				IRB.CreateRet(CI);
			}
            return true;
#endif
            return false;
        }
        return false;
    }

    bool WimpPass::runOnModule(llvm::Module& mod){
        auto TyPtr = llvm::PointerType::getUnqual(mod.getContext());
        llvm::Type* TyI64 = llvm::Type::getInt64Ty(mod.getContext());
        _wimp_lookup = mod.getFunction("_wimp_lookup");
        if (!_wimp_lookup){
            llvm::FunctionType* FT = llvm::FunctionType::get(TyPtr, {TyI64, TyI64}, false);
            _wimp_lookup = llvm::Function::Create(FT, llvm::GlobalValue::ExternalLinkage, "_wimp_lookup", mod);
        }
        #if 0
        _wimp_mod = mod.getFunction("_wimp_mod");
        if (!_wimp_mod){
            llvm::FunctionType* FT = llvm::FunctionType::get(TyPtr, {TyI64, TyI64}, false);
            _wimp_mod = llvm::Function::Create(FT, llvm::GlobalValue::ExternalLinkage, "_wimp_lookup", mod);
        }
        _wimp_func = mod.getFunction("_wimp_func");
        #endif
        llvm::outs() << " ptr_wimp_lookup: " << _wimp_lookup << "\n";
        return false;
    }
    
}