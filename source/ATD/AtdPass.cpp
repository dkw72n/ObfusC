#include "AtdPass.hpp"
#include <format>

namespace atd::detail {
    std::string rand_byte_hex(){
        return std::format("{:#04x}", rng() & 0xff);
    }
    llvm::InlineAsm* GenFakeRet(llvm::Module& mod){
        auto VoidFT = llvm::FunctionType::get(llvm::Type::getVoidTy(mod.getContext()), false);
        auto triple = mod.getTargetTriple();
        if (triple.find("x86_64") != std::string::npos){
            switch (rng() % 2){
                case 0: {
                    std::string code = R"asm(
                            lea  0f(%rip), %rax
                            push %rax
                            ret
                        .byte )asm";
                    auto length = rng() % 15 + 1;
                    while(length){
                        length--;
                        code += rand_byte_hex();
                        if (length){
                            code += ",";
                        } else {
                            code += "\n";
                        }
                    }
                    code += "0:\n";
                    return llvm::InlineAsm::get(VoidFT, code, "~{rax}", true /*hasSideEffects*/, false);
                }
                case 1: {
                    std::string code = R"asm(
                            call 1f
                            .byte )asm"; 
                    auto length = rng() % 15 + 1;
                    while(length){
                        length--;
                        code += rand_byte_hex();
                        if (length){
                            code += ",";
                        } else {
                            code += "\n";
                        }
                    }
                    code += R"asm(
                            ret
                    1:
                            pop %rax
                    )asm";
                    return llvm::InlineAsm::get(VoidFT, code, "~{rax}", true /*hasSideEffects*/, false);
                }
            }
        }
        return llvm::InlineAsm::get(VoidFT, R"asm(
                    nop
        )asm", "", true /*hasSideEffects*/, false);
    }
}

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
                lea  0f(%rip), %rax
                push %rax
                ret
            .byte 0xc3, 0xc4, 0xc5, 0xc6, 0x19, 0x90, 0x07, 0x24
            0:
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
                auto fake_ret = atd::detail::GenFakeRet(mod);
                IRB.CreateCall(fake_ret->getFunctionType(), fake_ret);
                changed |= true;
                // break;
            }
            
        }
        return changed;
    }
}