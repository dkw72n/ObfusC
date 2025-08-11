#include "IObfuscationPass.hpp"
#include "common.hpp"

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
    static int xxx = 0;
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
    // auto S = N % 2 ? MakeOne(Context, IRB, Value): MakeZero(Context, IRB, Value);
    auto D = rng() % 125 + 2;
    auto X = N / D; 
    auto V = Value;
    if (rng() % 2 == 0){
        V = IRB.CreateAdd(Value, llvm::ConstantInt::get(Int32Ty, rng()));
    }
    switch(rng()%4){
        case 1:
            return IRB.CreateAdd(MakeN(Context, IRB, V, N%D), IRB.CreateMul(llvm::ConstantInt::get(Int32Ty, X), llvm::ConstantInt::get(Int32Ty, D)));
        case 2:
            return IRB.CreateAdd(llvm::ConstantInt::get(Int32Ty, N%D), IRB.CreateMul(MakeN(Context, IRB, V, X), llvm::ConstantInt::get(Int32Ty, D)));
        case 3:
            return IRB.CreateAdd(llvm::ConstantInt::get(Int32Ty, N%D), IRB.CreateMul(llvm::ConstantInt::get(Int32Ty, X), MakeN(Context, IRB, V, D)));
        default:
            break;
    }

    return llvm::ConstantInt::get(Int32Ty, N);
    // return IRB.CreateAdd(llvm::ConstantInt::get(Int32Ty, N%D), IRB.CreateMul(llvm::ConstantInt::get(Int32Ty, X), llvm::ConstantInt::get(Int32Ty, D)));

    //auto Y = IRB.CreateAdd(X, X);
    //if (N % 2){
    //    return IRB.CreateAdd(Y, MakeOne(Context, IRB, Value));
    //} 
    //return Y;
#else
    return llvm::ConstantInt::get(Int32Ty, N);
#endif
}

llvm::Value* MakeOne64(llvm::LLVMContext& Context, llvm::IRBuilder<>& IRB, llvm::Value* Value){
    auto Int64Ty = llvm::Type::getInt64Ty(Context);
    auto One = llvm::ConstantInt::get(Int64Ty, 1);
    auto Two = llvm::ConstantInt::get(Int64Ty, 2);
    auto Three = llvm::ConstantInt::get(Int64Ty, 3);
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

llvm::Value* MakeZero64(llvm::LLVMContext& Context, llvm::IRBuilder<>& IRB, llvm::Value* Value){
    auto Int64Ty = llvm::Type::getInt64Ty(Context);
    auto Three = llvm::ConstantInt::get(Int64Ty, 3);
    auto One = llvm::ConstantInt::get(Int64Ty, 1);
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

llvm::Value* MakeN64(llvm::LLVMContext& Context, llvm::IRBuilder<>& IRB, llvm::Value* Value, int64_t N){
    static int xxx = 0;
    auto Int64Ty = llvm::Type::getInt64Ty(Context);
    auto One = llvm::ConstantInt::get(Int64Ty, 1);
    if (N == 0) return MakeZero64(Context, IRB, Value);
    if (N == 1) return MakeOne64(Context, IRB, Value); 
    if (N < 0) return IRB.CreateNeg(
        MakeN64(Context, IRB, Value, -N)
    );
    auto q = N;
    auto r = N % 2;
    auto v = Value;
    if (r){
        return IRB.CreateAdd(MakeOne64(Context, IRB, v), MakeN64(Context, IRB, v, N - 1));
    }
    int64_t n = 0;
    while(q % 2 == 0) { n++; q /= 2;}
    if (n < N){
        if (xxx++ == 0) v = IRB.CreateAdd(Value, llvm::ConstantInt::get(Int64Ty, rng()));
        return IRB.CreateLShr(MakeN64(Context, IRB, v, q), MakeN64(Context, IRB, v, n));
    } else {
        return IRB.CreateLShr(MakeN64(Context, IRB, v, q), llvm::ConstantInt::get(Int64Ty, n));
    }
}