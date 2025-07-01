#pragma once
#include "IObfuscationPass.hpp"
#include <vector>

namespace obfusc {
    class UcdPass : public IObfuscationPass {
    public:
        UcdPass();
        ~UcdPass() override;

        bool obfuscate(llvm::Module& mod, llvm::Function& func) override;
        bool fini() override;
    private:
        bool touched;
        void collectRemovables(llvm::Module& mod);
        llvm::Value* insertLazyInit(llvm::Module &mod, llvm::Function& func, llvm::GlobalVariable* g);
        llvm::Value* insertLazyInitCString(llvm::Module& mod, llvm::Function& func, llvm::StringRef s);
        std::set<llvm::GlobalVariable*> targets;
    };
}