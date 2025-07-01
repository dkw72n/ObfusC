#include "BcfPass.hpp"
#include <llvm/Transforms/Utils/Cloning.h>
#include <llvm/IR/Module.h>

#include <random>

//Heavily based on Obfuscator-LLVM
//https://github.com/obfuscator-llvm/obfuscator/blob/llvm-4.0/lib/Transforms/Obfuscation/BogusControlFlow.cpp

llvm::Value* MakeOne(llvm::LLVMContext& Context, llvm::IRBuilder<>& IRB, llvm::Value* Value);
llvm::Value* MakeZero(llvm::LLVMContext& Context, llvm::IRBuilder<>& IRB, llvm::Value* Value);

static OBfsRegister<obfusc::BcfPass> sRegIcall("bcf");
namespace obfusc {
    BcfPass::BcfPass() {}
    BcfPass::~BcfPass() {}

    bool BcfPass::obfuscate(llvm::Module& mod, llvm::Function& func) {
        bool changed = false;

        std::vector<llvm::BasicBlock*> basicBlocks; //make copy of blocks
        for (auto& block : func) {
            basicBlocks.push_back(&block);
        }

        //For every block copied
        for (llvm::BasicBlock* block : basicBlocks) {
            auto instruction = block->begin();
            size_t loc = 0;
            if (block->getFirstNonPHIOrDbgOrLifetime()) {
                instruction = static_cast<llvm::BasicBlock::iterator>(block->getFirstNonPHIOrDbgOrLifetime()); //skip past PHI instructions
            }
            for(auto it=block->begin(); it != block->end();  it++){
                if (it == instruction){
                    break;
                }
                loc++;
            }

            // llvm::outs() << "block size: " << block->size() << ", loc: " << loc << "\n";
            if (block->size() < loc + 20) continue;
            for(size_t i = 0; i < (block->size() - loc) / 2; ++i) instruction++;
            if (instruction == block->end()) continue;
            
            auto Int8PtrTy = llvm::PointerType::getUnqual(llvm::Type::getInt8Ty(mod.getContext()));
            auto Int32Ty = llvm::Type::getInt32Ty(mod.getContext());

            llvm::BasicBlock* origBlock = block->splitBasicBlock(instruction); //Get original block
            llvm::BasicBlock* changedBlock = MakeBogusBlock(origBlock, func); //Get Bogus/altered version of same block

            //Erase from parents for each block
            // llvm::outs() << "changedBlock:" << changedBlock<< "\n";
            // llvm::outs() << "changedBlock->getTerminator():" << changedBlock->getTerminator() << "\n";
            changedBlock->getTerminator()->eraseFromParent();
            block->getTerminator()->eraseFromParent();

            llvm::Type* int32Type = llvm::Type::getInt32Ty(mod.getContext());

            int32_t n = ((rng() % (123 - 65) + 65) << 24) | ((rng() % (123 - 65) + 65) << 16) | ((rng() % (123 - 65) + 65) << 8) | ((rng() % (123 - 65) + 65) << 0);
            //Create global variables for comparison
            llvm::GlobalVariable* x = new llvm::GlobalVariable(mod, int32Type, true, llvm::GlobalValue::LinkageTypes::InternalLinkage, llvm::ConstantInt::get(int32Type, n));
            // llvm::GlobalVariable* y = new llvm::GlobalVariable(mod, int32Type, false, llvm::GlobalValue::LinkageTypes::InternalLinkage, llvm::ConstantInt::get(int32Type, 3));

            llvm::IRBuilder<> IRB(block);
            auto AOR = IRB.CreatePtrToInt(
                IRB.CreateIntrinsic(Int8PtrTy, llvm::Intrinsic::addressofreturnaddress, {}, {}),
                Int32Ty
            );
            auto Zero = MakeZero(mod.getContext(), IRB, AOR);
            //Load values from global vars
            llvm::LoadInst* opX = new llvm::LoadInst(int32Type, x, "", block);
            // llvm::LoadInst* opY = new llvm::LoadInst(int32Type, y, "", block);

            //Create compare instruction
            auto cmpInstr = IRB.CreateCmp(llvm::CmpInst::Predicate::ICMP_SLT, Zero, opX);

            //Create branches to each block based on cmp instr
            llvm::BranchInst::Create(origBlock, changedBlock, cmpInstr, block);
            llvm::BranchInst::Create(origBlock, changedBlock);

            auto endInstr = origBlock->end();
            endInstr--; //Get actual last instr
            auto endBlock = origBlock->splitBasicBlock(endInstr);
            origBlock->getTerminator()->eraseFromParent(); //remove from parent block

            llvm::IRBuilder<> IRB2(origBlock);
            
            auto One = MakeOne(mod.getContext(), IRB2, AOR);
            
            auto cmpInstr2 = IRB2.CreateCmp(llvm::CmpInst::Predicate::ICMP_SLT, One, opX);
            //Make cmp instruction
            // cmpInstr = new llvm::ICmpInst(origBlock, llvm::CmpInst::Predicate::ICMP_ULT, opX, opY);
            llvm::BranchInst::Create(endBlock, changedBlock, cmpInstr2, origBlock); //Loop back from bogus block to changed block

            changed = true;
        }

        return changed;
    }

