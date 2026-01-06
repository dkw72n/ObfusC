#include "LscPass.hpp"
#include <format>
// static OBfsRegister<obfusc::LscPass> sRegIcall("lsc");

static std::string make_ptr_string(void* ptr) {
	char t[64];
	sprintf(t, "%p", ptr);
	return t;
}

static std::string make_ptr_string(void* ptr, int addrspace) {
	char t[64];
	sprintf(t, "%p.%d", ptr, addrspace);
	return t;
}

namespace lsc::detail {
	std::optional<std::string> binop2inst(llvm::Instruction::BinaryOps op){
		switch (op){
			case llvm::Instruction::Xor:
				return {"xorl"};
			case llvm::Instruction::Add:
				return {"addl"};
			case llvm::Instruction::And:
				return {"andl"};
			default:
				break;
		}
		return {};
	}

	using inline_asm_t = std::tuple<llvm::FunctionType*, std::string, std::string>;

	std::optional<inline_asm_t> gen_inline_asm(llvm::BinaryOperator* I){
		auto inst = binop2inst(I->getOpcode());
		if (!inst){
			// llvm::outs() << "[!] [BinOp] " << *I << " NOT SUPPORTED\n";
			return {};
		}
		llvm::Type *OpTy = I->getOperand(0)->getType();
		if (I->hasNoSignedWrap() || I->hasNoUnsignedWrap()){
			return {};
		}
		
		if (!OpTy->isIntegerTy(32)){
			return {};
		}
		auto FuncTy = llvm::FunctionType::get(OpTy, {OpTy, OpTy}, false);
		switch (rng() % 7){
			case 0:
				return {{
					FuncTy,
					std::format(R"asm(
						.byte 0x48, 0x8d, 0x05
						0:  
							jmp 1f
						.byte 0x00, 0x00
							jmp 2f
						.byte 0x48, 0x8d, 0x05
						1:
							{} $1, $0
							jmp 3f
						2:
							jmp 0b
						3:
					)asm", *inst), "=r,r,0,~{rax}"
				}};
			case 1:
				return {{
					FuncTy,
					std::format(R"asm(
							jz 1f
							jnz 1f
						.byte 0x48, 0x81, 0xec
						1:
							{} $1, $0
					)asm", *inst), "=r,r,0"
				}};
			default:
				break;
		}
		return {};
	}
}
namespace obfusc {
	static constexpr bool TRACE_CALL = false;
    LscPass::LscPass() {}
    LscPass::~LscPass() {}

