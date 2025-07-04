#pragma once
#include "IObfuscationPass.hpp"
#include <vector>

namespace obfusc {
    class VirtPass : public IObfuscationPass {
    public:
        VirtPass();
        ~VirtPass() override;

        bool obfuscate(llvm::Module& mod, llvm::Function& func) override;
        // bool fini() override;
    
    };
}