#include <cstdio>
#include <cstdint>
#include <cassert>

namespace SplitTest {
    
    [[obfusc::split]] int SplitTestVal(int a, int b) {
        return a+b+20;
    }
    
    
    void SplitTestAll() {
        int ret = SplitTestVal(10, 10);
        printf("Split Ret: %d\n", ret);
    }
}

#ifndef OBFUSC_TEST_BUILD_ALL

int main(int argc, char *argv[]) {
    SplitTest::SplitTestAll();
    return 0;
}

#endif