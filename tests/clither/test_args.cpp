#include "clither/tests/LogHelper.hpp"

#include "gmock/gmock.h"

extern "C" {
#include "clither/game/args.h"
}

#define NAME test_args

using namespace testing;

struct NAME : Test, LogHelper
{
};

TEST_F(NAME, no_args_check_defaults)
{
    const char* argv[] = {"./clither"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 1, (char**)argv), Eq(1)) << log().text;
    EXPECT_THAT(a.settings_file, StrEq("settings.ini"));
#if defined(CLITHER_SERVER) || defined(CLITHER_CLIENT) || defined(CLITHER_MCD)
    EXPECT_THAT(a.addr, IsNull());
    EXPECT_THAT(a.port, IsNull());
#endif
#if defined(CLITHER_CLIENT)
    EXPECT_THAT(a.username, IsNull());
#endif
#if defined(CLITHER_LOGGING)
    EXPECT_THAT(a.log_file, StrEq("clither.txt"));
    EXPECT_THAT(a.netlog_file, StrEq("net.txt"));
    EXPECT_THAT(a.prefix, IsNull());
#endif
#if defined(CLITHER_CLIENT)
    EXPECT_THAT(a.mode, Eq(MODE_CLIENT));
#elif defined(CLITHER_SERVER)
    EXPECT_THAT(a.mode, Eq(MODE_SERVER));
#else
    EXPECT_THAT(a.mode, Eq(MODE_NONE));
#endif
#if defined(CLITHER_GFX)
    EXPECT_THAT(a.gfx_backend, Eq(-1));
#endif
#if defined(CLITHER_MCD)
    EXPECT_THAT(a.mcd_port, IsNull());
    EXPECT_THAT(a.mcd_latency, Eq(-1));
    EXPECT_THAT(a.mcd_dup, Eq(-1));
    EXPECT_THAT(a.mcd_loss, Eq(-1));
    EXPECT_THAT(a.mcd_reorder, Eq(-1));
#endif
}

TEST_F(NAME, invalid_argument_1)
{
    const char* argv[] = {"./clither", "-"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(-1)) << log().text;
}

TEST_F(NAME, invalid_argument_2)
{
    const char* argv[] = {"./clither", "--invalid-arg"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(-1)) << log().text;
}

TEST_F(NAME, invalid_argument_3)
{
    const char* argv[] = {"./clither", "invalid-arg"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(-1)) << log().text;
}

TEST_F(NAME, help_single)
{
    const char* argv[] = {"./clither", "--help"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(1)) << log().text;
}

#if defined(CLITHER_SERVER) && defined(CLITHER_CLIENT)
TEST_F(NAME, help_multiple)
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
    ASSERT_THAT(args_parse(&a, 6, (char**)argv), Eq(1)) << log().text;
}
#endif

#if defined(CLITHER_CLIENT) && defined(CLITHER_SERVER)
TEST_F(NAME, set_host_mode_long)
{
    const char* argv[] = {"./clither", "--host"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(0)) << log().text;
    EXPECT_THAT(a.mode, Eq(MODE_HOST));
}

TEST_F(NAME, terminate_parsing)
{
    const char* argv[] = {"./clither", "--", "--host"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0)) << log().text;
    EXPECT_THAT(a.mode, Eq(MODE_CLIENT));
}

TEST_F(NAME, headless_and_client_at_same_time_invalid_1)
{
    const char* argv[] = {"./clither", "--host", "--server"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(-1));
}
#endif

#if defined(CLITHER_SERVER)
TEST_F(NAME, set_headless_mode_long)
{
    const char* argv[] = {"./clither", "--server"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(0)) << log().text;
    EXPECT_THAT(a.mode, Eq(MODE_SERVER));
}

TEST_F(NAME, set_headless_mode_short)
{
    const char* argv[] = {"./clither", "-s"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(0)) << log().text;
    EXPECT_THAT(a.mode, Eq(MODE_SERVER));
}
#endif

#if defined(CLITHER_LOGGING)
TEST_F(NAME, set_log_file_long)
{
    const char* argv[] = {"./clither", "--log", "mylog.txt"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0)) << log().text;
    EXPECT_THAT(a.log_file, StrEq("mylog.txt"));
}

TEST_F(NAME, set_log_file_short)
{
    const char* argv[] = {"./clither", "-l", "mylog.txt"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0)) << log().text;
    EXPECT_THAT(a.log_file, StrEq("mylog.txt"));
}