    bool LscPass::obfuscate(llvm::Module& mod, llvm::Function& func) {
        if (func.getName().starts_with(".lsc_")) return false;
        int n = 0;
        for (auto& BB : func) {
            for (auto& I : BB) {
                switch (I.getOpcode()) {
				case llvm::Instruction::Load:
					if (rng() % 10 < 1) runOnLoad(mod, dyn_cast<llvm::LoadInst>(&I));
                    n++;
					break;
				case llvm::Instruction::Store:
					if (rng() % 10 < 1) runOnStore(mod, dyn_cast<llvm::StoreInst>(&I));
					n++;
					break;
				case llvm::Instruction::Call:
					if (rng() % 10 < 1) runOnCall(mod, dyn_cast<llvm::CallInst>(&I));
					n++;
					break;
				case llvm::Instruction::Invoke:
					if (rng() % 10 < 1) runOnInvoke(mod, dyn_cast<llvm::InvokeInst>(&I));
					n++;
					break;
				default:
					{
						if (auto BO = dyn_cast<llvm::BinaryOperator>(&I)){
							runOnBinOp(mod, BO);
							break;
						}
					}
					break;
                }
            }
        }
        return n > 0;
    }
	bool LscPass::fini() {
		bool changed = false;
		for(auto I: _insts_to_remove){
			I->eraseFromParent();
			changed |= true;
		}
		_insts_to_remove.clear();
		return changed;
	}
    void LscPass::runOnLoad(llvm::Module& M, llvm::LoadInst* I)
    {
#if 0
            llvm::outs() << "[-]("
                << *I->getAccessType() << ","
                << I->getAlign().value() << ") "
                << *I << "\n";
#endif
		std::string name = ".lsc_load_" + make_ptr_string(I->getAccessType(), I->getPointerAddressSpace());
		auto f = M.getFunction(name);
		if (!f) {
			auto& Context = M.getContext();
			auto VoidFT = llvm::FunctionType::get(llvm::Type::getVoidTy(Context), {}, false);
			auto FuncType = llvm::FunctionType::get(I->getAccessType(), { I->getOperand(0)->getType() }, false);
			auto F = llvm::Function::Create(FuncType, llvm::GlobalValue::PrivateLinkage, name, M);
			F->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Local);
			auto BB = llvm::BasicBlock::Create(Context, "entry", F);
			llvm::IRBuilder<> IRB(BB);
			IRB.CreateRet(
				IRB.CreateAlignedLoad(I->getAccessType(), F->getArg(0), I->getAlign())
			);
			F->addFnAttr(llvm::Attribute::get(Context, llvm::Attribute::NoInline));
			f = F;
		}
		llvm::IRBuilder<> IRB(I);
		I->replaceAllUsesWith(IRB.CreateCall(f, { I->getOperand(0) }));
		_insts_to_remove.insert(I);
    }
    void LscPass::runOnStore(llvm::Module& M, llvm::StoreInst* I){
#if 0
		llvm::outs() << "[-]("
			<< *I->getAccessType() << ","
			<< I->getAlign().value() << ") "
			<< *I << "\n";
#endif
		auto PtrType = I->getPointerOperand()->getType();
		auto ValType = I->getValueOperand()->getType();
		std::string name = ".lsc_store_" + make_ptr_string(PtrType, I->getPointerAddressSpace()) + make_ptr_string(ValType);
		auto f = M.getFunction(name);
		if (!f) {
			auto& Context = M.getContext();
			auto FuncType = llvm::FunctionType::get(llvm::Type::getVoidTy(Context), { PtrType , ValType}, false);
			auto F = llvm::Function::Create(FuncType, llvm::GlobalValue::PrivateLinkage, name, M);
			F->setUnnamedAddr(llvm::GlobalValue::UnnamedAddr::Local);
			auto BB = llvm::BasicBlock::Create(Context, "entry", F);
			llvm::IRBuilder<> IRB(BB);
			IRB.CreateAlignedStore(F->getArg(1), F->getArg(0), I->getAlign());
			IRB.CreateRetVoid();
			F->addFnAttr(llvm::Attribute::get(Context, llvm::Attribute::NoInline));
			f = F;
		}
		llvm::IRBuilder<> IRB(I);
		IRB.CreateCall(f, { I->getPointerOperand(), I->getValueOperand()});
		// I->eraseFromParent();
		_insts_to_remove.insert(I);
    }

	bool LscPass::getNewFunctionAndArgs(llvm::Module& M, llvm::CallBase* I, llvm::Function* &NF, std::vector<llvm::Value*>& args){
		auto F = I->getCalledFunction();
		llvm::Value* Callee = F;
		auto FT = I->getFunctionType();
		auto RT = FT->getReturnType();
		if (FT->isVarArg()) return false;
		auto CC = I->getCallingConv();
		if (CC != llvm::CallingConv::C){
			if (F){
				llvm::outs() << "skip callee (unsupported callconv): " << F->getName() << "\n";
			}
			return false;
		}
		if (F) {
			/*
			if (F->hasFnAttribute(llvm::Attribute::ReturnsTwice)){
				llvm::outs() << "skip callee (return twice): " << F->getName() << "\n";
				return;
			}
			*/
			if (F->isIntrinsic()) return false;
			// if (I->isTailCall()) return;
			if (F->getName().starts_with(".lsc_")) return false;
			// if (F->doesNotReturn()) return;
		}
		else {
			// llvm::outs() << "[!] indirect call\n";
			Callee = I->getCalledOperand();
		}
		if (llvm::dyn_cast<llvm::InlineAsm>(Callee)){
			return false;
		}
		if constexpr (TRACE_CALL){
			llvm::outs() << "[-]: " << *I << "\n";
			llvm::outs() << "  + " << CC << "\n";
			llvm::outs() << "  + " << *FT << "\n";
		}
		std::string name = ".lsc_call_";

		name += make_ptr_string(RT);
		name += "_";
		for (auto& T : FT->params()) {
			name += make_ptr_string(T);
		}
		auto f = M.getFunction(name);
		if (!f) {
			std::vector<llvm::Type*> argts;
			auto& Context = M.getContext();
			argts.push_back(llvm::PointerType::getUnqual(FT));
			for (auto& T : FT->params()) {
				argts.push_back(T);
			}
			auto WFT = llvm::FunctionType::get(RT, argts, false);
			auto WF = llvm::Function::Create(WFT, llvm::GlobalValue::PrivateLinkage, name, M);
			auto BB = llvm::BasicBlock::Create(Context, "entry", WF);
			llvm::IRBuilder<> IRB(BB);
			std::vector<llvm::Value*> argvs;
			
			for (size_t i = 1; i < WF->arg_size(); i++)
			{
				argvs.push_back(WF->getArg(i));
			}
			auto CI = IRB.CreateCall(FT, WF->getArg(0), argvs);
			CI->setTailCall(true);
			if (FT->getReturnType()->isVoidTy()) {
				IRB.CreateRetVoid();
			}
			else {
				IRB.CreateRet(CI);
			}
			WF->addFnAttr(llvm::Attribute::get(Context, llvm::Attribute::NoInline));
			f = WF;
		}
		llvm::IRBuilder<> IRB(I);
		std::vector<llvm::Value*> argvs;
		argvs.push_back(Callee);
		for (int i = 0; i < I->arg_size(); i++) {
			argvs.push_back(I->getArgOperand(i));
		}
		NF = f;
		args = argvs;
		return true;
	}
    void LscPass::runOnCall(llvm::Module& M, llvm::CallBase* I){
		llvm::Function* f = nullptr;
		std::vector<llvm::Value*> argvs;
		if (getNewFunctionAndArgs(M, I, f, argvs)){
			llvm::IRBuilder<> IRB(I);
			auto CI = IRB.CreateCall(f, argvs);
			// I->eraseFromParent();
			if constexpr (TRACE_CALL){
				llvm::outs() << "[=] " << *I << " \n -> \t " << *CI << "\n--------------\n";
			}
			if (I->isTailCall()) CI->setTailCall();
			I->replaceAllUsesWith(CI);
			_insts_to_remove.insert(I);
		}
    }

	void LscPass::runOnInvoke(llvm::Module& M, llvm::CallBase* I){
		llvm::Function* f = nullptr;
		std::vector<llvm::Value*> argvs;
		if (getNewFunctionAndArgs(M, I, f, argvs)){
			llvm::IRBuilder<> IRB(I);
			auto II = llvm::dyn_cast<llvm::InvokeInst>(I);
			auto CI = IRB.CreateInvoke(f, II->getNormalDest(), II->getUnwindDest(), argvs);
			// I->eraseFromParent();
			// if (I->isTailCall()) CI->setTailCall();
			I->replaceAllUsesWith(CI);
			_insts_to_remove.insert(I);
		}
    }

	void LscPass::runOnBinOp(llvm::Module& mod, llvm::BinaryOperator* I){
		
		auto inline_asm = lsc::detail::gen_inline_asm(I);
		if (!inline_asm){
			return;
		}
		
		llvm::IRBuilder<> IRB(I);
		
		auto [FuncTy, Code, Constraints] = *inline_asm;
		llvm::Value *LHS = I->getOperand(0);
    	llvm::Value *RHS = I->getOperand(1);
		auto bo_inline_asm = llvm::InlineAsm::get(FuncTy, Code, Constraints, true, false);
		auto CI = IRB.CreateCall(FuncTy, bo_inline_asm, {LHS, RHS});
		I->replaceAllUsesWith(CI);
		_insts_to_remove.insert(I);
	}
}