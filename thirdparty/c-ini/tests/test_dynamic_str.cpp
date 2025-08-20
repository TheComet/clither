#include "test_dynamic_str.h"

#include "gmock/gmock.h"

#define NAME dynamic_str

SECTION("dynamic_str")
struct dynamic_str_struct
{
    char* str;
};

struct NAME : testing::Test
{
    void SetUp() override { dynamic_str_struct_init(&s); }
    void TearDown() override { dynamic_str_struct_deinit(&s); }

    struct dynamic_str_struct s;
};

using namespace testing;

TEST_F(NAME, simple)
{
    const char* ini = "[dynamic_str]\nstr = \"Hello\"\n";
    ASSERT_THAT(
        dynamic_str_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.str, StrEq("Hello"));
}

TEST_F(NAME, str_empty)
{
    const char* ini = "[dynamic_str]\nstr = \"\"\n";
    ASSERT_THAT(
        dynamic_str_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.str, StrEq(""));
}

TEST_F(NAME, missing_string)
{
    const char* ini = "[dynamic_str]\n";
    ASSERT_THAT(
        dynamic_str_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.str[0], Eq('\0')); // Should not have changed
}

TEST_F(NAME, invalid)
{
    const char* ini = "[dynamic_str]\nstr = \"Invalid string\n";
    ASSERT_THAT(
        dynamic_str_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(-1));
    ASSERT_THAT(s.str[0], Eq('\0')); // Should not have changed
}

TEST_F(NAME, set_str_twice)
{
    const char* ini = "[dynamic_str]\nstr = \"First\"\n";
    ASSERT_THAT(
        dynamic_str_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.str, StrEq("First"));

    ini = "[dynamic_str]\nstr = \"Second\"\n";
    ASSERT_THAT(
        dynamic_str_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.str, StrEq("Second"));
}
