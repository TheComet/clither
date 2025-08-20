#include "test_fixed_strlist.h"

#include "gmock/gmock.h"

#define NAME fixed_strlist

SECTION("fixed_strlist")
struct fixed_strlist_struct
{
    char strlist[4][16];
};

struct NAME : testing::Test
{
    void SetUp() override { fixed_strlist_struct_init(&s); }
    void TearDown() override { fixed_strlist_struct_deinit(&s); }

    struct fixed_strlist_struct s;
};

using namespace testing;

TEST_F(NAME, init_sets_all_to_null)
{
    fixed_strlist_struct_init(&s);
    for (int i = 0; i < 4; i++)
        ASSERT_THAT(s.strlist[i], StrEq(""));
}

TEST_F(NAME, simple)
{
    const char* ini =
        "[fixed_strlist]\nstrlist = \"One\", \"Two\", \"Three\"\n";
    ASSERT_THAT(
        fixed_strlist_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.strlist[0], StrEq("One"));
    ASSERT_THAT(s.strlist[1], StrEq("Two"));
    ASSERT_THAT(s.strlist[2], StrEq("Three"));
    ASSERT_THAT(s.strlist[3], StrEq("")); // Last should be empty
}

TEST_F(NAME, str_empty)
{
    const char* ini = "[fixed_strlist]\nstrlist = \"\"\n";
    ASSERT_THAT(
        fixed_strlist_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.strlist[0], StrEq(""));
}

TEST_F(NAME, str_too_long)
{
    const char* ini =
        "[fixed_strlist]\nstrlist = \"This string is too long\"\n";
    ASSERT_THAT(
        fixed_strlist_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(-1));
    ASSERT_THAT(s.strlist[0], StrEq("")); // Should not have changed
}

TEST_F(NAME, third_str_too_long)
{
    const char* ini =
        "[fixed_strlist]\nstrlist = \"One\", \"Two\", \"This string is too "
        "long\"\n";
    ASSERT_THAT(
        fixed_strlist_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(-1));
    ASSERT_THAT(s.strlist[0], StrEq("One"));
    ASSERT_THAT(s.strlist[1], StrEq("Two"));
    ASSERT_THAT(s.strlist[2], StrEq("")); // Last should be empty
}

TEST_F(NAME, missing_string)
{
    const char* ini = "[fixed_strlist]\n";
    ASSERT_THAT(
        fixed_strlist_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.strlist[0], StrEq("")); // Should not have changed
}

TEST_F(NAME, invalid)
{
    const char* ini = "[fixed_strlist]\nstrlist = \"Invalid string\n";
    ASSERT_THAT(
        fixed_strlist_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(-1));
    ASSERT_THAT(s.strlist[0], StrEq("")); // Should not have changed
}

TEST_F(NAME, set_str_twice)
{
    const char* ini = "[fixed_strlist]\nstrlist = \"First\"\n";
    ASSERT_THAT(
        fixed_strlist_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.strlist[0], StrEq("First"));

    ini = "[fixed_strlist]\nstrlist = \"Second\"\n";
    ASSERT_THAT(
        fixed_strlist_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.strlist[0], StrEq("Second"));
}

TEST_F(NAME, shrink_list)
{
    const char* ini =
        "[fixed_strlist]\nstrlist = \"One\", \"Two\", \"Three\", \"Four\"\n";
    ASSERT_THAT(
        fixed_strlist_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.strlist[0], StrEq("One"));
    ASSERT_THAT(s.strlist[1], StrEq("Two"));
    ASSERT_THAT(s.strlist[2], StrEq("Three"));
    ASSERT_THAT(s.strlist[3], StrEq("Four"));

    ini = "[fixed_strlist]\nstrlist = \"One\", \"Two\"\n";
    ASSERT_THAT(
        fixed_strlist_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.strlist[0], StrEq("One"));
    ASSERT_THAT(s.strlist[1], StrEq("Two"));
    ASSERT_THAT(s.strlist[2], StrEq(""));
    ASSERT_THAT(s.strlist[3], StrEq(""));
}
