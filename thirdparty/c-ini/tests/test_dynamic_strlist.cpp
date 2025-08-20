#include "test_dynamic_strlist.h"

#include "gmock/gmock.h"

#define NAME dynamic_strlist

SECTION("dynamic_strlist")
struct dynamic_strlist_struct
{
    char** strlist;
};

struct NAME : testing::Test
{
    void SetUp() override { dynamic_strlist_struct_init(&s); }
    void TearDown() override { dynamic_strlist_struct_deinit(&s); }

    struct dynamic_strlist_struct s;
};

using namespace testing;

TEST_F(NAME, init_sets_all_to_null)
{
    ASSERT_THAT(s.strlist[0], IsNull());
}

TEST_F(NAME, simple)
{
    const char* ini =
        "[dynamic_strlist]\nstrlist = \"One\", \"Two\", \"Three\"\n";
    ASSERT_THAT(
        dynamic_strlist_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.strlist[0], StrEq("One"));
    ASSERT_THAT(s.strlist[1], StrEq("Two"));
    ASSERT_THAT(s.strlist[2], StrEq("Three"));
    ASSERT_THAT(s.strlist[3], IsNull());
}

TEST_F(NAME, str_empty)
{
    const char* ini = "[dynamic_strlist]\nstrlist = \"\"\n";
    ASSERT_THAT(
        dynamic_strlist_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.strlist[0], StrEq(""));
    ASSERT_THAT(s.strlist[1], IsNull());
}

TEST_F(NAME, missing_string)
{
    const char* ini = "[dynamic_strlist]\n";
    ASSERT_THAT(
        dynamic_strlist_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.strlist[0], IsNull()); // Should not have changed
}

TEST_F(NAME, invalid)
{
    const char* ini = "[dynamic_strlist]\nstrlist = \"Invalid string\n";
    ASSERT_THAT(
        dynamic_strlist_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(-1));
    ASSERT_THAT(s.strlist[0], IsNull()); // Should not have changed
}

TEST_F(NAME, set_str_twice)
{
    const char* ini = "[dynamic_strlist]\nstrlist = \"First\"\n";
    ASSERT_THAT(
        dynamic_strlist_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.strlist[0], StrEq("First"));
    ASSERT_THAT(s.strlist[1], IsNull());

    ini = "[dynamic_strlist]\nstrlist = \"Second\"\n";
    ASSERT_THAT(
        dynamic_strlist_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.strlist[0], StrEq("Second"));
    ASSERT_THAT(s.strlist[1], IsNull());
}

TEST_F(NAME, shrink_list)
{
    const char* ini =
        "[dynamic_strlist]\nstrlist = \"One\", \"Two\", \"Three\", \"Four\"\n";
    ASSERT_THAT(
        dynamic_strlist_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.strlist[0], StrEq("One"));
    ASSERT_THAT(s.strlist[1], StrEq("Two"));
    ASSERT_THAT(s.strlist[2], StrEq("Three"));
    ASSERT_THAT(s.strlist[3], StrEq("Four"));
    ASSERT_THAT(s.strlist[4], IsNull());

    ini = "[dynamic_strlist]\nstrlist = \"One\", \"Two\"\n";
    ASSERT_THAT(
        dynamic_strlist_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.strlist[0], StrEq("One"));
    ASSERT_THAT(s.strlist[1], StrEq("Two"));
    ASSERT_THAT(s.strlist[2], IsNull());
}
