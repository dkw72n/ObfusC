#include "AtdPass.hpp"
#include <format>

namespace atd::detail {

    template<typename T, std::size_t N>
    const T& select(const T (&Arr)[N]) {
        return Arr[rng() % N];
    }
    void emit_rand_op(std::string& code){
        std::string inst = ".byte ";
        int ops[] = {0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5e, 0x5f, 0xc3};
        inst += std::format("{:#04x}\n", select(ops));
        code += inst;
    }
    void emit_rand_op_imm8(std::string& code){
        std::string inst = ".byte ";
        int ops[] = {0xeb, 0x6a, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76, 0x77, 0x78, 0x79, 0x7a, 0x7b, 0x7c, 0x7d, 0x7e, 0x7f};
        inst += std::format("{:#04x}, {:#04x}\n", select(ops), rng() % 0xff);
        code += inst;
    }
    void emit_rand_op_imm32(std::string& code){
        std::string inst = ".byte ";
        int ops[] = {0xe8, 0xe9, 0x68};
        inst += std::format("{:#04x}, {:#04x}, {:#04x}, {:#04x}, {:#04x}\n", select(ops), 
            rng() % 0xff, rng() % 0xff, rng() % 0xff, rng() % 0xff);
        code += inst;
    }
    void emit_rand_visit_rsp(std::string& code){
        std::string ops[] = {"lea", "mov", "cmp"};
        std::string regs[] = {"rax", "rbx", "rcx", "rdx", "r8", "r9", "rsi", "rdi", "r10", "r11", "r12", "r13", "r14"};
        code += std::format("{} {:#05x}(%rsp), %{}\n", select(ops), rng()%0x7fff, select(regs));
    }
    void emit_garbage(std::string& code){
        code += std::format(".byte {:#04x}, {:#04x}\n", rng() % 0xff, rng() % 0xff);
    }
    void emit_rand_code(std::string& code){
        decltype(&emit_rand_op) handlers[] = {
            &emit_rand_op,
            &emit_rand_op_imm8,
            &emit_rand_op_imm32,
            &emit_rand_visit_rsp,
            &emit_garbage
        };
        select(handlers)(code);
    }
    std::string rand_byte_hex(){
        return std::format("{:#04x}", rng() & 0xff);
    }
    llvm::InlineAsm* GenFakeRet(llvm::Module& mod){
        auto VoidFT = llvm::FunctionType::get(llvm::Type::getVoidTy(mod.getContext()), false);
        auto triple = mod.getTargetTriple();
        if (triple.find("x86_64") != std::string::npos){
            auto dice = rng() % 4;
            // dice = 3;
            switch (dice){
                case 0: {
                    std::string code = R"asm(
                            lea  0f(%rip), %rax
                            push %rax
                            ret
                        )asm";
                    auto length = rng() % 7 + 1;
                    while(length){
                        length--;
                        emit_rand_code(code);
                    }
                    code += "0:\n";
                    return llvm::InlineAsm::get(VoidFT, code, "~{rax}", true /*hasSideEffects*/, false);
                }
                case 1: {
                    std::string code = R"asm(
                            call 1f
                    )asm"; 
                    auto length = rng() % 7 + 1;
                    while(length){
                        length--;
                        emit_rand_code(code);
                    }
                    code += std::format(".byte 0x{:02x}\n", 0x66 + (rng() & 1));
                    code += R"asm(
                    1:
                            pop %rax
                    )asm";
                    return llvm::InlineAsm::get(VoidFT, code, "~{rax}", true /*hasSideEffects*/, false);
                }
                case 2: {
                    auto length = rng() %  7 + 2;
                    std::string code = std::format(".byte 0x66, 0x0f, 0x1f, 0x84, 0xeb, 0x{:02x}, 0xff, 0xff, 0xff, 0xeb, 0xf9\n", length + 5);
                    /*
0:  66 0f 1f 84 eb 05 ff    nop    WORD PTR [rbx+rbp*8-0xfb]
7:  ff ff
9:  eb f9                   jmp    0x4

->

0:  eb 05                   jmp    0x7
2:  ff                      (bad)
3:  ff                      (bad)
4:  ff                      (bad)
5:  eb f9                   jmp    0x0
                    */
                    while(length--){
                        code += std::format(".byte 0x{:02x}\n", rng() & 0x7f);
                        // emit_rand_op(code);
                    }
                    return llvm::InlineAsm::get(VoidFT, code, "", true /*hasSideEffects*/, false);
                }
                case 3:{
                    int bytes[] = {
                        0x89, 0x94, 0x24, 0x88, 0x01, 0x00, 0x00,
                        0x48, 0x8D, 0x84, 0x24, 0x08, 0x02, 0x00, 0x00,
                        0x89, 0xC1,
                        0x48, 0xC1, 0xE8, 0x00
                    };
                    auto disp1 = rng() % 37;
                    auto disp2 = rng() % 37;
                    std::string code = std::format(R"asm(
                        leaq -0x{:x}(%rip), %rax
                        pushq %rax
                        addq $$0x{:x}, (%rsp)
                        ret
                    )asm", disp1 + 7, disp1 + 15 + disp2); // assert(disp1 + 15 + disp2 <= 0x7f)
                    while(disp2){
                        disp2--;
                        code += std::format(".byte {:#04x}\n", select(bytes));
                    }
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
            
            if (It->getOpcode() != llvm::Instruction::Load
             && It->getOpcode() != llvm::Instruction::Add
             && It->getOpcode() != llvm::Instruction::GetElementPtr
             && It->getOpcode() != llvm::Instruction::Store
             && It->getOpcode() != llvm::Instruction::Alloca
            ){
                llvm::outs() << "[-] [ATD] Skip: " << func.getName() << "@" << It->getOpcodeName() << "\n";
                continue;
            }
            if (rng() % 15 < 10){
                llvm::IRBuilder<> IRB(It);
                // llvm::outs() << "[-] [ATD] Inserting: " << func.getName() << "@" << *It << "\n";
                auto fake_ret = atd::detail::GenFakeRet(mod);
                IRB.CreateCall(fake_ret->getFunctionType(), fake_ret);
                changed |= true;
                // break;
            }
            
        }
        return changed;
    }
}