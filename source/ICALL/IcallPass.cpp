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
                llvm::outs() << "Found CallInst:" << CI << "\n";
                auto Callee = CI->getCalledFunction();
                if (!Callee) {
                    continue;
                }

                if (Callee->isIntrinsic()){
                    continue;
                }

                M.insert(CI);
                llvm::outs() << "  calling (" << Callee << ")" << Callee->getName() << "\n";
            }
        }
    }
}


static llvm::GlobalVariable* make_function_list(llvm::Module& M, CalleeMap& CM){
    std::string GVName = CM.name("icall_gv_");
    llvm::GlobalVariable *GV = M.getNamedGlobal(GVName);
    llvm::outs() << "GVName: " << GVName << "\n";
    if (GV) return GV;
    std::vector<llvm::Constant *> Elements;
    auto Int8PtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(M.getContext()));
    for (size_t i = 0; i < CM.size(); ++i){
        llvm::Constant *CE = llvm::ConstantExpr::getBitCast(CM.getFunc(i), Int8PtrTy);
        // CE = llvm::ConstantExpr::getGetElementPtr(Int8PtrTy, CE, EncKey);
        Elements.push_back(CE);
    }
    llvm::ArrayType *ATy = llvm::ArrayType::get(Int8PtrTy, CM.size());
    auto CArr = llvm::ConstantArray::get(ATy, llvm::ArrayRef<llvm::Constant *>(Elements));
    GV = new llvm::GlobalVariable(M, ATy, false, llvm::GlobalValue::LinkageTypes::PrivateLinkage, CArr, GVName);
    llvm::appendToCompilerUsed(M, {GV});
    return GV;
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
        for(auto CI: CM.callsites){
            llvm::IRBuilder<> IRB(CI);
            CI->setCalledOperand(IRB.CreateBitCast(
                IRB.CreateLoad(
                    Int8PtrTy,
                    IRB.CreateGEP(
                        Int8PtrTy,
                        ptrs,
                        llvm::ConstantInt::get(Int32Ty, CM.getIdx(CI->getCalledFunction()))
                    )
                ),
                CI->getFunctionType()->getPointerTo()
            ));
        }
        return true;
    }
}