#include "gmock/gmock.h"

extern "C" {
#include "clither/tests.h"
}

int tests_run(int argc, char* argv[])
{
    testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
