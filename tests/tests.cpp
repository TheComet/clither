#include "clither/tests.h"
#include "cstructures/memory.h"
#include "gmock/gmock.h"

int
tests_run(int argc, char* argv[])
{
    testing::InitGoogleMock(&argc, argv);

    memory_init_thread();
    int ret = RUN_ALL_TESTS();
    memory_deinit_thread();
    return ret;
}
