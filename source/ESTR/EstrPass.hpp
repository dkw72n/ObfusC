#pragma once
#include "IObfuscationPass.hpp"
#include <map>
#include <vector>

namespace obfusc {
    class EstrPass : public IObfuscationPass {
    public:
        EstrPass();
        ~EstrPass() override;

        bool obfuscate(llvm::Module& mod, llvm::Function& func) override;
    private:
        bool touched;
        std::map<llvm::GlobalVariable*, std::vector<int32_t>> removing;

        void collectRemovables(llvm::Module& mod);
    };
}