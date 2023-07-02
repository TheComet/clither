#include "clither/tests.h"
#include "cstructures/init.h"
#include "gmock/gmock.h"

int
tests_run(int argc, char* argv[])
{
    testing::InitGoogleMock(&argc, argv);

    cs_init();
    int ret = RUN_ALL_TESTS();
    cs_deinit();
    return ret;
}
