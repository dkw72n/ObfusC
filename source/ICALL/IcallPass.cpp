#include "IcallPass.hpp"
#include <llvm/IR/Module.h>
#include <llvm/IR/AbstractCallSite.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>

#include <random>
#include <vector>
#include <map>

static std::random_device rd; // random device engine, usually based on /dev/random on UNIX-like systems
// initialize Mersennes' twister using rd to generate the seed
static std::mt19937 rng{rd()}; 

// https://github.com/DreamSoule/ollvm17/blob/main/llvm-project/llvm/lib/Passes/Obfuscation/IndirectCall.cpp

static constexpr uint64_t FNV_offset_basis = 0xcbf29ce484222325;
static constexpr uint64_t FNV_prime = 0x100000001b3;
struct CalleeMap {
    static constexpr size_t NOT_FOUND = (size_t)-1;

    std::vector<llvm::Function*> idx2func;
    std::map<llvm::Function*, size_t> func2idx;
    std::vector<llvm::CallInst*> callsites;
public:
    void insert(llvm::CallInst* CI){
        callsites.emplace_back(CI);
        auto f = CI->getCalledFunction();
        
        if (func2idx.find(f) != func2idx.end()){
            return;
        }
        func2idx[f] = idx2func.size();
        
        idx2func.emplace_back(f);
        
    }

    void insert(){

    }


    void shuffle(){
        for(int i = 0; i < 10; ++i) idx2func.emplace_back(nullptr);
        for(int i = 0; i < idx2func.size(); i++){
            auto x = rng() % (idx2func.size() - i) + i;
            auto y = i;
            if (x != y){
                std::swap(idx2func[x], idx2func[y]);
                
            }
            if (idx2func[i]){
                func2idx[idx2func[i]] = i;
            }
        }
    }
    llvm::Function* getFunc(size_t idx) const{
        if (idx >= 0 && idx < idx2func.size()) return idx2func[idx];
        return nullptr;
    }

    size_t getIdx(llvm::Function* f) const{
        auto it = func2idx.find(f);
        if (it == func2idx.end()){
            return NOT_FOUND;
        }
        return it->second;
    }

    size_t size() const {
        return idx2func.size();
    }

    bool empty() const {
        return size() == 0;
    }

    std::string name(const std::string& prefix) const {
        uint64_t x = FNV_offset_basis;
        for(auto f: idx2func){
            x ^= reinterpret_cast<uint64_t>(f);
            x *= FNV_prime;
        }
        return prefix + std::to_string(x);
    }


};

static void number_callees(llvm::Function& F, CalleeMap& M){
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


static llvm::GlobalVariable* make_function_list(llvm::Module& M, CalleeMap& CM){
    std::string GVName = CM.name("icall_gv_");
    llvm::GlobalVariable *GV = M.getNamedGlobal(GVName);
    llvm::outs() << "GVName: " << GVName << "\n";
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
    GV = new llvm::GlobalVariable(M, ATy, false, llvm::GlobalValue::LinkageTypes::PrivateLinkage, CArr, GVName);
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
    if (N < 0) return IRB.CreateNeg(
        MakeN(Context, IRB, Value, -N)
    );
    auto S = N % 2 ? MakeOne(Context, IRB, Value): MakeZero(Context, IRB, Value);
    auto X = MakeN(Context, IRB, Value, N / 2);
    auto Y = IRB.CreateAdd(X, X);
    if (N % 2){
        return IRB.CreateAdd(Y, MakeOne(Context, IRB, Value));
    } 
    return Y;
#else
    return llvm::ConstantInt::get(Int32Ty, N);
#endif
}

namespace obfusc {
    IcallPass::IcallPass() {}
    IcallPass::~IcallPass() {}

    bool IcallPass::obfuscate(llvm::Module& mod, llvm::Function& func){
        CalleeMap CM;
        llvm::outs() << "[-] in icall\n";
        auto& ctx = func.getContext();
        number_callees(func, CM);
        if (CM.empty())
            return false;
        llvm::GlobalVariable* ptrs = make_function_list(mod, CM);
        auto Int8PtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(mod.getContext()));
        auto Int32Ty = llvm::Type::getInt32Ty(mod.getContext());
        auto Int64Ty = llvm::Type::getInt64Ty(mod.getContext());
        for(auto CI: CM.callsites){
            llvm::IRBuilder<> IRB(CI);
            auto AOR = IRB.CreatePtrToInt(
                IRB.CreateIntrinsic(Int8PtrTy, llvm::Intrinsic::addressofreturnaddress, {}, {}),
                Int32Ty
            );
            int32_t Shift = rng() % 2048 + 2048;
            CI->setCalledOperand(IRB.CreateBitCast(
                IRB.CreateLoad(
                    Int8PtrTy,
                    IRB.CreateGEP(
                        Int8PtrTy,
                        IRB.CreateGEP(
                            Int8PtrTy,
                            ptrs,
                            llvm::ConstantInt::get(Int32Ty, Shift)
                        ),
                        MakeN(mod.getContext(), IRB, AOR, -Shift + CM.getIdx(CI->getCalledFunction()))
                    )
                ),
                CI->getFunctionType()->getPointerTo()
            ));
        }
        return true;
    }
}