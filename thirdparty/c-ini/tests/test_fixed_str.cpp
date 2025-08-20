#include "test_fixed_str.h"

#include "gmock/gmock.h"

#define NAME fixed_str

SECTION("fixed_str")
struct fixed_str_struct
{
    char str[16];
};

struct NAME : testing::Test
{
    void SetUp() override { fixed_str_struct_init(&s); }
    void TearDown() override { fixed_str_struct_deinit(&s); }

    struct fixed_str_struct s;
};

using namespace testing;

TEST_F(NAME, simple)
{
    const char* ini = "[fixed_str]\nstr = \"Hello\"\n";
    ASSERT_THAT(fixed_str_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.str, StrEq("Hello"));
}

TEST_F(NAME, str_empty)
{
    const char* ini = "[fixed_str]\nstr = \"\"\n";
    ASSERT_THAT(fixed_str_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.str, StrEq(""));
}

TEST_F(NAME, str_too_long)
{
    const char* ini = "[fixed_str]\nstr = \"This string is too long\"\n";
    ASSERT_THAT(
        fixed_str_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(-1));
    ASSERT_THAT(s.str[0], Eq('\0')); // Should not have changed
}

TEST_F(NAME, missing_string)
{
    const char* ini = "[fixed_str]\n";
    ASSERT_THAT(fixed_str_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.str[0], Eq('\0')); // Should not have changed
}

TEST_F(NAME, invalid)
{
    const char* ini = "[fixed_str]\nstr = \"Invalid string\n";
    ASSERT_THAT(
        fixed_str_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(-1));
    ASSERT_THAT(s.str[0], Eq('\0')); // Should not have changed
}

TEST_F(NAME, set_str_twice)
{
    const char* ini = "[fixed_str]\nstr = \"First\"\n";
    ASSERT_THAT(fixed_str_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.str, StrEq("First"));

    ini = "[fixed_str]\nstr = \"Second\"\n";
    ASSERT_THAT(fixed_str_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.str, StrEq("Second"));
}
