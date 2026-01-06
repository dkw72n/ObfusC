#pragma once
#include "IObfuscationPass.hpp"
#include <set>
namespace obfusc {
    class LscPass : public IObfuscationPass {
    public:
        LscPass();
        ~LscPass() override;

        bool obfuscate(llvm::Module& mod, llvm::Function& func) override;
        bool fini() override;
    private:

        void runOnLoad(llvm::Module& mod, llvm::LoadInst* I);
        void runOnStore(llvm::Module& mod, llvm::StoreInst* I);
        void runOnCall(llvm::Module& mod, llvm::CallBase* I);
        void runOnInvoke(llvm::Module& mod, llvm::CallBase* I);
        void runOnBinOp(llvm::Module& mod, llvm::BinaryOperator* I);
        bool getNewFunctionAndArgs(llvm::Module& mod, llvm::CallBase* I, llvm::Function* &F, std::vector<llvm::Value*>& args);
        std::set<llvm::Instruction*> _insts_to_remove;
    };
}