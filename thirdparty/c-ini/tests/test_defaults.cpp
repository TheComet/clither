#include "test_defaults.h"

#include "gmock/gmock.h"

#define NAME defaults

SECTION("defaults")
struct defaults_struct
{
    char          fixed_str[16] DEFAULT("Hello");
    char* dyn_str DEFAULT("World");

    char fixed_strlist[4][16] DEFAULT("One") DEFAULT("Two") DEFAULT("Three");
    char** dyn_strlist DEFAULT("Four") DEFAULT("Five");

    char i8_1      DEFAULT(-128);
    int8_t i8_2    DEFAULT(127);
    uint8_t u8     DEFAULT(255);
    int16_t i16    DEFAULT(-32768);
    uint16_t u16   DEFAULT(65535);
    int i32_1      DEFAULT(-2147483648);
    unsigned u32_1 DEFAULT(4294967295);
    int32_t i32_2  DEFAULT(2147483647);
    uint32_t u32_2 DEFAULT(4294967295);

    float f32  DEFAULT(3.14);
    double f64 DEFAULT(2.718281828459045);

    bool bool_value DEFAULT(true);
    unsigned        bitfield : 1 DEFAULT(1);
};

struct NAME : testing::Test
{
    void SetUp() override { defaults_struct_init(&s); }
    void TearDown() override { defaults_struct_deinit(&s); }

    struct defaults_struct s;
};

using namespace testing;

TEST_F(NAME, check_all)
{
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
