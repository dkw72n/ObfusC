#include "EstrPass.hpp"
#include <llvm/IR/Module.h>
#include <llvm/Transforms/Utils/ModuleUtils.h>

#include <random>

static std::random_device rd; // random device engine, usually based on /dev/random on UNIX-like systems
// initialize Mersennes' twister using rd to generate the seed
static std::mt19937 rng{rd()}; 

// https://github.com/DreamSoule/ollvm17/blob/main/llvm-project/llvm/lib/Passes/Obfuscation/IndirectBranch.cpp


static OBfsRegister<obfusc::EstrPass> sRegEstr("estr");

namespace obfusc {
    EstrPass::EstrPass() {}
    EstrPass::~EstrPass() {}

    bool EstrPass::obfuscate(llvm::Module& mod, llvm::Function& func){
        llvm::outs() << "[ESTR]:\n";
        for(auto& BB: func){
            for(auto& I: BB){
                for(auto& Op: I.operands()){
                    auto *G = llvm::dyn_cast<llvm::GlobalVariable>(Op->stripPointerCasts());
                    if (!G || !G->hasInitializer())
                        continue;
                    if (!G->isConstant())
                        continue;
                    llvm::outs() << "[0] G:"; G->dump();
                    
                }
            }
        }
        return false;
    }
}