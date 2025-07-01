#pragma once
#include "IObfuscationPass.hpp"
#include <map>
#include <vector>

namespace obfusc {

    static constexpr uint64_t FNV_offset_basis = 0xcbf29ce484222325;
    static constexpr uint64_t FNV_prime = 0x100000001b3;
    struct CalleeMap {
        static constexpr size_t NOT_FOUND = (size_t)-1;

        std::vector<llvm::Function*> idx2func;
        std::map<llvm::Function*, size_t> func2idx;
        std::vector<llvm::CallInst*> callsites;
    public:
        void insert(llvm::CallInst* CI){
            callsites.emplace_back(CI);
            auto f = CI->getCalledFunction();
            
            if (func2idx.find(f) != func2idx.end()){
                return;
            }
            func2idx[f] = idx2func.size();
            
            idx2func.emplace_back(f);
            
        }

        void insert(){

        }


        void shuffle(){
            // for(int i = 0; i < 10; ++i) idx2func.emplace_back(nullptr);
            for(int i = 0; i < idx2func.size(); i++){
                auto x = rng() % (idx2func.size() - i) + i;
                auto y = i;
                if (x != y){
                    std::swap(idx2func[x], idx2func[y]);
                    
                }
                if (idx2func[i]){
                    func2idx[idx2func[i]] = i;
                }
            }
        }
        llvm::Function* getFunc(size_t idx) const{
            if (idx >= 0 && idx < idx2func.size()) return idx2func[idx];
            return nullptr;
        }

        size_t getIdx(llvm::Function* f) const{
            auto it = func2idx.find(f);
            if (it == func2idx.end()){
                return NOT_FOUND;
            }
            return it->second;
        }

        size_t size() const {
            return idx2func.size();
        }

        bool empty() const {
            return size() == 0;
        }

        std::string name(const std::string& prefix) const {
            uint64_t x = FNV_offset_basis;
            for(auto f: idx2func){
                x ^= reinterpret_cast<uint64_t>(f);
                x *= FNV_prime;
            }
            return prefix + std::to_string(x);
        }


    };

    class IcallPass : public IObfuscationPass {
    public:
        IcallPass();
        ~IcallPass() override;

        bool obfuscate(llvm::Module& mod, llvm::Function& func) override;

    private:
        void collectCallables(llvm::Module& mod);
        CalleeMap M;
        llvm::GlobalVariable* dispatchTable;
        bool touched;
    };
}