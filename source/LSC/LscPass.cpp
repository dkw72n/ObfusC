#include "LscPass.hpp"

static OBfsRegister<obfusc::LscPass> sRegIcall("lsc");

static std::string make_ptr_string(void* ptr) {
	char t[64];
	sprintf(t, "%p", ptr);
	return t;
}

namespace obfusc {
    LscPass::LscPass() {}
    LscPass::~LscPass() {}

    bool LscPass::obfuscate(llvm::Module& mod, llvm::Function& func) {
        if (func.getName().starts_with(".lsc_")) return false;
        int n = 0;
        for (auto& BB : func) {
            for (auto& I : BB) {
                switch (I.getOpcode()) {
				case llvm::Instruction::Load:
					runOnLoad(mod, dyn_cast<llvm::LoadInst>(&I));
                    n++;
					break;
				case llvm::Instruction::Store:
					runOnStore(mod, dyn_cast<llvm::StoreInst>(&I));
					n++;
					break;
				case llvm::Instruction::Call:
					runOnCall(mod, dyn_cast<llvm::CallInst>(&I));
					n++;
					break;
				default:
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
		std::string name = ".lsc_load_" + make_ptr_string(I->getAccessType());
		auto f = M.getFunction(name);
		if (!f) {
			auto& Context = M.getContext();
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
		std::string name = ".lsc_store_" + make_ptr_string(PtrType) + make_ptr_string(ValType);
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

    void LscPass::runOnCall(llvm::Module& M, llvm::CallInst* I){
        auto F = I->getCalledFunction();
		llvm::Value* Callee = F;
		auto FT = I->getFunctionType();
		auto RT = FT->getReturnType();
		if (FT->isVarArg()) return;
		
		if (F) {
			if (F->isIntrinsic()) return;
			// if (I->isTailCall()) return;
			if (F->getName().starts_with(".lsc_")) return;
			if (F->doesNotReturn()) return;
		}
		else {
			llvm::outs() << "[!] indirect call\n";
			Callee = I->getCalledOperand();
		}

		llvm::outs() << "[-]: " << *I << "\n";
		llvm::outs() << "  + " << *Callee->getType() << "\n";
		llvm::outs() << "  + " << *FT << "\n";
		std::string name = ".lsc_call_";

		name += make_ptr_string(RT);
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
			if (FT->getReturnType()->isVoidTy()) {
				IRB.CreateCall(FT, WF->getArg(0), argvs);
				IRB.CreateRetVoid();
			}
			else {
				IRB.CreateRet(IRB.CreateCall(FT, WF->getArg(0), argvs));
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
		auto CI = IRB.CreateCall(f, argvs);
		// I->eraseFromParent();
		if (I->isTailCall()) CI->setTailCall();
		I->replaceAllUsesWith(CI);
        _insts_to_remove.insert(I);
    }
}