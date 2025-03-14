#include "gmock/gmock.h"

extern "C" {
#include "clither/bmap.h"
}

#define NAME              bmap
#define BMAP_MIN_CAPACITY 32

using namespace testing;

namespace {
struct obj
{
    float x, y, z;
};

bool operator==(const obj& v1, const obj& v2)
{
    return v1.x == v2.x && v1.y == v2.y && v1.z == v2.z;
}

BMAP_DECLARE(obj_bmap, int16_t, struct obj, 16)
BMAP_DEFINE(obj_bmap, int16_t, struct obj, 16)
} // namespace

struct NAME : Test
{
    void SetUp() override { obj_bmap_init(&obj_bmap); }
    void TearDown() override { obj_bmap_deinit(obj_bmap); }

    struct obj_bmap* obj_bmap;
};

TEST_F(NAME, null_bmap_is_set)
{
    EXPECT_THAT(bmap_capacity(obj_bmap), Eq(0u));
    EXPECT_THAT(bmap_count(obj_bmap), Eq(0u));
}

TEST_F(NAME, deinit_null_bmap_works)
{
    obj_bmap_deinit(obj_bmap);
}

TEST_F(NAME, insertion_forwards)
{
    obj a = {56, 56, 56}, b = {45, 45, 45}, c = {18, 18, 18}, d = {27, 27, 27},
        e = {84, 84, 84};
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 0, a), Eq(BMAP_NEW));
    ASSERT_THAT(bmap_count(obj_bmap), Eq(1u));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 1, b), Eq(BMAP_NEW));
    ASSERT_THAT(bmap_count(obj_bmap), Eq(2u));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 2, c), Eq(BMAP_NEW));
    ASSERT_THAT(bmap_count(obj_bmap), Eq(3u));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 3, d), Eq(BMAP_NEW));
    ASSERT_THAT(bmap_count(obj_bmap), Eq(4u));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 4, e), Eq(BMAP_NEW));
    ASSERT_THAT(bmap_count(obj_bmap), Eq(5u));

    EXPECT_THAT(obj_bmap_find(obj_bmap, 0), Pointee(a));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 1), Pointee(b));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 2), Pointee(c));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 3), Pointee(d));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 4), Pointee(e));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 5), IsNull());
}

TEST_F(NAME, insertion_backwards)
{
    obj a = {56, 56, 56}, b = {45, 45, 45}, c = {18, 18, 18}, d = {27, 27, 27},
        e = {84, 84, 84};
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 4, a), Eq(BMAP_NEW));
    ASSERT_THAT(bmap_count(obj_bmap), Eq(1u));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 3, b), Eq(BMAP_NEW));
    ASSERT_THAT(bmap_count(obj_bmap), Eq(2u));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 2, c), Eq(BMAP_NEW));
    ASSERT_THAT(bmap_count(obj_bmap), Eq(3u));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 1, d), Eq(BMAP_NEW));
    ASSERT_THAT(bmap_count(obj_bmap), Eq(4u));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 0, e), Eq(BMAP_NEW));
    ASSERT_THAT(bmap_count(obj_bmap), Eq(5u));

    EXPECT_THAT(obj_bmap_find(obj_bmap, 0), Pointee(e));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 1), Pointee(d));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 2), Pointee(c));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 3), Pointee(b));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 4), Pointee(a));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 5), IsNull());
}

TEST_F(NAME, insertion_random)
{
    obj a = {56, 56, 56}, b = {45, 45, 45}, c = {18, 18, 18}, d = {27, 27, 27},
        e = {84, 84, 84};
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 26, a), Eq(BMAP_NEW));
    ASSERT_THAT(bmap_count(obj_bmap), Eq(1u));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 44, b), Eq(BMAP_NEW));
    ASSERT_THAT(bmap_count(obj_bmap), Eq(2u));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 82, c), Eq(BMAP_NEW));
    ASSERT_THAT(bmap_count(obj_bmap), Eq(3u));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 41, d), Eq(BMAP_NEW));
    ASSERT_THAT(bmap_count(obj_bmap), Eq(4u));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 70, e), Eq(BMAP_NEW));
    ASSERT_THAT(bmap_count(obj_bmap), Eq(5u));

    EXPECT_THAT(obj_bmap_find(obj_bmap, 26), Pointee(a));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 41), Pointee(d));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 44), Pointee(b));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 70), Pointee(e));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 82), Pointee(c));
}

