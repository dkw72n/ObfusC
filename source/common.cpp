#include "IObfuscationPass.hpp"
#include "common.hpp"

llvm::Value* MakeOneT(llvm::LLVMContext& Context, llvm::IRBuilder<>& IRB, llvm::Value* Value, llvm::IntegerType* T){
   auto One = llvm::ConstantInt::get(T, 1);
    auto Two = llvm::ConstantInt::get(T, 2);
    auto Three = llvm::ConstantInt::get(T, 3);
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

llvm::Value* MakeZeroT(llvm::LLVMContext& Context, llvm::IRBuilder<>& IRB, llvm::Value* Value, llvm::IntegerType* T){
    auto Three = llvm::ConstantInt::get(T, 3);
    auto One = llvm::ConstantInt::get(T, 1);
    auto X = IRB.CreateURem(Value, Three);
    switch (rng() % 2){
        case 0:
            return IRB.CreateAnd(IRB.CreateLShr(X, One), X);
        case 1:
            return IRB.CreateNot(IRB.CreateNeg(MakeOneT(Context, IRB, Value, T)));
    }
    // return IRB.CreateLShr(IRB.CreateURem(Value, llvm::ConstantInt::get(Int32Ty, 3)), llvm::ConstantInt::get(Int32Ty, 3));
    return IRB.CreateXor(Value, Value);
    // return IRB.CreateSub(IRB.CreateURem(IRB.CreateMul(X,X), IRB.CreateAdd(X, One)), One);
}
template<typename T>
static llvm::IntegerType* GetIntT(llvm::LLVMContext& Context){
    if constexpr (std::is_same_v<T, int32_t>()){
        return llvm::Type::getInt32Ty(Context);
    }
    if constexpr (std::is_same_v<T, int64_t>()){
        return llvm::Type::getInt64Ty(Context);
    }
    static_assert(false, "unknown type");
}

#define PLAN_B 1
llvm::Value* MakeN64(llvm::LLVMContext& Context, llvm::IRBuilder<>& IRB, llvm::Value* Value, int64_t N){
    static int xxx = 0;
    auto T = llvm::dyn_cast<llvm::IntegerType>(Value->getType());
    auto One = llvm::ConstantInt::get(T, 1);
    if (N == 0) return MakeZeroT(Context, IRB, Value, T);
    if (N == 1) return MakeOneT(Context, IRB, Value, T); 
    if (N < 0) return IRB.CreateNeg(
        MakeN64(Context, IRB, Value, -N)
    );

    // llvm::outs() << "MakeN " << N << "\n";
    auto v = Value;
    v = IRB.CreateAdd(v, llvm::ConstantInt::get(T, rng()));
    auto scheme = rng() % 2;
    if (scheme == 0){
        int64_t Sq = floor(sqrt(N));
        
        auto d = Sq ? (rng() % Sq) / 10 : 0;
        Sq -= d;
        if (Sq > 1) {
            auto SqV = MakeN64(Context, IRB, v, Sq);
            return IRB.CreateAdd(IRB.CreateMul(SqV, SqV), llvm::ConstantInt::get(T, N-Sq*Sq));
        }
        else if (Sq == 1){
            return IRB.CreateAdd(MakeOneT(Context, IRB, v, T), llvm::ConstantInt::get(T, N-1));
        }
        else {
            abort();
        }
    } else {
        int64_t bits = 0;
        auto n = N;
        while(n){
            bits++;
            n >>= 1;
        }
        auto sh = bits/2;
        auto q1 = N >> sh;
        auto q2 = N & (((int64_t)1 << sh) - 1);
        
        switch(rng() % 3){
        case 0:
            return IRB.CreateAdd(IRB.CreateShl(MakeN64(Context, IRB, v, q1), llvm::ConstantInt::get(T, sh)), llvm::ConstantInt::get(T, q2));
        case 1:
            return IRB.CreateAdd(IRB.CreateShl(llvm::ConstantInt::get(T, q1), MakeN64(Context, IRB, v, sh)), llvm::ConstantInt::get(T, q2));
        default:
            return IRB.CreateAdd(llvm::ConstantInt::get(T, q1 << sh), MakeN64(Context, IRB, v, q2));
        }
    }

/*
    auto q = N;
    auto r = N % 2;
    
    if (r){
        return IRB.CreateAdd(MakeOne64(Context, IRB, v), MakeN64(Context, IRB, v, N - 1));
    }
    int64_t n = 0;
    while(q % 2 == 0) { n++; q /= 2;}
    if (n < N){
        // if (xxx++ == 0) v = IRB.CreateAdd(Value, llvm::ConstantInt::get(Int64Ty, rng()));
        return IRB.CreateLShr(MakeN64(Context, IRB, v, q), MakeN64(Context, IRB, v, n));
    } else {
        return IRB.CreateLShr(MakeN64(Context, IRB, v, q), llvm::ConstantInt::get(Int64Ty, n));
    }
*/
}