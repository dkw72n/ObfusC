#pragma once
#include "IObfuscationPass.hpp"
#include <map>
#include <vector>

namespace obfusc {
    class IbrPass : public IObfuscationPass {
    public:
        IbrPass();
        ~IbrPass() override;

        bool obfuscate(llvm::Module& mod, llvm::Function& func) override;

    private:
        std::map<llvm::BasicBlock *, unsigned> BBNumbering;
        std::vector<llvm::BasicBlock *> BBTargets; // all conditional branch targets

        void number_basic_blocks(llvm::Function& func);
        llvm::GlobalVariable * get_indirect_targets(llvm::Function &F, llvm::ConstantInt *EncKey);
    };
}