TEST_F(NAME, insert_new_with_realloc_shifts_data_correctly)
{
    int16_t midway = BMAP_MIN_CAPACITY / 2;

    obj value = {0x55, 0x55, 0x55};
    for (int16_t i = 0; i != BMAP_MIN_CAPACITY; ++i)
    {
        if (i < midway)
            ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, i, value), Eq(BMAP_NEW));
        else
            ASSERT_THAT(
                obj_bmap_insert_new(&obj_bmap, i + 1, value), Eq(BMAP_NEW));
    }

    // Make sure we didn't cause a realloc yet
    ASSERT_THAT(bmap_capacity(obj_bmap), Eq(BMAP_MIN_CAPACITY));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, midway, value), Eq(BMAP_NEW));
    // Now it should have reallocated
    ASSERT_THAT(bmap_capacity(obj_bmap), Gt(BMAP_MIN_CAPACITY));

    // Check all values are there
    for (int i = 0; i != BMAP_MIN_CAPACITY + 1; ++i)
        EXPECT_THAT(obj_bmap_find(obj_bmap, i), NotNull())
            << "i: " << i << ", midway: " << midway;
}

TEST_F(NAME, clear_keeps_underlying_buffer)
{
    obj a = {53, 53, 53};
    obj_bmap_insert_new(&obj_bmap, 0, a);
    obj_bmap_insert_new(&obj_bmap, 1, a);
    obj_bmap_insert_new(&obj_bmap, 2, a);

    // this should delete all entries but keep the underlying buffer
    obj_bmap_clear(obj_bmap);

    EXPECT_THAT(bmap_count(obj_bmap), Eq(0u));
    EXPECT_THAT(bmap_capacity(obj_bmap), Ne(0u));
}

TEST_F(NAME, compact_when_no_buffer_is_allocated_doesnt_crash)
{
    obj_bmap_compact(&obj_bmap);
}

TEST_F(NAME, compact_reduces_capacity_and_keeps_elements_in_tact)
{
    obj a = {53, 53, 53};
    for (int i = 0; i != BMAP_MIN_CAPACITY * 3; ++i)
        ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, i, a), Eq(BMAP_NEW));
    for (int i = 0; i != BMAP_MIN_CAPACITY; ++i)
        obj_bmap_erase(obj_bmap, i);

    int16_t old_capacity = bmap_capacity(obj_bmap);
    obj_bmap_compact(&obj_bmap);
    EXPECT_THAT(bmap_capacity(obj_bmap), Lt(old_capacity));
    EXPECT_THAT(bmap_count(obj_bmap), Eq(BMAP_MIN_CAPACITY * 2));
    EXPECT_THAT(bmap_capacity(obj_bmap), Eq(BMAP_MIN_CAPACITY * 2));
}

TEST_F(NAME, clear_and_compact_deletes_underlying_buffer)
{
    obj a = {53, 53, 53};
    obj_bmap_insert_new(&obj_bmap, 0, a);
    obj_bmap_insert_new(&obj_bmap, 1, a);
    obj_bmap_insert_new(&obj_bmap, 2, a);

    // this should delete all entries + free the underlying buffer
    obj_bmap_clear(obj_bmap);
    obj_bmap_compact(&obj_bmap);

    ASSERT_THAT(bmap_count(obj_bmap), Eq(0u));
    ASSERT_THAT(bmap_capacity(obj_bmap), Eq(0u));
}

TEST_F(NAME, insert_new_existing_key_keeps_old_value)
{
    obj a = {53, 53, 53};
    obj b = {69, 69, 69};
    EXPECT_THAT(obj_bmap_insert_new(&obj_bmap, 0, a), Eq(BMAP_NEW));
    EXPECT_THAT(obj_bmap_insert_new(&obj_bmap, 0, b), Eq(BMAP_EXISTS));
    EXPECT_THAT(bmap_count(obj_bmap), Eq(1));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 0), Pointee(a));
}

