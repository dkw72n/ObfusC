#pragma once
#include "IObfuscationPass.hpp"


namespace obfusc {
    class AtdPass : public IObfuscationPass {
    public:
        AtdPass();
        ~AtdPass() override;

        bool obfuscate(llvm::Module& mod, llvm::Function& func) override;

    private:
        void init(llvm::Module& mod);
        llvm::InlineAsm* _fake_ret;
        llvm::Module* M;
    };
}