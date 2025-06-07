#include <cstdio>
#include <cstdint>
#include <cassert>

namespace IbrTest {
    
    [[obfusc::ibr]] int IbrTestVal(int numLoops) {
        int ret = 0;
        int x = 0;
        for (int i = 0; i < numLoops; i++) {
            if (i % 2 == 0){
                ret += 2;
                x += 1;
            } else {
                ret++;
            }
        }
        ret -= x;
        return ret;
    }
    
    
    void IbrTestAll() {
        int ret = IbrTestVal(100);
        printf("Ibr Ret: %d\n", ret);
    }
}

#ifndef OBFUSC_TEST_BUILD_ALL

int main(int argc, char *argv[]) {
    IbrTest::IbrTestAll();
    return 0;
}

#endif