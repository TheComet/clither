#include <unordered_map>

#include <gmock/gmock.h>

extern "C" {
#include "clither/util/hmap.h"
}

#define NAME         hmap
#define MIN_CAPACITY 128

using namespace ::testing;

namespace {
const uintptr_t KEY1 = 1111;
const uintptr_t KEY2 = 2222;
const uintptr_t KEY3 = 3333;
const uintptr_t KEY4 = 4444;

HMAP_DECLARE(hmap_test, uintptr_t, float, 32)
HMAP_DEFINE(hmap_test, uintptr_t, float, 32);
} // namespace

struct NAME : Test
{
    void SetUp() override { hmap_test_init(&hmap_test); }
    void TearDown() override { hmap_test_deinit(hmap_test); }

    struct hmap_test* hmap_test;
};

TEST_F(NAME, null_hmap_is_set)
{
    EXPECT_THAT(hmap_test, IsNull());
    EXPECT_THAT(hmap_count(hmap_test), Eq(0));
    EXPECT_THAT(hmap_capacity(hmap_test), Eq(0));
}

TEST_F(NAME, deinit_null_hmap_works)
{
    hmap_test_deinit(hmap_test);
}

TEST_F(NAME, insert_increases_slots_used)
{
    EXPECT_THAT(hmap_count(hmap_test), Eq(0));
    EXPECT_THAT(hmap_test_insert_new(&hmap_test, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hmap_count(hmap_test), Eq(1));
    EXPECT_THAT(hmap_capacity(hmap_test), Eq(MIN_CAPACITY));
}

TEST_F(NAME, erase_decreases_slots_used)
{
    EXPECT_THAT(hmap_test_insert_new(&hmap_test, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hmap_test_erase(hmap_test, KEY1), Pointee(5.6f));
    EXPECT_THAT(hmap_count(hmap_test), Eq(0));
    EXPECT_THAT(hmap_capacity(hmap_test), Eq(MIN_CAPACITY));
}

TEST_F(NAME, insert_same_key_twice_only_works_once)
{
    EXPECT_THAT(hmap_test_insert_new(&hmap_test, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hmap_test_insert_new(&hmap_test, KEY1, 7.6f), Eq(-1));
    EXPECT_THAT(hmap_count(hmap_test), Eq(1));
}

TEST_F(NAME, insert_or_get_returns_inserted_value)
{
    float  f = 0.0f;
    float* p = &f;
    EXPECT_THAT(hmap_test_emplace_or_get(&hmap_test, KEY1, &p), HMAP_NEW);
    *p = 5.6f;
    p = &f;
    EXPECT_THAT(hmap_test_emplace_or_get(&hmap_test, KEY1, &p), HMAP_EXISTS);
    EXPECT_THAT(f, Eq(0.0f));
    EXPECT_THAT(p, Pointee(5.6f));
    EXPECT_THAT(hmap_count(hmap_test), Eq(1));
}

TEST_F(NAME, erasing_same_key_twice_only_works_once)
{
    EXPECT_THAT(hmap_test_insert_new(&hmap_test, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hmap_test_erase(hmap_test, KEY1), Pointee(5.6f));
    EXPECT_THAT(hmap_test_erase(hmap_test, KEY1), IsNull());
    EXPECT_THAT(hmap_count(hmap_test), Eq(0));
}

TEST_F(NAME, insert_ab_erase_ba)
{
    EXPECT_THAT(hmap_test_insert_new(&hmap_test, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hmap_test_insert_new(&hmap_test, KEY2, 3.4f), Eq(0));
    EXPECT_THAT(hmap_count(hmap_test), Eq(2));
    EXPECT_THAT(hmap_test_erase(hmap_test, KEY2), Pointee(3.4f));
    EXPECT_THAT(hmap_test_erase(hmap_test, KEY1), Pointee(5.6f));
    EXPECT_THAT(hmap_count(hmap_test), Eq(0));
}

TEST_F(NAME, insert_ab_erase_ab)
{
    EXPECT_THAT(hmap_test_insert_new(&hmap_test, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hmap_test_insert_new(&hmap_test, KEY2, 3.4f), Eq(0));
    EXPECT_THAT(hmap_count(hmap_test), Eq(2));
    EXPECT_THAT(hmap_test_erase(hmap_test, KEY1), Pointee(5.6f));
    EXPECT_THAT(hmap_test_erase(hmap_test, KEY2), Pointee(3.4f));
    EXPECT_THAT(hmap_count(hmap_test), Eq(0));
}

TEST_F(NAME, insert_ab_find_ab)
{
    EXPECT_THAT(hmap_test_insert_new(&hmap_test, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hmap_test_insert_new(&hmap_test, KEY2, 3.4f), Eq(0));
    EXPECT_THAT(hmap_test_find(hmap_test, KEY1), Pointee(5.6f));
    EXPECT_THAT(hmap_test_find(hmap_test, KEY2), Pointee(3.4f));
}

TEST_F(NAME, insert_ab_erase_a_find_b)
{
    EXPECT_THAT(hmap_test_insert_new(&hmap_test, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hmap_test_insert_new(&hmap_test, KEY2, 3.4f), Eq(0));
    EXPECT_THAT(hmap_test_erase(hmap_test, KEY1), NotNull());
    EXPECT_THAT(hmap_test_find(hmap_test, KEY2), Pointee(3.4f));
}

TEST_F(NAME, insert_ab_erase_b_find_a)
{
    EXPECT_THAT(hmap_test_insert_new(&hmap_test, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hmap_test_insert_new(&hmap_test, KEY2, 3.4f), Eq(0));
    EXPECT_THAT(hmap_test_erase(hmap_test, KEY2), NotNull());
    EXPECT_THAT(hmap_test_find(hmap_test, KEY1), Pointee(5.6f));
}

TEST_F(NAME, rehash_test)
{
    uintptr_t key;
    float     value = 0;
    for (int i = 0; i != MIN_CAPACITY * 128; ++i, value += 1.5f)
    {
        key = i;
        ASSERT_THAT(hmap_test_insert_new(&hmap_test, key, value), Eq(0));
    }

    value = 0;
    for (int i = 0; i != MIN_CAPACITY * 128; ++i, value += 1.5f)
    {
        key = i;
        EXPECT_THAT(hmap_test_erase(hmap_test, key), Pointee(value)) << i;
    }
}

TEST_F(NAME, foreach_empty)
{
    int       slot;
    uintptr_t key;
    float*    value;
    int       counter = 0;
    hmap_for_each(hmap_test, slot, key, value)(void) slot, (void)key,
        (void)value, counter++;
    EXPECT_THAT(counter, Eq(0));
}

TEST_F(NAME, foreach)
{
    for (int i = 0; i != 16; ++i)
        hmap_test_insert_new(&hmap_test, i, float(i));

    hmap_test_erase(hmap_test, 5);
    hmap_test_erase(hmap_test, 8);
    hmap_test_erase(hmap_test, 14);
    hmap_test_erase(hmap_test, 3);
    hmap_test_erase(hmap_test, 11);
    hmap_test_erase(hmap_test, 6);

    for (int i = 16; i != 20; ++i)
        hmap_test_insert_new(&hmap_test, i, float(i));

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
    hmap_for_each(hmap_test, slot, key, value)(void) slot, (void)key,
        expected_values[*value] += 1;

    EXPECT_THAT(hmap_count(hmap_test), Eq(14));
    EXPECT_THAT(expected_values.size(), Eq(14));
    for (auto it = expected_values.begin(); it != expected_values.end(); ++it)
        EXPECT_THAT(it->second, Eq(1)) << it->first;
}
