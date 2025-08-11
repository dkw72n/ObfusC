#pragma once
#include "IObfuscationPass.hpp"
#include <vector>

namespace obfusc {
    class WimpPass : public IObfuscationPass {
    public:
        WimpPass();
        ~WimpPass() override;

        bool obfuscate(llvm::Module& mod, llvm::Function& func) override;

        bool runOnModule(llvm::Module& mod);
        // bool fini() override;
        llvm::Module* current;

        llvm::Function* _wimp_lookup;
    };
}