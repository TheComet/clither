#include "clither/tests/LogHelper.hpp"

#include "gmock/gmock.h"

extern "C" {
#include "clither/util/strlist.h"
}

#define NAME test_strlist

using namespace ::testing;

struct NAME : Test, LogHelper
{
public:
    void            SetUp() override { strlist_init(&strlist); }
    void            TearDown() override { strlist_deinit(strlist); }
    struct strlist* strlist;
};

TEST_F(NAME, append_strings)
{
    ASSERT_THAT(strlist_add_cstr(&strlist, "Hello"), Eq(0));
    ASSERT_THAT(strlist_add_cstr(&strlist, "World"), Eq(0));
    ASSERT_THAT(strlist_add_cstr(&strlist, "!"), Eq(0));
    ASSERT_THAT(strlist_count(strlist), Eq(3));
    ASSERT_THAT(strview_eq_cstr(strlist_view(strlist, 0), "Hello"), IsTrue());
    ASSERT_THAT(strview_eq_cstr(strlist_view(strlist, 1), "World"), IsTrue());
    ASSERT_THAT(strview_eq_cstr(strlist_view(strlist, 2), "!"), IsTrue());
}

TEST_F(NAME, many_strings)
{
    for (int i = 0; i < 1000; ++i)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "String %d", i);
        ASSERT_THAT(strlist_add_cstr(&strlist, buf), Eq(0));
    }
    ASSERT_THAT(strlist_count(strlist), Eq(1000));

    for (int i = 0; i < 1000; ++i)
    {
        char buf[32];
        snprintf(buf, sizeof(buf), "String %d", i);
        ASSERT_THAT(strview_eq_cstr(strlist_view(strlist, i), buf), IsTrue());
    }
}