TEST_F(NAME, insert_update_existing_key_updates_value)
{
    obj a = {53, 53, 53};
    obj b = {69, 69, 69};
    EXPECT_THAT(obj_bmap_insert_update(&obj_bmap, 0, a), Eq(BMAP_NEW));
    EXPECT_THAT(obj_bmap_insert_update(&obj_bmap, 0, b), Eq(BMAP_EXISTS));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 0), Pointee(b));
}

TEST_F(NAME, emplace_or_get_works)
{
    obj a = {53, 53, 53}, b = {77, 77, 77}, *get;
    get = NULL;
    EXPECT_THAT(obj_bmap_emplace_or_get(&obj_bmap, 0, &get), Eq(BMAP_NEW));
    *get = a;
    EXPECT_THAT(obj_bmap_find(obj_bmap, 0), Pointee(a));
    EXPECT_THAT(bmap_count(obj_bmap), Eq(1));

    get = NULL;
    EXPECT_THAT(obj_bmap_emplace_or_get(&obj_bmap, 1, &get), Eq(BMAP_NEW));
    *get = b;
    EXPECT_THAT(obj_bmap_find(obj_bmap, 0), Pointee(a));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 1), Pointee(b));
    EXPECT_THAT(bmap_count(obj_bmap), Eq(2));

    get = NULL;
    EXPECT_THAT(obj_bmap_emplace_or_get(&obj_bmap, 0, &get), Eq(BMAP_EXISTS));
    EXPECT_THAT(get, Pointee(a));
    EXPECT_THAT(bmap_count(obj_bmap), Eq(2));

    get = NULL;
    EXPECT_THAT(obj_bmap_emplace_or_get(&obj_bmap, 1, &get), Eq(BMAP_EXISTS));
    EXPECT_THAT(get, Pointee(b));
    EXPECT_THAT(bmap_count(obj_bmap), Eq(2));
}

TEST_F(NAME, insert_or_get_with_realloc_shifts_data_correctly)
{
    int16_t midway = BMAP_MIN_CAPACITY / 2;

    obj value = {0x55, 0x55, 0x55}, *get;
    for (int i = 0; i != BMAP_MIN_CAPACITY; ++i)
    {
        if (i < (int)midway)
        {
            ASSERT_THAT(
                obj_bmap_emplace_or_get(&obj_bmap, i, &get), Eq(BMAP_NEW));
            *get = value;
        }
        else
        {
            ASSERT_THAT(
                obj_bmap_emplace_or_get(&obj_bmap, i + 1, &get), Eq(BMAP_NEW));
            *get = value;
        }
    }

    // Make sure we didn't cause a realloc yet
    get = nullptr;
    ASSERT_THAT(bmap_capacity(obj_bmap), Eq(BMAP_MIN_CAPACITY));
    ASSERT_THAT(obj_bmap_emplace_or_get(&obj_bmap, midway, &get), Eq(BMAP_NEW));
    *get = value;
    // Now it should have reallocated
    ASSERT_THAT(bmap_capacity(obj_bmap), Gt(BMAP_MIN_CAPACITY));

    // Check all values are there
    for (int i = 0; i != BMAP_MIN_CAPACITY + 1; ++i)
    {
        get = nullptr;
        EXPECT_THAT(
            obj_bmap_emplace_or_get(&obj_bmap, i, &get), Eq(BMAP_EXISTS))
            << "i: " << i << ", midway: " << midway;
        EXPECT_THAT(get, Pointee(value));
    }
}

TEST_F(NAME, find_on_empty_bmap_doesnt_crash)
{
    EXPECT_THAT(obj_bmap_find(obj_bmap, 3), IsNull());
}

TEST_F(NAME, find_returns_false_if_key_doesnt_exist)
{
    obj a = {53, 53, 53}, b = {77, 77, 77}, c = {99, 99, 99};
    obj_bmap_insert_new(&obj_bmap, 3, a);
    obj_bmap_insert_new(&obj_bmap, 8, b);
    obj_bmap_insert_new(&obj_bmap, 2, c);

    EXPECT_THAT(obj_bmap_find(obj_bmap, 4), IsNull());
}

