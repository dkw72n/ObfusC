#pragma once
#include "IObfuscationPass.hpp"
#include <vector>

namespace obfusc {
    class SplitPass : public IObfuscationPass {
    public:
        SplitPass();
        ~SplitPass() override;

        bool obfuscate(llvm::Module& mod, llvm::Function& func) override;

    private:
        bool contains_phi(llvm::BasicBlock* bb);
        void shuffle(std::vector<int>& v);
    };
}