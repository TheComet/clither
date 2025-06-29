#include "clither/tests/LogHelper.hpp"

#include "gmock/gmock.h"

extern "C" {
#include "clither/util/strview.h"
}

#define NAME test_strview

using namespace ::testing;

struct NAME : Test, LogHelper
{
};

TEST_F(NAME, compare_cstr)
{
    struct strview view = strview("Hello, world!", 0, 10);
    ASSERT_THAT(strview_eq_cstr(view, "Hello, wor"), IsTrue());
}