TEST_F(NAME, find_returns_value_if_key_exists)
{
    obj a = {53, 53, 53}, b = {77, 77, 77}, c = {99, 99, 99};
    obj_bmap_insert_new(&obj_bmap, 3, a);
    obj_bmap_insert_new(&obj_bmap, 8, b);
    obj_bmap_insert_new(&obj_bmap, 2, c);

    EXPECT_THAT(obj_bmap_find(obj_bmap, 3), Pointee(a));
}

TEST_F(NAME, erase)
{
    obj a = {56, 56, 56}, b = {45, 45, 45}, c = {18, 18, 18}, d = {27, 27, 27},
        e = {84, 84, 84};
    obj_bmap_insert_new(&obj_bmap, 0, a);
    obj_bmap_insert_new(&obj_bmap, 1, b);
    obj_bmap_insert_new(&obj_bmap, 2, c);
    obj_bmap_insert_new(&obj_bmap, 3, d);
    obj_bmap_insert_new(&obj_bmap, 4, e);

    EXPECT_THAT(obj_bmap_erase(obj_bmap, 2), Eq(1));
    EXPECT_THAT(obj_bmap_erase(obj_bmap, 2), Eq(0));

    // 4
    EXPECT_THAT(obj_bmap_find(obj_bmap, 0), Pointee(a));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 1), Pointee(b));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 2), IsNull());
    EXPECT_THAT(obj_bmap_find(obj_bmap, 3), Pointee(d));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 4), Pointee(e));

    EXPECT_THAT(obj_bmap_erase(obj_bmap, 4), Eq(1));
    EXPECT_THAT(obj_bmap_erase(obj_bmap, 4), Eq(0));

    // 3
    EXPECT_THAT(obj_bmap_find(obj_bmap, 0), Pointee(a));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 1), Pointee(b));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 2), IsNull());
    EXPECT_THAT(obj_bmap_find(obj_bmap, 3), Pointee(d));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 4), IsNull());

    EXPECT_THAT(obj_bmap_erase(obj_bmap, 0), Eq(1));
    EXPECT_THAT(obj_bmap_erase(obj_bmap, 0), Eq(0));

    // 2
    EXPECT_THAT(obj_bmap_find(obj_bmap, 0), IsNull());
    EXPECT_THAT(obj_bmap_find(obj_bmap, 1), Pointee(b));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 2), IsNull());
    EXPECT_THAT(obj_bmap_find(obj_bmap, 3), Pointee(d));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 4), IsNull());

    EXPECT_THAT(obj_bmap_erase(obj_bmap, 1), Eq(1));
    EXPECT_THAT(obj_bmap_erase(obj_bmap, 1), Eq(0));

    // 1
    EXPECT_THAT(obj_bmap_find(obj_bmap, 0), IsNull());
    EXPECT_THAT(obj_bmap_find(obj_bmap, 1), IsNull());
    EXPECT_THAT(obj_bmap_find(obj_bmap, 2), IsNull());
    EXPECT_THAT(obj_bmap_find(obj_bmap, 3), Pointee(d));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 4), IsNull());

    EXPECT_THAT(obj_bmap_erase(obj_bmap, 3), Eq(1));
    EXPECT_THAT(obj_bmap_erase(obj_bmap, 3), Eq(0));

    // 0
    EXPECT_THAT(obj_bmap_find(obj_bmap, 0), IsNull());
    EXPECT_THAT(obj_bmap_find(obj_bmap, 1), IsNull());
    EXPECT_THAT(obj_bmap_find(obj_bmap, 2), IsNull());
    EXPECT_THAT(obj_bmap_find(obj_bmap, 3), IsNull());
    EXPECT_THAT(obj_bmap_find(obj_bmap, 4), IsNull());
}

