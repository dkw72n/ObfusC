#include "MbaPass.hpp"
#include <cstdlib>
#include <limits>

#include <random>

static std::random_device rd; // random device engine, usually based on /dev/random on UNIX-like systems
// initialize Mersennes' twister using rd to generate the seed
static std::mt19937 rng{rd()}; 

static OBfsRegister<obfusc::MbaPass> sRegIcall("mba");
namespace obfusc {
    MbaPass::MbaPass() {}
    MbaPass::~MbaPass() {}

    bool MbaPass::obfuscate(llvm::Module& mod, llvm::Function& func) {
        bool changed = false;

        for(int i = 0; i < s_RecursiveAmount; ++i){
            changed |= single_pass(mod, func);
        }
        return changed;
    }

    uint64_t MbaPass::GetSignedMax(llvm::Type* type) {
        switch (type->getIntegerBitWidth()) {
        case 8:
            return CHAR_MAX;
        case 16:
            return SHRT_MAX;
        case 32:
            return INT_MAX;
        case 64:
            return LLONG_MAX;
        default:
            return CHAR_MAX;
        }
    }

    bool MbaPass::single_pass(llvm::Module& mod, llvm::Function& func){
        bool changed = false;
        for (auto& block : func) {
            uint32_t cnt = 0;
            for (auto instruction = block.begin(); instruction != block.end(); instruction++) {
                auto dice = rng() % 30;
                llvm::BinaryOperator* binOp = llvm::dyn_cast<llvm::BinaryOperator>(instruction); 
                if (!binOp) //Is instruction a binary op
                    continue;

                auto Opcode = binOp->getOpcode();

                if (!binOp->getType()->isIntegerTy()){
                    continue;
                }

                if (binOp->getOperand(0)->getType() != binOp->getOperand(1)->getType()){
                    continue;
                }

                //llvm::outs() << Opcode << "\n";
                //llvm::outs() << "\tt[0]:" << binOp->getOperand(0)->getType() << "\n";
                //llvm::outs() << "\tt[1]:" << binOp->getOperand(1)->getType() << "\n";
                if (Opcode == llvm::Instruction::Add){
                    if (dice < 5){
                        llvm::IRBuilder irBuilder(binOp);
                        // a + b == (a ^ b) + 2 * (a & b)
                        //       == (a & b) + (a | b)
                        auto *NewValue = irBuilder.CreateAdd(
                            irBuilder.CreateXor(binOp->getOperand(0),
                                            binOp->getOperand(1)),
                            irBuilder.CreateMul(
                                llvm::ConstantInt::get(binOp->getType(), 2),
                                irBuilder.CreateAnd(
                                    binOp->getOperand(0),
                                    binOp->getOperand(1)
                                )
                            )
                        );
                        llvm::ReplaceInstWithValue(instruction, NewValue);
                        changed = true;
                        cnt++;
                    }
                    else if (dice < 10){
                        llvm::IRBuilder irBuilder(binOp);
                        auto *NewValue = irBuilder.CreateAdd(
                            irBuilder.CreateAnd(
                                binOp->getOperand(0),
                                binOp->getOperand(1)
                            ),
                            irBuilder.CreateOr(
                                binOp->getOperand(0),
                                binOp->getOperand(1)
                            )
                        );
                        llvm::ReplaceInstWithValue(instruction, NewValue);
                        changed = true;
                        cnt++;
                    }
                }
                else if (Opcode == llvm::Instruction::Xor){
                    if (dice < 20){
                        llvm::IRBuilder irBuilder(binOp);
                        // a ^ b == (a | b) - (a & b)
                        auto *NewValue = irBuilder.CreateSub(
                            irBuilder.CreateOr(
                                binOp->getOperand(0), 
                                binOp->getOperand(1)),
                            irBuilder.CreateAnd(
                                binOp->getOperand(0),
                                binOp->getOperand(1))
                        );
                        llvm::ReplaceInstWithValue(instruction, NewValue);
                        changed = true;
                        cnt++;
                    }
                }
                else if (Opcode == llvm::Instruction::Sub){
                    if (dice < 10){
                        llvm::IRBuilder irBuilder(binOp);
                        // a - b == (a ^ -b) + 2*(a & -b)
                        auto *NewValue = irBuilder.CreateAdd(
                            irBuilder.CreateXor(
                                binOp->getOperand(0), 
                                irBuilder.CreateNeg(binOp->getOperand(1))
                            ),
                            irBuilder.CreateMul(
                                llvm::ConstantInt::get(binOp->getType(), 2),
                                irBuilder.CreateAnd(
                                    binOp->getOperand(0),
                                    irBuilder.CreateNeg(binOp->getOperand(1))
                                )
                            )
                        );
                        llvm::ReplaceInstWithValue(instruction, NewValue);
                        changed = true;
                        cnt++;
                    }
                }
            }
        }
        return changed;
    }
    /* Gen loads and stores to convert initial value to a 128 bit value (e.g. 32 bit to 128 bit) */
    llvm::Value* MbaPass::GenStackAlignmentCode(llvm::IRBuilder<>& irBuilder, llvm::Type* newType, llvm::Value* operand) {
        llvm::Instruction* alloc = irBuilder.CreateAlloca(newType);
        auto constZero = llvm::ConstantInt::get(newType, 0);
        llvm::Instruction* store = irBuilder.CreateStore(constZero, alloc);
        store = irBuilder.CreateStore(operand, alloc);
        return irBuilder.CreateLoad(newType, alloc);
    }

