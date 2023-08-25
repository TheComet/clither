#include "clither/tests.h"
#include "gmock/gmock.h"

int
tests_run(int argc, char* argv[])
{
    testing::InitGoogleMock(&argc, argv);
    return RUN_ALL_TESTS();
}
