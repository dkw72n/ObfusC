#include "IcallPass.hpp"
#include <llvm/IR/Module.h>
#include <llvm/IR/AbstractCallSite.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>

#include <random>
#include <vector>
#include <map>


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


llvm::Value* MakeOne(llvm::LLVMContext& Context, llvm::IRBuilder<>& IRB, llvm::Value* Value){
    auto Int32Ty = llvm::Type::getInt32Ty(Context);
    auto One = llvm::ConstantInt::get(Int32Ty, 1);
    auto Two = llvm::ConstantInt::get(Int32Ty, 2);
    auto Three = llvm::ConstantInt::get(Int32Ty, 3);
    auto X = IRB.CreateURem(Value, Three);
    auto Y = IRB.CreateAdd(X, One);
    auto Z = IRB.CreateShl(One, X);
    switch (rng() % 2){
        case 0:
            return IRB.CreateAnd(IRB.CreateOr(IRB.CreateLShr(Y, One), Y), One);
        case 1:
            return IRB.CreateURem(IRB.CreateMul(Z, Z), IRB.CreateAdd(Z, One));
    }
    return One;
}

llvm::Value* MakeZero(llvm::LLVMContext& Context, llvm::IRBuilder<>& IRB, llvm::Value* Value){
    auto Int32Ty = llvm::Type::getInt32Ty(Context);
    auto Three = llvm::ConstantInt::get(Int32Ty, 3);
    auto One = llvm::ConstantInt::get(Int32Ty, 1);
    auto X = IRB.CreateURem(Value, Three);
    switch (rng() % 2){
        case 0:
            return IRB.CreateAnd(IRB.CreateLShr(X, One), X);
        case 1:
            return IRB.CreateNot(IRB.CreateNeg(MakeOne(Context, IRB, Value)));
    }
    // return IRB.CreateLShr(IRB.CreateURem(Value, llvm::ConstantInt::get(Int32Ty, 3)), llvm::ConstantInt::get(Int32Ty, 3));
    return IRB.CreateXor(Value, Value);
    // return IRB.CreateSub(IRB.CreateURem(IRB.CreateMul(X,X), IRB.CreateAdd(X, One)), One);
}

llvm::Value* MakeN(llvm::LLVMContext& Context, llvm::IRBuilder<>& IRB, llvm::Value* Value, int32_t N){
    auto Int32Ty = llvm::Type::getInt32Ty(Context);
#if 1
    if (N == 0) return MakeZero(Context, IRB, Value);
    if (N == 1) return MakeOne(Context, IRB, Value); 
    if (N < 0) return IRB.CreateNeg(
        MakeN(Context, IRB, Value, -N)
    );
    if (N < 0x10){
        switch(rng() % 3){
            case 0:
                return IRB.CreateAdd(llvm::ConstantInt::get(Int32Ty, N/2), MakeN(Context, IRB, Value, N - N/2));
            case 1:
                return IRB.CreateOr(llvm::ConstantInt::get(Int32Ty, N & 0x3), MakeN(Context, IRB, Value, N & 0xc));
            default:
                return IRB.CreateXor(llvm::ConstantInt::get(Int32Ty, N^6), MakeN(Context, IRB, Value, 6));
        }
    }
    auto S = N % 2 ? MakeOne(Context, IRB, Value): MakeZero(Context, IRB, Value);
    auto D = rng() % 11 + 2;
    auto X = MakeN(Context, IRB, Value, N / D);
    return IRB.CreateAdd(llvm::ConstantInt::get(Int32Ty, N%D), IRB.CreateMul(X, llvm::ConstantInt::get(Int32Ty, D)));

    //auto Y = IRB.CreateAdd(X, X);
    //if (N % 2){
    //    return IRB.CreateAdd(Y, MakeOne(Context, IRB, Value));
    //} 
    //return Y;
#else
    return llvm::ConstantInt::get(Int32Ty, N);
#endif
}

static OBfsRegister<obfusc::IcallPass> sRegIcall("icall");

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
                        // llvm::outs() << "  calling (" << Callee << ")" << Callee->getName() << "\n";
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
            CI->setCalledOperand(IRB.CreateBitCast(
                IRB.CreateLoad(
                    Int8PtrTy,
                    IRB.CreateIntToPtr(
                        IRB.CreateAdd(
                            IRB.CreatePtrToInt(
                                llvm::ConstantExpr::getGetElementPtr(
                                    Int8PtrTy,
                                    dispatchTable,
                                    llvm::ConstantInt::get(Int32Ty, Shift)
                                ),
                                IntPtrTy
                            ),
                            IRB.CreateSExt(
                                MakeN(mod.getContext(), IRB, AOR, (-Shift + M.getIdx(CI->getCalledFunction())) * (IntPtrTy->getBitWidth() / 8)),
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
}