TEST_F(NAME, reinsertion_forwards)
{
    obj a = {56, 56, 56}, b = {45, 45, 45}, c = {18, 18, 18}, d = {27, 27, 27},
        e = {84, 84, 84};
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 0, a), Eq(BMAP_NEW));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 1, b), Eq(BMAP_NEW));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 2, c), Eq(BMAP_NEW));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 3, d), Eq(BMAP_NEW));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 4, e), Eq(BMAP_NEW));

    ASSERT_THAT(obj_bmap_erase(obj_bmap, 4), Eq(1));
    ASSERT_THAT(obj_bmap_erase(obj_bmap, 3), Eq(1));
    ASSERT_THAT(obj_bmap_erase(obj_bmap, 2), Eq(1));

    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 2, c), Eq(BMAP_NEW));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 3, d), Eq(BMAP_NEW));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 4, e), Eq(BMAP_NEW));

    EXPECT_THAT(obj_bmap_find(obj_bmap, 0), Pointee(a));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 1), Pointee(b));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 2), Pointee(c));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 3), Pointee(d));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 4), Pointee(e));
}

TEST_F(NAME, reinsertion_backwards)
{
    obj a = {56, 56, 56}, b = {45, 45, 45}, c = {18, 18, 18}, d = {27, 27, 27},
        e = {84, 84, 84};
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 0, a), Eq(BMAP_NEW));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 1, b), Eq(BMAP_NEW));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 2, c), Eq(BMAP_NEW));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 3, d), Eq(BMAP_NEW));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 4, e), Eq(BMAP_NEW));

    ASSERT_THAT(obj_bmap_erase(obj_bmap, 0), Eq(1));
    ASSERT_THAT(obj_bmap_erase(obj_bmap, 1), Eq(1));
    ASSERT_THAT(obj_bmap_erase(obj_bmap, 2), Eq(1));

    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 2, c), Eq(BMAP_NEW));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 1, b), Eq(BMAP_NEW));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 0, a), Eq(BMAP_NEW));

    EXPECT_THAT(obj_bmap_find(obj_bmap, 0), Pointee(a));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 1), Pointee(b));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 2), Pointee(c));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 3), Pointee(d));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 4), Pointee(e));
}

TEST_F(NAME, reinsertion_random)
{
    obj a = {56, 56, 56}, b = {45, 45, 45}, c = {18, 18, 18}, d = {27, 27, 27},
        e = {84, 84, 84};
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 26, a), Eq(BMAP_NEW));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 44, b), Eq(BMAP_NEW));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 82, c), Eq(BMAP_NEW));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 41, d), Eq(BMAP_NEW));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 70, e), Eq(BMAP_NEW));

    ASSERT_THAT(obj_bmap_erase(obj_bmap, 44), Eq(1));
    ASSERT_THAT(obj_bmap_erase(obj_bmap, 70), Eq(1));
    ASSERT_THAT(obj_bmap_erase(obj_bmap, 26), Eq(1));

    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 26, a), Eq(BMAP_NEW));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 70, e), Eq(BMAP_NEW));
    ASSERT_THAT(obj_bmap_insert_new(&obj_bmap, 44, b), Eq(BMAP_NEW));

    EXPECT_THAT(obj_bmap_find(obj_bmap, 26), Pointee(a));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 44), Pointee(b));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 82), Pointee(c));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 41), Pointee(d));
    EXPECT_THAT(obj_bmap_find(obj_bmap, 70), Pointee(e));
}

TEST_F(NAME, iterate_with_no_items)
{
    int16_t idx, key;
    obj*    val;
    int     counter = 0;
    bmap_for_each (obj_bmap, idx, key, val)
    {
        (void)idx;
        (void)key;
        (void)val;
        ++counter;
    }
    ASSERT_THAT(counter, Eq(0));
}

