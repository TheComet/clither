#include <chrono>
#include <thread>

#include "gmock/gmock.h"

extern "C" {
#include "clither/tick.h"
}

#define NAME tick

using namespace testing;

TEST(NAME, tps_1)
{
    struct tick t;
    tick_cfg(&t, 10);
    auto start = std::chrono::steady_clock::now();
    EXPECT_THAT(tick_wait(&t), Eq(0));
    auto                          end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    EXPECT_THAT(elapsed_seconds.count(), Gt(0.09));
    EXPECT_THAT(elapsed_seconds.count(), Lt(0.11));
}

TEST(NAME, tps_60)
{
    struct tick t;
    tick_cfg(&t, 60);
    auto start = std::chrono::steady_clock::now();
    EXPECT_THAT(tick_wait(&t), Eq(0));
    auto                          end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    EXPECT_THAT(elapsed_seconds.count(), Gt(1.0 / 60.0 - 0.01));
    EXPECT_THAT(elapsed_seconds.count(), Lt(1.0 / 60.0 + 0.01));
}

TEST(NAME, too_long_1)
{
    struct tick t;
    tick_cfg(&t, 60);
    std::this_thread::sleep_for(
        std::chrono::duration<double>(1.0 / 60.0 + 1.0 / 60.0 / 2.0));
    auto start = std::chrono::steady_clock::now();
    EXPECT_THAT(tick_wait(&t), Eq(1));
    auto                          end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    EXPECT_THAT(elapsed_seconds.count(), Lt(0.001));
}

TEST(NAME, too_long_20)
{
    struct tick t;
    tick_cfg(&t, 60);
    std::this_thread::sleep_for(
        std::chrono::duration<double>(20.0 / 60.0 + 1.0 / 60.0 / 2.0));
    auto start = std::chrono::steady_clock::now();
    EXPECT_THAT(tick_wait(&t), Eq(20));
    auto                          end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    EXPECT_THAT(elapsed_seconds.count(), Lt(0.001));
}

TEST(NAME, no_context_switch)
{
    struct tick t;
    tick_cfg(&t, 1000000);
    auto start = std::chrono::steady_clock::now();
    EXPECT_THAT(tick_wait(&t), Eq(0));
    auto                          end = std::chrono::steady_clock::now();
    std::chrono::duration<double> elapsed_seconds = end - start;
    EXPECT_THAT(elapsed_seconds.count(), Gt(0.0000006));
    EXPECT_THAT(elapsed_seconds.count(), Lt(0.0000014));
}
