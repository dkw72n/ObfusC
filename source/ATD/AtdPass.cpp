#include "AtdPass.hpp"

namespace obfusc {
    
    AtdPass::AtdPass(): M(nullptr), _fake_ret(nullptr){}
    AtdPass::~AtdPass() {}

    void AtdPass::init(llvm::Module& mod){
        /*
        对抗反编译器 https://github.com/AppleReer/Anti-Disassembly-On-Arm64
        */
        auto VoidFT = llvm::FunctionType::get(llvm::Type::getVoidTy(mod.getContext()), false);
        auto triple = mod.getTargetTriple();
        llvm::outs() << "[-] target triple: " << triple << "\n";
        // llvm::outs() << "[-] target: " << mod.getContext().getDefaultTargetCPU() << "\n";
        if (triple.find("x86_64") != std::string::npos){
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
        if (func.getName().starts_with(".")) return false;
        bool changed = false;
        for (auto& BB: func){
            auto It = BB.getFirstNonPHIOrDbgOrLifetime();
            if (!It) continue;
            
            if (It->getOpcode() != llvm::Instruction::Load){
                continue;
            }
            if (rng() % 15 <= 1){
                llvm::IRBuilder<> IRB(It);
                llvm::outs() << "[-] [ATD] Inserting: " << func.getName() << "@" << *It << "\n";
                IRB.CreateCall(_fake_ret->getFunctionType(), _fake_ret);
                changed |= true;
                
                // break;
            }
            
        }
        return changed;
    }
}