TEST_F(NAME, set_netlog_file_long)
{
    const char* argv[] = {"./clither", "--netlog", "mylog.txt"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0)) << log().text;
    EXPECT_THAT(a.netlog_file, StrEq("mylog.txt"));
}

TEST_F(NAME, set_prefix_long)
{
    const char* argv[] = {"./clither", "--prefix", "MyPrefix"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0)) << log().text;
    EXPECT_THAT(a.prefix, StrEq("MyPrefix"));
}
#endif

#if defined(CLITHER_LOGGING) && defined(CLITHER_SERVER)
TEST_F(NAME, set_log_file_short_other_options)
{
    const char* argv[] = {"./clither", "-sl", "mylog.txt"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0)) << log().text;
    EXPECT_THAT(a.mode, Eq(MODE_SERVER));
    EXPECT_THAT(a.log_file, StrEq("mylog.txt"));
}

TEST_F(NAME, set_log_file_short_other_options_invalid)
{
    const char* argv[] = {"./clither", "-lh", "mylog.txt"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(-1)) << log().text;
}
#endif

#if defined(CLITHER_LOGGING)
TEST_F(NAME, set_log_file_long_empty)
{
    const char* argv[] = {"./clither", "--log", ""};
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0)) << log().text;
    EXPECT_THAT(a.log_file, StrEq(""));
}

TEST_F(NAME, set_log_file_short_empty)
{
    const char* argv[] = {"./clither", "-l", ""};
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0)) << log().text;
    EXPECT_THAT(a.log_file, StrEq(""));
}

TEST_F(NAME, set_log_file_long_missing_arg)
{
    const char* argv[] = {"./clither", "--log"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(-1)) << log().text;
}

TEST_F(NAME, set_log_file_short_missing_arg)
{
    const char* argv[] = {"./clither", "-l"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(-1)) << log().text;
}
#endif

#if defined(CLITHER_LOGGING) && defined(CLITHER_GFX)
TEST_F(NAME, set_log_file_short_missing_arg_other_options)
{
    const char* argv[] = {"./clither", "-hl"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(-1)) << log().text;
}
#endif

#if defined(CLITHER_SERVER) || defined(CLITHER_CLIENT) || defined(CLITHER_MCD)
TEST_F(NAME, set_address_long)
{
    const char* argv[] = {"./clither", "--addr", "192.168.1.2"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0)) << log().text;
    EXPECT_THAT(a.addr, StrEq("192.168.1.2"));
}

TEST_F(NAME, set_address_long_empty)
{
    const char* argv[] = {"./clither", "--addr", ""};
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(-1)) << log().text;
}

TEST_F(NAME, set_address_long_missing_arg)
{
    const char* argv[] = {"./clither", "--addr"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(-1)) << log().text;
}

TEST_F(NAME, set_port_long)
{
    const char* argv[] = {"./clither", "--port", "1234"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0)) << log().text;
    EXPECT_THAT(a.port, StrEq("1234"));
}

TEST_F(NAME, set_port_short)
{
    const char* argv[] = {"./clither", "-p", "1234"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0)) << log().text;
    EXPECT_THAT(a.port, StrEq("1234"));
}
#endif

#if defined(CLITHER_SERVER)
TEST_F(NAME, set_port_short_other_options)
{
    const char* argv[] = {"./clither", "-sp", "1234"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(0)) << log().text;
    EXPECT_THAT(a.mode, Eq(MODE_SERVER));
    EXPECT_THAT(a.port, StrEq("1234"));
}

TEST_F(NAME, set_port_short_other_options_invalid)
{
    const char* argv[] = {"./clither", "-ps", "1234"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(-1));
}

TEST_F(NAME, set_port_short_missing_arg_other_options)
{
    const char* argv[] = {"./clither", "-sp"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(-1));
}
#endif

TEST_F(NAME, set_port_long_empty)
{
    const char* argv[] = {"./clither", "--port", ""};
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(-1));
}

TEST_F(NAME, set_port_short_empty)
{
    const char* argv[] = {"./clither", "-p", ""};
    struct args a;
    ASSERT_THAT(args_parse(&a, 3, (char**)argv), Eq(-1));
}

TEST_F(NAME, set_port_long_missing_arg)
{
    const char* argv[] = {"./clither", "--port"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(-1));
}

TEST_F(NAME, set_port_short_missing_arg)
{
    const char* argv[] = {"./clither", "-p"};
    struct args a;
    ASSERT_THAT(args_parse(&a, 2, (char**)argv), Eq(-1));
}
