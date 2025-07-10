#include "AtdPass.hpp"

namespace obfusc {
    
    AtdPass::AtdPass(): M(nullptr), _fake_ret(nullptr){}
    AtdPass::~AtdPass() {}

    void AtdPass::init(llvm::Module& mod){
        /*
        对抗反编译器 https://github.com/AppleReer/Anti-Disassembly-On-Arm64
        */
        auto VoidFT = llvm::FunctionType::get(llvm::Type::getVoidTy(mod.getContext()), false);
        llvm::outs() << "[-] target: " << mod.getContext().getDefaultTargetCPU() << "\n";
        if (mod.getContext().getDefaultTargetCPU() == "x86-64"){
            _fake_ret = llvm::InlineAsm::get(VoidFT, R"asm(
                lea 2(%rip), %rax
                push %rax
                ret
            )asm", "~{rax}", true /*hasSideEffects*/, false);
            llvm::outs() << "[-] created x86-64 _fake_ret: " << *_fake_ret << "\n"; 
        }
        else {
            _fake_ret = llvm::InlineAsm::get(VoidFT, R"asm(
                adr x8,#0xc
                mov x30, x8
                ret
            )asm", "~{x8},~{lr}", true /*hasSideEffects*/, false);
            llvm::outs() << "[-] created aarch64 _fake_ret: " << *_fake_ret << "\n";
        }
        M = &mod;
    }
    bool AtdPass::obfuscate(llvm::Module& mod, llvm::Function& func){
        if (!M) init(mod);
        if (!_fake_ret) return false; /* not supported */
        bool changed = false;
        for (auto& BB: func){
            auto It = BB.getFirstNonPHIOrDbgOrLifetime();
            if (!It) continue;
            if (rng() % 5 <= 1){
                llvm::IRBuilder<> IRB(It);
                IRB.CreateCall(_fake_ret->getFunctionType(), _fake_ret);
                changed |= true;
                llvm::outs() << "[-] FUN " << func.getName() << "\n";
                break;
            }
            
        }
        return changed;
    }
}