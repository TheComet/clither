#include "gmock/gmock.h"
#include "clither/args.h"

#define NAME args

using namespace testing;

TEST(NAME, no_args_check_defaults)
{
    const char* argv[] = {
        "./clither"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 1, (char**)argv), Eq(0));
    EXPECT_THAT(a.ip, StrEq(""));
    EXPECT_THAT(a.port, StrEq("5555"));
#if defined(CLITHER_LOGGING)
    EXPECT_THAT(a.log_file, StrEq("clither.txt"));
#endif
#if defined(CLITHER_GFX)
    EXPECT_THAT(a.mode, Eq(MODE_CLIENT));
#else
    EXPECT_THAT(a.mode, Eq(MODE_HEADLESS));
#endif
}

TEST(NAME, invalid_argument_1)
{
    const char* argv[] = {
        "./clither",
        "-"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(-1));
}

TEST(NAME, invalid_argument_2)
{
    const char* argv[] = {
        "./clither",
        "--invalid-arg"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(-1));
}

TEST(NAME, invalid_argument_3)
{
    const char* argv[] = {
        "./clither",
        "invalid-arg"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(-1));
}

TEST(NAME, help_single)
{
    const char* argv[] = {
        "./clither",
        "--help"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(1));
}

TEST(NAME, help_multiple)
{
    const char* argv[] = {
        "./clither",
        "--server",
        "--host",
        "--help",
        "--ip",
        "127.0.0.1",
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 6, (char**)argv), Eq(1));
}

#if defined(CLITHER_GFX)
TEST(NAME, set_host_mode_long)
{
    const char* argv[] = {
        "./clither",
        "--host"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(0));
    EXPECT_THAT(a.mode, Eq(MODE_CLIENT_AND_SERVER));
}

TEST(NAME, set_host_mode_short)
{
    const char* argv[] = {
        "./clither",
        "-h"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(0));
    EXPECT_THAT(a.mode, Eq(MODE_CLIENT_AND_SERVER));
}
#endif

#if defined(CLITHER_GFX)
TEST(NAME, terminate_parsing)
{
    const char* argv[] = {
        "./clither",
        "--",
        "--host"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0));
    EXPECT_THAT(a.mode, Eq(MODE_CLIENT));
}
#endif

TEST(NAME, set_headless_mode_long)
{
    const char* argv[] = {
        "./clither",
        "--server"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(0));
    EXPECT_THAT(a.mode, Eq(MODE_HEADLESS));
}

TEST(NAME, set_headless_mode_short)
{
    const char* argv[] = {
        "./clither",
        "-s"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(0));
    EXPECT_THAT(a.mode, Eq(MODE_HEADLESS));
}

#if defined(CLITHER_GFX)
TEST(NAME, headless_and_client_at_same_time_invalid_1)
{
    const char* argv[] = {
        "./clither",
        "-hs"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(-1));
}

TEST(NAME, headless_and_client_at_same_time_invalid_2)
{
    const char* argv[] = {
        "./clither",
        "--host",
        "--server"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(-1));
}
#endif

#if defined(CLITHER_LOGGING)
TEST(NAME, set_log_file_long)
{
    const char* argv[] = {
        "./clither",
        "--log",
        "mylog.txt"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0));
    EXPECT_THAT(a.log_file, StrEq("mylog.txt"));
}

TEST(NAME, set_log_file_short)
{
    const char* argv[] = {
        "./clither",
        "-l",
        "mylog.txt"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0));
    EXPECT_THAT(a.log_file, StrEq("mylog.txt"));
}
#endif

#if defined(CLITHER_LOGGING) && defined(CLITHER_GFX)
TEST(NAME, set_log_file_short_other_options)
{
    const char* argv[] = {
        "./clither",
        "-hl",
        "mylog.txt"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0));
    EXPECT_THAT(a.mode, Eq(MODE_CLIENT_AND_SERVER));
    EXPECT_THAT(a.log_file, StrEq("mylog.txt"));
}

TEST(NAME, set_log_file_short_other_options_invalid)
{
    const char* argv[] = {
        "./clither",
        "-lh",
        "mylog.txt"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(-1));
}
#endif

#if defined(CLITHER_LOGGING)
TEST(NAME, set_log_file_long_empty)
{
    const char* argv[] = {
        "./clither",
        "--log",
        ""
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0));
    EXPECT_THAT(a.log_file, StrEq(""));
}

TEST(NAME, set_log_file_short_empty)
{
    const char* argv[] = {
        "./clither",
        "-l",
        ""
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0));
    EXPECT_THAT(a.log_file, StrEq(""));
}

TEST(NAME, set_log_file_long_missing_arg)
{
    const char* argv[] = {
        "./clither",
        "--log"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(-1));
}

TEST(NAME, set_log_file_short_missing_arg)
{
    const char* argv[] = {
        "./clither",
        "-l"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(-1));
}
#endif

#if defined(CLITHER_LOGGING) && defined(CLITHER_GFX)
TEST(NAME, set_log_file_short_missing_arg_other_options)
{
    const char* argv[] = {
        "./clither",
        "-hl"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(-1));
}
#endif

TEST(NAME, set_address_long)
{
    const char* argv[] = {
        "./clither",
        "--ip",
        "192.168.1.2"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0));
    EXPECT_THAT(a.ip, StrEq("192.168.1.2"));
}

TEST(NAME, set_address_long_empty)
{
    const char* argv[] = {
        "./clither",
        "--ip",
        ""
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(-1));
}

TEST(NAME, set_address_long_missing_arg)
{
    const char* argv[] = {
        "./clither",
        "--ip"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(-1));
}

TEST(NAME, set_port_long)
{
    const char* argv[] = {
        "./clither",
        "--port",
        "1234"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0));
    EXPECT_THAT(a.port, StrEq("1234"));
}

TEST(NAME, set_port_short)
{
    const char* argv[] = {
        "./clither",
        "-p",
        "1234"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0));
    EXPECT_THAT(a.port, StrEq("1234"));
}

#if defined(CLITHER_GFX)
TEST(NAME, set_port_short_other_options)
{
    const char* argv[] = {
        "./clither",
        "-hp",
        "1234"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0));
    EXPECT_THAT(a.mode, Eq(MODE_CLIENT_AND_SERVER));
    EXPECT_THAT(a.port, StrEq("1234"));
}

TEST(NAME, set_port_short_other_options_invalid)
{
    const char* argv[] = {
        "./clither",
        "-ph",
        "1234"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(-1));
}
#endif

TEST(NAME, set_port_long_empty)
{
    const char* argv[] = {
        "./clither",
        "--port",
        ""
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(-1));
}

TEST(NAME, set_port_short_empty)
{
    const char* argv[] = {
        "./clither",
        "-p",
        ""
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(-1));
}

TEST(NAME, set_port_long_missing_arg)
{
    const char* argv[] = {
        "./clither",
        "--port"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(-1));
}

TEST(NAME, set_port_short_missing_arg)
{
    const char* argv[] = {
        "./clither",
        "-p"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(-1));
}

#if defined(CLITHER_GFX)
TEST(NAME, set_port_short_missing_arg_other_options)
{
    const char* argv[] = {
        "./clither",
        "-hp"
    };
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(-1));
}
#endif
