#include "SplitPass.hpp"
#include <cstdlib>
#include <limits>
#include <random>

static std::random_device rd; // random device engine, usually based on /dev/random on UNIX-like systems
// initialize Mersennes' twister using rd to generate the seed
static std::mt19937 rng{rd()}; 

namespace obfusc {
    SplitPass::SplitPass() {}
    SplitPass::~SplitPass() {}

    static constexpr int SplitNum = 3;
    bool SplitPass::obfuscate(llvm::Module& mod, llvm::Function& func) {
        std::vector<llvm::BasicBlock *> origBB;
        // 保存所有基本块 防止分割的同时迭代新的基本块
        for (auto I = func.begin(), IE = func.end(); I != IE; ++I){
            origBB.push_back(&*I);
        }
        // 遍历函数的全部基本块
        for (auto I = origBB.begin(), IE = origBB.end();I != IE; ++I){
            auto curr = *I;
            //outs() << "\033[1;32mSplitNum : " << SplitNum << "\033[0m\n";
            //outs() << "\033[1;32mBasicBlock Size : " << curr->size() << "\033[0m\n";
            int splitN = curr->size() / 5;
            if (splitN < 2){
                continue;
            }
            // 无需分割只有一条指令的基本块
            // 不可分割含有PHI指令基本块
            if (curr->size() < 2 || contains_phi(curr)){
                //outs() << "\033[0;33mThis BasicBlock is lower then two or had PIH Instruction!\033[0m\n";
                continue;
            }
            // 检查splitN和基本块大小 如果传入的分割块数甚至大于等于基本块自身大小 则修改分割数为基本块大小减一
            if ((size_t)splitN >= curr->size()){
                //outs() << "\033[0;33mSplitNum is bigger then currBasicBlock's size\033[0m\n"; // warning
                //outs() << "\033[0;33mSo SplitNum Now is BasicBlock's size -1 : " << (curr->size() - 1) << "\033[0m\n";
                splitN = curr->size() - 1;
            } else {
                //outs() << "\033[1;32msplitNum Now is " << splitN << "\033[0m\n";
            }
            // Generate splits point
            std::vector<int> test;
            for (unsigned i = 1; i < curr->size(); ++i){
                test.push_back(i);
            }
            // Shuffle
            if (test.size() != 1){
                shuffle(test);
                std::sort(test.begin(), test.begin() + splitN);
            }
            // 分割
            auto it = curr->begin();
            auto toSplit = curr;
            int last = 0;
            for (int i = 0; i < splitN; ++i){
                if (toSplit->size() < 2){
                    continue;
                }
                for (int j = 0; j < test[i] - last; ++j){
                    ++it;
                }
                last = test[i];
                toSplit = toSplit->splitBasicBlock(it, toSplit->getName() + ".split");
            }
            // ++Split;
        }
        return true;
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