TEST_F(NAME, iterate_5_random_items)
{
    obj a = {79579, 79579, 79579}, b = {235, 235, 235}, c = {347, 347, 347},
        d = {124, 124, 124}, e = {457, 457, 457};
    obj_bmap_insert_new(&obj_bmap, 243, a);
    obj_bmap_insert_new(&obj_bmap, 256, b);
    obj_bmap_insert_new(&obj_bmap, 456, c);
    obj_bmap_insert_new(&obj_bmap, 468, d);
    obj_bmap_insert_new(&obj_bmap, 969, e);

    int16_t idx, key;
    obj*    val;
    int     counter = 0;
    bmap_for_each (obj_bmap, idx, key, val)
    {
        switch (counter)
        {
            case 0:
                ASSERT_THAT(key, Eq(243u));
                ASSERT_THAT(val, Pointee(a));
                break;
            case 1:
                ASSERT_THAT(key, Eq(256u));
                ASSERT_THAT(val, Pointee(b));
                break;
            case 2:
                ASSERT_THAT(key, Eq(456u));
                ASSERT_THAT(val, Pointee(c));
                break;
            case 3:
                ASSERT_THAT(key, Eq(468u));
                ASSERT_THAT(val, Pointee(d));
                break;
            case 4:
                ASSERT_THAT(key, Eq(969u));
                ASSERT_THAT(val, Pointee(e));
                break;
            default: ASSERT_FALSE(true); break;
        }
        ++counter;
    }
    ASSERT_THAT(counter, Eq(5));
}

TEST_F(NAME, retain_empty)
{
    int counter = 0;
    EXPECT_THAT(
        obj_bmap_retain(
            obj_bmap,
            [](int16_t, obj*, void* counter)
            {
                ++*(int*)counter;
                return BMAP_RETAIN;
            },
            &counter),
        Eq(0));
    EXPECT_THAT(bmap_count(obj_bmap), Eq(0));
    EXPECT_THAT(counter, Eq(0));
}

TEST_F(NAME, retain_all)
{
    for (int16_t i = 0; i != 8; ++i)
        obj_bmap_insert_new(&obj_bmap, i, obj{(float)i, (float)i, (float)i});

    int counter = 0;
    EXPECT_THAT(bmap_count(obj_bmap), Eq(8));
    EXPECT_THAT(
        obj_bmap_retain(
            obj_bmap,
            [](int16_t, obj*, void* counter)
            {
                ++*(int*)counter;
                return BMAP_RETAIN;
            },
            &counter),
        Eq(0));
    EXPECT_THAT(bmap_count(obj_bmap), Eq(8));
    EXPECT_THAT(counter, Eq(8));
}

TEST_F(NAME, retain_half)
{
    for (int16_t i = 0; i != 8; ++i)
        obj_bmap_insert_new(&obj_bmap, i, obj{(float)i, (float)i, (float)i});

    int counter = 0;
    EXPECT_THAT(bmap_count(obj_bmap), Eq(8));
    EXPECT_THAT(
        obj_bmap_retain(
            obj_bmap,
            [](int16_t, obj*, void* user) { return (*(int*)user)++ % 2; },
            &counter),
        Eq(0));
    EXPECT_THAT(bmap_count(obj_bmap), Eq(4));
    EXPECT_THAT(counter, Eq(8));
    EXPECT_THAT(
        obj_bmap_find(obj_bmap, 0), Pointee(obj{(float)0, (float)0, (float)0}));
    EXPECT_THAT(
        obj_bmap_find(obj_bmap, 2), Pointee(obj{(float)2, (float)2, (float)2}));
    EXPECT_THAT(
        obj_bmap_find(obj_bmap, 4), Pointee(obj{(float)4, (float)4, (float)4}));
    EXPECT_THAT(
        obj_bmap_find(obj_bmap, 6), Pointee(obj{(float)6, (float)6, (float)6}));
}

TEST_F(NAME, retain_returning_error)
{
    for (int16_t i = 0; i != 8; ++i)
        ASSERT_THAT(
            obj_bmap_insert_new(
                &obj_bmap, i, obj{(float)i, (float)i, (float)i}),
            Eq(BMAP_NEW));

    ASSERT_THAT(bmap_count(obj_bmap), Eq(8));
    EXPECT_THAT(
        obj_bmap_retain(
            obj_bmap, [](int16_t, obj*, void*) { return -5; }, NULL),
        Eq(-5));
    EXPECT_THAT(bmap_count(obj_bmap), Eq(8));
}
