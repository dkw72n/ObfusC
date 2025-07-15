#pragma once
#include <llvm/Passes/PassBuilder.h>
#include "IObfuscationPass.hpp"
#include <map>
namespace obfusc {
    void SetOptMode();
    bool IsOptMode();
    struct ObfuscationPassManager : llvm::PassInfoMixin<ObfuscationPassManager> {
        // Takes IR unit to run the pass on Module and the corresponding manager
        llvm::PreservedAnalyses run(llvm::Module& mod, llvm::ModuleAnalysisManager&);

        // If false, pass is skipped for functions decorated with the optnone attribute (e.g. -O0).
        static bool isRequired() { return true; }
    };


    struct ObfuscPassWrapper : llvm::PassInfoMixin<ObfuscationPassManager> {

        ObfuscPassWrapper(IObfuscationPass* p): impl(p) {
            llvm::outs() << "ObfuscPassWrapper: " << p << "\n";
        }
        // Takes IR unit to run the pass on Module and the corresponding manager
        inline llvm::PreservedAnalyses run(llvm::Module& mod, llvm::ModuleAnalysisManager&){
            llvm::outs() <<"[RUN] ON " << mod.getName() << "\n";
            bool changed = false;
            impl->init();
            for(auto& f: mod){
                changed |= impl->obfuscate(mod, f);
            }
            changed |= impl->fini();
            return changed ? llvm::PreservedAnalyses::none() : llvm::PreservedAnalyses::all();
        }

        // If false, pass is skipped for functions decorated with the optnone attribute (e.g. -O0).
        static bool isRequired() { return true; }

    private:
        IObfuscationPass* impl;
    };
}