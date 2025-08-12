#include "SplitPass.hpp"
#include <cstdlib>
#include <limits>

namespace obfusc {
    SplitPass::SplitPass() {}
    SplitPass::~SplitPass() {}

    static constexpr int SplitNum = 3;
    bool SplitPass::obfuscate(llvm::Module& mod, llvm::Function& func) {

        std::vector<llvm::BasicBlock *> origBB;
        for (auto & bb: func){
            if (bb.size() > 20 && !contains_phi(&bb)){
                origBB.emplace_back(&bb);
            }
        }
        int total_splited = 0;
        // 遍历函数的全部基本块
        for (auto bb: origBB){
            auto cur = bb;
            while(cur->size() > 20){
                bool splited = false;
                int n = 20;
                auto it = cur->begin();
                while(it != cur->end()){
                    if (n-- < 0 && it->getOpcode() == llvm::Instruction::Add){
                        cur = cur->splitBasicBlock(it);
                        llvm::outs() << "[split] bb: " << cur << "@" << *it << "\n";
                        splited = true;
                        total_splited++;
                        break;
                    }
                    it++;
                }
                if (!splited) break;
            }
        }
        return total_splited > 0;
    }

    /**
     * @brief 判断基本块是否包含PHI指令
     * 
     * @param BB 
     * @return true 
     * @return false 
     */
    bool SplitPass::contains_phi(llvm::BasicBlock* BB){
        for (auto &I : *BB){
            if (isa<llvm::PHINode>(&I)){
                return true;
            }
        }
        return false;
    }

    /**
     * @brief 辅助分割流程的函数
     * 
     * @param vec 
     */
    void SplitPass::shuffle(std::vector<int>& vec){
        int n = vec.size();
        for (int i = n - 1; i > 0; --i){
            std::swap(vec[i], vec[rng() % (i + 1)]);
        }
    }
}