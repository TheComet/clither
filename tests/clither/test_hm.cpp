#include <gmock/gmock.h>

extern "C" {
#include "clither/hm.h"
}

#define NAME         hm
#define MIN_CAPACITY 128

using namespace ::testing;

namespace {
const uintptr_t KEY1 = 1111;
const uintptr_t KEY2 = 2222;
const uintptr_t KEY3 = 3333;
const uintptr_t KEY4 = 4444;

HM_DECLARE(hm_test, uintptr_t, float, 32)
HM_DEFINE(hm_test, uintptr_t, float, 32);
} // namespace

struct NAME : Test
{
    void SetUp() override { hm_test_init(&hm_test); }
    void TearDown() override { hm_test_deinit(hm_test); }

    struct hm_test* hm_test;
};

TEST_F(NAME, null_hm_is_set)
{
    EXPECT_THAT(hm_test, IsNull());
    EXPECT_THAT(hm_count(hm_test), Eq(0));
    EXPECT_THAT(hm_capacity(hm_test), Eq(0));
}

TEST_F(NAME, deinit_null_hm_works)
{
    hm_test_deinit(hm_test);
}

TEST_F(NAME, insert_increases_slots_used)
{
    EXPECT_THAT(hm_count(hm_test), Eq(0));
    EXPECT_THAT(hm_test_insert_new(&hm_test, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hm_count(hm_test), Eq(1));
    EXPECT_THAT(hm_capacity(hm_test), Eq(MIN_CAPACITY));
}

TEST_F(NAME, erase_decreases_slots_used)
{
    EXPECT_THAT(hm_test_insert_new(&hm_test, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hm_test_erase(hm_test, KEY1), Pointee(5.6f));
    EXPECT_THAT(hm_count(hm_test), Eq(0));
    EXPECT_THAT(hm_capacity(hm_test), Eq(MIN_CAPACITY));
}

TEST_F(NAME, insert_same_key_twice_only_works_once)
{
    EXPECT_THAT(hm_test_insert_new(&hm_test, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hm_test_insert_new(&hm_test, KEY1, 7.6f), Eq(-1));
    EXPECT_THAT(hm_count(hm_test), Eq(1));
}

TEST_F(NAME, insert_or_get_returns_inserted_value)
{
    float  f = 0.0f;
    float* p = &f;
    EXPECT_THAT(hm_test_emplace_or_get(&hm_test, KEY1, &p), HM_NEW);
    *p = 5.6f;
    p = &f;
    EXPECT_THAT(hm_test_emplace_or_get(&hm_test, KEY1, &p), HM_EXISTS);
    EXPECT_THAT(f, Eq(0.0f));
    EXPECT_THAT(p, Pointee(5.6f));
    EXPECT_THAT(hm_count(hm_test), Eq(1));
}

TEST_F(NAME, erasing_same_key_twice_only_works_once)
{
    EXPECT_THAT(hm_test_insert_new(&hm_test, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hm_test_erase(hm_test, KEY1), Pointee(5.6f));
    EXPECT_THAT(hm_test_erase(hm_test, KEY1), IsNull());
    EXPECT_THAT(hm_count(hm_test), Eq(0));
}

TEST_F(NAME, insert_ab_erase_ba)
{
    EXPECT_THAT(hm_test_insert_new(&hm_test, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hm_test_insert_new(&hm_test, KEY2, 3.4f), Eq(0));
    EXPECT_THAT(hm_count(hm_test), Eq(2));
    EXPECT_THAT(hm_test_erase(hm_test, KEY2), Pointee(3.4f));
    EXPECT_THAT(hm_test_erase(hm_test, KEY1), Pointee(5.6f));
    EXPECT_THAT(hm_count(hm_test), Eq(0));
}

TEST_F(NAME, insert_ab_erase_ab)
{
    EXPECT_THAT(hm_test_insert_new(&hm_test, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hm_test_insert_new(&hm_test, KEY2, 3.4f), Eq(0));
    EXPECT_THAT(hm_count(hm_test), Eq(2));
    EXPECT_THAT(hm_test_erase(hm_test, KEY1), Pointee(5.6f));
    EXPECT_THAT(hm_test_erase(hm_test, KEY2), Pointee(3.4f));
    EXPECT_THAT(hm_count(hm_test), Eq(0));
}

TEST_F(NAME, insert_ab_find_ab)
{
    EXPECT_THAT(hm_test_insert_new(&hm_test, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hm_test_insert_new(&hm_test, KEY2, 3.4f), Eq(0));
    EXPECT_THAT(hm_test_find(hm_test, KEY1), Pointee(5.6f));
    EXPECT_THAT(hm_test_find(hm_test, KEY2), Pointee(3.4f));
}

TEST_F(NAME, insert_ab_erase_a_find_b)
{
    EXPECT_THAT(hm_test_insert_new(&hm_test, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hm_test_insert_new(&hm_test, KEY2, 3.4f), Eq(0));
    EXPECT_THAT(hm_test_erase(hm_test, KEY1), NotNull());
    EXPECT_THAT(hm_test_find(hm_test, KEY2), Pointee(3.4f));
}

TEST_F(NAME, insert_ab_erase_b_find_a)
{
    EXPECT_THAT(hm_test_insert_new(&hm_test, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hm_test_insert_new(&hm_test, KEY2, 3.4f), Eq(0));
    EXPECT_THAT(hm_test_erase(hm_test, KEY2), NotNull());
    EXPECT_THAT(hm_test_find(hm_test, KEY1), Pointee(5.6f));
}

TEST_F(NAME, rehash_test)
{
    uintptr_t key;
    float     value = 0;
    for (int i = 0; i != MIN_CAPACITY * 128; ++i, value += 1.5f)
    {
        key = i;
        ASSERT_THAT(hm_test_insert_new(&hm_test, key, value), Eq(0));
    }

    value = 0;
    for (int i = 0; i != MIN_CAPACITY * 128; ++i, value += 1.5f)
    {
        key = i;
        EXPECT_THAT(hm_test_erase(hm_test, key), Pointee(value)) << i;
    }
}

TEST_F(NAME, foreach_empty)
{
    int       slot;
    uintptr_t key;
    float*    value;
    int       counter = 0;
    hm_for_each (hm_test, slot, key, value)
    {
        counter++;
    }
    EXPECT_THAT(counter, Eq(0));
}

TEST_F(NAME, foreach)
{
    for (int i = 0; i != 16; ++i)
        hm_test_insert_new(&hm_test, i, float(i));

    hm_test_erase(hm_test, 5);
    hm_test_erase(hm_test, 8);
    hm_test_erase(hm_test, 14);
    hm_test_erase(hm_test, 3);
    hm_test_erase(hm_test, 11);
    hm_test_erase(hm_test, 6);

    for (int i = 16; i != 20; ++i)
        hm_test_insert_new(&hm_test, i, float(i));

    std::unordered_map<float, int> expected_values = {
        {0.0f, 0},
        {1.0f, 0},
        {2.0f, 0},
        {4.0f, 0},
        {7.0f, 0},
        {9.0f, 0},
        {10.0f, 0},
        {12.0f, 0},
        {13.0f, 0},
        {15.0f, 0},
        {16.0f, 0},
        {17.0f, 0},
        {18.0f, 0},
        {19.0f, 0},
    };

    int       slot;
    uintptr_t key;
    float*    value;
    hm_for_each (hm_test, slot, key, value)
    {
        expected_values[*value] += 1;
    }

    EXPECT_THAT(hm_count(hm_test), Eq(14));
    EXPECT_THAT(expected_values.size(), Eq(14));
    for (const auto& [k, v] : expected_values)
        EXPECT_THAT(v, Eq(1)) << k;
}
