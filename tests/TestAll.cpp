#include <cstdio>
#include <cstdint>

namespace MbaTest {
    void MbaTestAll();
}

namespace CffTest {
    void CffTestAll();
}

namespace iSubTest {
    void iSubTestAll();
}

namespace BcfTest {
    void BcfTestAll();
}

namespace IbrTest {
    void IbrTestAll();
}

namespace SplitTest {
    void SplitTestAll();
}

#ifdef OBFUSC_TEST_BUILD_ALL

int main(int argc, char *argv[]) {
    MbaTest::MbaTestAll();
    CffTest::CffTestAll();
    iSubTest::iSubTestAll();
    BcfTest::BcfTestAll();
    IbrTest::IbrTestAll();
    SplitTest::SplitTestAll();
    return 0;
}

#endif