    /* Recursive function to generate the various mixed arithmetic IR. Essentially creates a reversible tree of IR instructions */
    llvm::Value* MbaPass::Substitute(llvm::IRBuilder<>& irBuilder, llvm::Type* type, llvm::Type* origType, llvm::Value* operand, size_t numRecursions) {
        if (numRecursions >= s_RecursiveAmount) {
            // operand->mutateType(origType);
            // return llvm::CastInst::Create(llvm::Instruction::BitCast, operand, origType);
            return irBuilder.CreateTruncOrBitCast(operand, origType);
            // return operand;
        }

        llvm::Instruction* instr = llvm::dyn_cast<llvm::Instruction>(operand);
        if (instr) {
            unsigned int opCode = instr->getOpcode();
            if ((opCode == llvm::Instruction::Add) || (opCode == llvm::Instruction::Sub) || (opCode == llvm::Instruction::UDiv) ||
                (opCode == llvm::Instruction::SDiv) || (opCode == llvm::Instruction::Xor)) 
            {
                operand->mutateType(type);
                //operand = llvm::CastInst::Create(llvm::Instruction::BitCast, operand, type);
            }
        }

        int randType = m_randGen64() % SubstituteType::Max; //Randomly make obfuscation operation
        auto randVal = llvm::ConstantInt::get(type, GetRandomNumber(origType)); //Get random number for each operation

        /* x = (x - randVal) + randVal */
        if (randType == SubstituteType::Add) {
            auto a = Substitute(irBuilder, type, origType, irBuilder.CreateSub(operand, randVal), numRecursions+1);
            return irBuilder.CreateAdd(a, randVal);
        }

        /* x = (x + randVal) - randVal */
        else if (randType == SubstituteType::Subtract) {
            auto a = Substitute(irBuilder, type, origType, irBuilder.CreateAdd(operand, randVal), numRecursions+1);
            return irBuilder.CreateSub(a, randVal);
        }

        /* x = (x * randVal) / randVal */
        else if (randType == SubstituteType::Divide) {
            auto a = Substitute(irBuilder, type, origType, operand, numRecursions+1);
            return irBuilder.CreateSDiv(irBuilder.CreateMul(a, randVal), randVal);
        }

        /* x = ~(~x) */
        else if (randType == SubstituteType::Not) {
            auto signedMax = llvm::ConstantInt::get(type, GetSignedMax(origType)); //Get random number for each operation
            auto a = Substitute(irBuilder, type, origType, irBuilder.CreateXor(operand, signedMax), numRecursions+1);
            return irBuilder.CreateXor(a, signedMax);
        }

        /* x = (x ^ randVal) ^ randVal */
        else if (randType == SubstituteType::Xor) {
            auto a = Substitute(irBuilder, type, origType, irBuilder.CreateXor(operand, randVal), numRecursions+1);
            return irBuilder.CreateXor(a, randVal);
        }

        return operand;
    }
    
}