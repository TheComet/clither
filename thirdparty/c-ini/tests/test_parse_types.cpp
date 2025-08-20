#include "test_parse_types.h"

#include "gmock/gmock.h"

#define NAME parse_types

SECTION("parse_types")
struct parse_types_struct
{
    char  fixed_str[16];
    char* dyn_str;

    char   fixed_strlist[4][16];
    char** dyn_strlist;

    char     i8_1;
    int8_t   i8_2;
    uint8_t  u8;
    int16_t  i16;
    uint16_t u16;
    int      i32_1;
    unsigned u32_1;
    int32_t  i32_2;
    uint32_t u32_2;

    float  f32;
    double f64;

    bool     bool_value;
    unsigned bitfield : 1;
};

struct NAME : testing::Test
{
    void SetUp() override { parse_types_struct_init(&s); }
    void TearDown() override { parse_types_struct_deinit(&s); }

    struct parse_types_struct s;
};

using namespace testing;

TEST_F(NAME, parse_types_fixed_str)
{
    const char* ini = "[parse_types]\nfixed_str = \"Hello\"\n";
    ASSERT_THAT(
        parse_types_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.fixed_str, StrEq("Hello"));
}

TEST_F(NAME, parse_types_dyn_str)
{
    const char* ini = "[parse_types]\ndyn_str = \"World\"\n";
    ASSERT_THAT(
        parse_types_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.dyn_str, StrEq("World"));
}

TEST_F(NAME, parse_types_fixed_strlist)
{
    const char* ini =
        "[parse_types]\n"
        "fixed_strlist = \"One\", \"Two\", \"Three\"\n";
    ASSERT_THAT(
        parse_types_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.fixed_strlist[0], StrEq("One"));
    ASSERT_THAT(s.fixed_strlist[1], StrEq("Two"));
    ASSERT_THAT(s.fixed_strlist[2], StrEq("Three"));
}

TEST_F(NAME, parse_types_dyn_strlist)
{
    const char* ini =
        "[parse_types]\n"
        "dyn_strlist = \"Four\", \"Five\"\n";
    ASSERT_THAT(
        parse_types_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.dyn_strlist[0], StrEq("Four"));
    ASSERT_THAT(s.dyn_strlist[1], StrEq("Five"));
}

TEST_F(NAME, parse_types_i8_1)
{
    const char* ini = "[parse_types]\ni8_1 = -128\n";
    ASSERT_THAT(
        parse_types_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.i8_1, Eq(-128));
}

TEST_F(NAME, parse_types_i8_2)
{
    const char* ini = "[parse_types]\ni8_2 = 127\n";
    ASSERT_THAT(
        parse_types_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.i8_2, Eq(127));
}

TEST_F(NAME, parse_types_u8)
{
    const char* ini = "[parse_types]\nu8 = 255\n";
    ASSERT_THAT(
        parse_types_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.u8, Eq(255));
}

TEST_F(NAME, parse_types_i16)
{
    const char* ini = "[parse_types]\ni16 = -32768\n";
    ASSERT_THAT(
        parse_types_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.i16, Eq(-32768));
}

TEST_F(NAME, parse_types_u16)
{
    const char* ini = "[parse_types]\nu16 = 65535\n";
    ASSERT_THAT(
        parse_types_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.u16, Eq(65535));
}

TEST_F(NAME, parse_types_i32_1)
{
    const char* ini = "[parse_types]\ni32_1 = -2147483648\n";
    ASSERT_THAT(
        parse_types_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.i32_1, Eq(-2147483648));
}

TEST_F(NAME, parse_types_u32_1)
{
    const char* ini = "[parse_types]\nu32_1 = 4294967295\n";
    ASSERT_THAT(
        parse_types_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.u32_1, Eq(4294967295U));
}

TEST_F(NAME, parse_types_i32_2)
{
    const char* ini = "[parse_types]\ni32_2 = 2147483647\n";
    ASSERT_THAT(
        parse_types_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.i32_2, Eq(2147483647));
}

TEST_F(NAME, parse_types_u32_2)
{
    const char* ini = "[parse_types]\nu32_2 = 4294967295\n";
    ASSERT_THAT(
        parse_types_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.u32_2, Eq(4294967295U));
}

TEST_F(NAME, parse_types_f32)
{
    const char* ini = "[parse_types]\nf32 = 3.14\n";
    ASSERT_THAT(
        parse_types_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.f32, FloatEq(3.14f));
}

TEST_F(NAME, parse_types_f64)
{
    const char* ini = "[parse_types]\nf64 = 2.718281828459045\n";
    ASSERT_THAT(
        parse_types_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.f64, DoubleEq(2.718281828459045));
}

TEST_F(NAME, parse_types_bool_value)
{
    const char* ini = "[parse_types]\nbool_value = true\n";
    ASSERT_THAT(
        parse_types_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.bool_value, Eq(true));
}

TEST_F(NAME, parse_types_bitfield)
{
    const char* ini = "[parse_types]\nbitfield = 1\n";
    ASSERT_THAT(
        parse_types_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));
    ASSERT_THAT(s.bitfield, Eq(1U));
}

TEST_F(NAME, parse_types_all)
{
    const char* ini =
        "[parse_types]\n"
        "fixed_str = \"Hello\"\n"
        "dyn_str = \"World\"\n"
        "fixed_strlist = \"One\", \"Two\", \"Three\"\n"
        "dyn_strlist = \"Four\", \"Five\"\n"
        "i8_1 = -128\n"
        "i8_2 = 127\n"
        "u8 = 255\n"
        "i16 = -32768\n"
        "u16 = 65535\n"
        "i32_1 = -2147483648\n"
        "u32_1 = 4294967295\n"
        "i32_2 = 2147483647\n"
        "u32_2 = 4294967295\n"
        "f32 = 3.14\n"
        "f64 = 2.718281828459045\n"
        "bool_value = true\n"
        "bitfield = 1";
    ASSERT_THAT(
        parse_types_struct_parse(&s, "<stdin>", ini, strlen(ini)), Eq(0));

    ASSERT_THAT(s.fixed_str, StrEq("Hello"));
    ASSERT_THAT(s.dyn_str, StrEq("World"));
    ASSERT_THAT(s.fixed_strlist[0], StrEq("One"));
    ASSERT_THAT(s.fixed_strlist[1], StrEq("Two"));
    ASSERT_THAT(s.fixed_strlist[2], StrEq("Three"));
    ASSERT_THAT(s.dyn_strlist[0], StrEq("Four"));
    ASSERT_THAT(s.dyn_strlist[1], StrEq("Five"));
    ASSERT_THAT(s.i8_1, Eq(-128));
    ASSERT_THAT(s.i8_2, Eq(127));
    ASSERT_THAT(s.u8, Eq(255));
    ASSERT_THAT(s.i16, Eq(-32768));
    ASSERT_THAT(s.u16, Eq(65535));
    ASSERT_THAT(s.i32_1, Eq(-2147483648));
    ASSERT_THAT(s.u32_1, Eq(4294967295U));
    ASSERT_THAT(s.i32_2, Eq(2147483647));
    ASSERT_THAT(s.u32_2, Eq(4294967295U));
    ASSERT_THAT(s.f32, FloatEq(3.14f));
    ASSERT_THAT(s.f64, DoubleEq(2.718281828459045));
    ASSERT_THAT(s.bool_value, Eq(true));
    ASSERT_THAT(s.bitfield, Eq(1U));
}
