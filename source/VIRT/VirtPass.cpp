#include "VirtPass.hpp"
#include <llvm/MC/TargetRegistry.h>
#include <llvm/IR/LegacyPassManager.h>
#include <llvm/IR/Module.h>
#include <llvm/Target/TargetMachine.h>
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/Transforms/Utils/StripNonLineTableDebugInfo.h>
#include <llvm/IR/DebugInfo.h>
#include <llvm/IR/Verifier.h>
#include <format>

#define DEBUG_TYPE "virtpass"

static OBfsRegister<obfusc::VirtPass> sRegVirt("virt");

static const char* vm_target = "wasm64";
static const char* vm_triple = "wasm64-unknown-emscripten";

namespace obfusc {
    static constexpr bool log_level = 0;
    VirtPass::VirtPass() {}
    VirtPass::~VirtPass() {}

    bool VirtPass::obfuscate(llvm::Module& mod, llvm::Function& func){
        llvm::outs() << "[-] [VIRT]" << func.getName() << "\n";

        std::string Triple = "wasm32-unknown-unknown";
        std::string Error;
        const llvm::Target *TheTarget = llvm::TargetRegistry::lookupTarget(Triple, Error);

        if (!TheTarget) {
            llvm::errs() << "Error looking up target: " << Error << "\n";
            return false;
        }

        // --- 2. 创建 TargetMachine ---
        auto CPU = "generic";
        auto Features = "";
        llvm::TargetOptions Opts;
        // 对于可重定位代码，PIC_ 是一个好的默认值
        auto TheTargetMachine = std::unique_ptr<llvm::TargetMachine> {
            TheTarget->createTargetMachine(Triple, CPU, Features, Opts, {llvm::Reloc::Model::PIC_})
        };

        if (!TheTargetMachine) {
            llvm::errs() << "Could not create TargetMachine\n";
            return false;
        }
#if 0
        auto copy = new llvm::Module("wasm", mod.getContext());
        llvm::StripDebugInfo(*copy);
        copy->setTargetTriple(vm_triple);
		copy->setDataLayout("e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128");

        // 删除 annotation
        auto G = copy->getNamedGlobal("llvm.global.annotations");
        if (G)
            G->eraseFromParent();

        auto c = copy->getOrInsertFunction("mul_add",
        /*ret type*/                           llvm::Type::getInt32Ty(copy->getContext()),
        /*args*/                               llvm::Type::getInt32Ty(copy->getContext()),
                                               llvm::Type::getInt32Ty(copy->getContext()),
                                               llvm::Type::getInt32Ty(copy->getContext()));

        auto mul_add = llvm::dyn_cast<llvm::Function>(c.getCallee());
        llvm::outs() << "mul_add " << mul_add << "\n";
        mul_add->setCallingConv(llvm::CallingConv::C);

        auto x = mul_add->getArg(0);
        auto y = mul_add->getArg(1);
        auto z = mul_add->getArg(2);

        auto* block = llvm::BasicBlock::Create(copy->getContext(), "entry", mul_add);
        llvm::IRBuilder<> builder(block);

        auto* tmp = builder.CreateBinOp(llvm::Instruction::Mul,
                                   x, y, "tmp");
        auto* tmp2 = builder.CreateBinOp(llvm::Instruction::Add,
                                            tmp, z, "tmp2");

        builder.CreateRet(tmp2);
#else
        auto copy = llvm::CloneModule(mod);
        llvm::StripDebugInfo(*copy);
        copy->setTargetTriple(vm_triple);
		copy->setDataLayout("e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-f80:128-n8:16:32:64-S128");
#endif
        // 删除 attribute
        for(auto& F: *copy){
            F.setComdat(nullptr);
            F.removeFnAttr("target-cpu");
            F.removeFnAttr("target-features");
        }

        copy->dump();
        
        llvm::verifyModule(*copy, &llvm::errs());

        
        // --- 3. 设置输出流 ---
        // 我们将输出捕获到一个字符串中
        llvm::SmallString<4096> CodeString;
        llvm::raw_svector_ostream OS(CodeString);

        // --- 4. 创建并配置代码生成 Pass 管理器 ---
        // 注意: addPassesToEmitFile 使用的是 legacy PassManager
        llvm::legacy::PassManager PM;
        
        // 选择输出文件类型:
        // CGFT_AssemblyFile -> 文本汇编 (.s / .wat)
        // CGFT_ObjectFile   -> 二进制目标文件 (.o / .wasm)
        auto FileType = llvm::CodeGenFileType::ObjectFile;

        if (TheTargetMachine->addPassesToEmitFile(PM, OS, nullptr, FileType)) {
            llvm::errs() << "TargetMachine can't emit a file of this type!\n";
            return false;
        }
        
        // --- 5. 运行代码生成 ---
        llvm::errs() << "--- Generating WASM Assembly for Module containing " << func.getName() << " ---\n";
        PM.run(*copy);
        llvm::errs() << "--- Code Generation Complete ---\n\n";
        if (FileType == llvm::CodeGenFileType::ObjectFile){
            for(auto i = 0; i < CodeString.size(); ++i){
                llvm::errs() << std::format("{:02x}", CodeString[i]) << " ";
                if (!((i + 1)%32)) llvm::errs() <<"\n";
            } 
        }
        else {
            llvm::errs() << CodeString;
        }
        return false;
    }
}