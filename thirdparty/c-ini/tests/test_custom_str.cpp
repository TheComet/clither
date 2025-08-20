#include "custom_str.h"
#include "test_custom_str.h"

#include "gmock/gmock.h"

#define NAME custom_str

SECTION("custom_str")
struct custom_str_struct
{
    struct str* str STRING(custom_str);
};

struct NAME : public testing::Test
{
    void SetUp() override { custom_str_struct_init(&s); }
    void TearDown() override { custom_str_struct_deinit(&s); }

    struct custom_str_struct s;
};

using namespace testing;

TEST_F(NAME, simple)
{
    const char* ini = "[custom_str]\nstr = \"Hello\"\n";
    ASSERT_THAT(
        custom_str_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(*reinterpret_cast<std::string*>(s.str), Eq("Hello"));
}

TEST_F(NAME, str_empty)
{
    const char* ini = "[custom_str]\nstr = \"\"\n";
    ASSERT_THAT(
        custom_str_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(*reinterpret_cast<std::string*>(s.str), Eq(""));
}

TEST_F(NAME, missing_string)
{
    const char* ini = "[custom_str]\n";
    ASSERT_THAT(
        custom_str_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(*reinterpret_cast<std::string*>(s.str), Eq(""));
}

TEST_F(NAME, invalid)
{
    const char* ini = "[custom_str]\nstr = \"Invalid string\n";
    ASSERT_THAT(
        custom_str_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(-1));
    ASSERT_THAT(*reinterpret_cast<std::string*>(s.str), Eq(""));
}

TEST_F(NAME, set_str_twice)
{
    const char* ini = "[custom_str]\nstr = \"First\"\n";
    ASSERT_THAT(
        custom_str_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(*reinterpret_cast<std::string*>(s.str), Eq("First"));

    ini = "[custom_str]\nstr = \"Second\"\n";
    ASSERT_THAT(
        custom_str_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(*reinterpret_cast<std::string*>(s.str), Eq("Second"));
}
