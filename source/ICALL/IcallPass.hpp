#pragma once
#include "IObfuscationPass.hpp"
#include <map>
#include <vector>

namespace obfusc {
    class IcallPass : public IObfuscationPass {
    public:
        IcallPass();
        ~IcallPass() override;

        bool obfuscate(llvm::Module& mod, llvm::Function& func) override;

    };
}