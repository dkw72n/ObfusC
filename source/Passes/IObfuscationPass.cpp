
#include "IObfuscationPass.hpp"

static std::random_device rd; // random device engine, usually based on /dev/random on UNIX-like systems
// initialize Mersennes' twister using rd to generate the seed
std::mt19937 rng{rd()}; 

IObfuscationPass::IObfuscationPass() {
    std::random_device rd;  //Get hardware specific random device
    m_randGen64.seed(rd()); //Seed Mersenne Twister random generator  
}

bool IObfuscationPass::init(){ return false; }
bool IObfuscationPass::fini(){ return false; }
/* Get a random number that is half the types bit length (e.g. 64 bit for 128 bit values) */
uint64_t IObfuscationPass::GetRandomNumber(llvm::Type* type) {
    uint64_t modNum = UCHAR_MAX; //UCHAR_MAX covers 8-bit and 16-bit
    if (type->getIntegerBitWidth() == 32) { //32-bit
        modNum = USHRT_MAX;
    } else if (type->getIntegerBitWidth() >= 64) { //64-bit and 128-bit
        modNum = UINT_MAX;
    }

    return m_randGen64()%modNum;
}

ObfsRegistar& ObfsRegistar::GetInstance(){
    static ObfsRegistar* sInstance = NULL;
    if (!sInstance){
        sInstance = new ObfsRegistar();
    }
    return *sInstance;
}