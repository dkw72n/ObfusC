#pragma once
#include <llvm/Passes/PassBuilder.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Transforms/Utils/BasicBlockUtils.h>
#include <random>
#include <map>

class IObfuscationPass {
public:
    virtual bool obfuscate(llvm::Module& mod, llvm::Function& func) = 0;
protected:
    IObfuscationPass();
    virtual ~IObfuscationPass() {}

    /* Get a random number that is half the types bit length (e.g. 64 bit for 128 bit values) */
    virtual uint64_t GetRandomNumber(llvm::Type* type);
    std::mt19937_64 m_randGen64;
};

struct ObfsRegistar {
    std::map<std::string, IObfuscationPass*> passes;

    static ObfsRegistar& GetInstance();
};

template<typename T>
struct OBfsRegister {
    OBfsRegister(const char* n) {
        ObfsRegistar::GetInstance().passes[n] = new T();
    }
};