    llvm::BasicBlock* BcfPass::MakeBogusBlock(llvm::BasicBlock* originalBlock, llvm::Function& func) {
        llvm::ValueToValueMapTy valMap;
        llvm::BasicBlock* retBlock = llvm::CloneBasicBlock(originalBlock, valMap, "", &func);

        //Start remapping information for each instruction
        auto origInstruction = originalBlock->begin();
        for (auto instruction = retBlock->begin(); instruction != retBlock->end(); instruction++) {
            for (auto op = instruction->op_begin(); op != instruction->op_end(); ++op) {
                llvm::Value* val = llvm::MapValue(*op, valMap); //Map operations of new instrs
                if (val) {
                    *op = val;
                }
            }

            llvm::PHINode* phiNode = llvm::dyn_cast<llvm::PHINode>(instruction);
            if (phiNode) { //Remap PHI nodes 
                for (unsigned int i = 0; i != phiNode->getNumIncomingValues(); ++i) {
                    llvm::Value* val = llvm::MapValue(phiNode->getIncomingBlock(i), valMap);
                    if (val) {
                        phiNode->setIncomingBlock(i, llvm::cast<llvm::BasicBlock>(val));
                    }
                }
            }

            llvm::SmallVector<std::pair<unsigned int, llvm::MDNode*>> metadata;
            instruction->getAllMetadata(metadata);

            //Needed for DWARF symbols
            instruction->setDebugLoc(origInstruction->getDebugLoc());
            origInstruction++;
 
            /* Start altering blocks!! */

            if (instruction->isBinaryOp()) { //Swap operators for binary instrs
                llvm::Value* op0 = instruction->getOperand(0);
                llvm::Value* op1 = instruction->getOperand(1);
                instruction->setOperand(0, op1);
                instruction->setOperand(1, op0);
            }
           #if 0
            //Make Load instrs into Store instrs
            else if (instruction->getOpcode() == llvm::Instruction::Load) {
                llvm::Value* op0 = instruction->getOperand(0);
                llvm::outs() << "*** processing " ;
                instruction->dump();
                if (auto allocIns = llvm::dyn_cast<llvm::AllocaInst>(op0)){
                    llvm::Type* opType = allocIns->getAllocatedType();
                    if (opType->isIntegerTy()){
                        llvm::outs() << (uintptr_t)opType << "\n";
                        opType->dump();
                        auto randVal = llvm::ConstantInt::get(opType, rng()); //Get random number for storing

                        llvm::StoreInst* newOp = new llvm::StoreInst(randVal, op0, true, &*instruction);
                        instruction = instruction->eraseFromParent();
                        llvm::outs() << "*** --> ";
                        newOp->dump();
                    }
                } else {
                    llvm::outs() << "*** --> X\n";
                }
            }
            #endif
            //Make Store instrs into Load instrs
            else if (instruction->getOpcode() == llvm::Instruction::Store) {
                llvm::Value* op0 = instruction->getOperand(0);
                llvm::Value* op1 = instruction->getOperand(1);

                llvm::LoadInst* newOp = new llvm::LoadInst(op0->getType(), op1, "", &*instruction);
                instruction = instruction->eraseFromParent();
            }
            
        }

        return retBlock;
    }
}