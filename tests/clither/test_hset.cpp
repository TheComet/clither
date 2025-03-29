#include "gmock/gmock.h"

extern "C" {
#include "clither/util/hset.h"
}

#define NAME         hset
#define MIN_CAPACITY 128

using namespace testing;

namespace {
HSET_DECLARE(obj_hset, int16_t, 16)
HSET_DEFINE(obj_hset, int16_t, 16)
} // namespace

struct NAME : Test
{
    void SetUp() override { obj_hset_init(&obj_hset); }
    void TearDown() override { obj_hset_deinit(obj_hset); }

    struct obj_hset* obj_hset;
};

TEST_F(NAME, null_hset_is_set)
{
    EXPECT_THAT(hset_capacity(obj_hset), Eq(0u));
    EXPECT_THAT(hset_count(obj_hset), Eq(0u));
}

TEST_F(NAME, deinit_null_hset_works)
{
    obj_hset_deinit(obj_hset);
}

TEST_F(NAME, insertion_forwards)
{
    ASSERT_THAT(obj_hset_insert(&obj_hset, 0), Eq(HSET_NEW));
    ASSERT_THAT(hset_count(obj_hset), Eq(1u));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 1), Eq(HSET_NEW));
    ASSERT_THAT(hset_count(obj_hset), Eq(2u));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 2), Eq(HSET_NEW));
    ASSERT_THAT(hset_count(obj_hset), Eq(3u));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 3), Eq(HSET_NEW));
    ASSERT_THAT(hset_count(obj_hset), Eq(4u));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 4), Eq(HSET_NEW));
    ASSERT_THAT(hset_count(obj_hset), Eq(5u));

    EXPECT_THAT(obj_hset_find(obj_hset, 0), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 1), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 2), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 3), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 4), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 5), IsFalse());
}

TEST_F(NAME, insertion_backwards)
{
    ASSERT_THAT(obj_hset_insert(&obj_hset, 4), Eq(HSET_NEW));
    ASSERT_THAT(hset_count(obj_hset), Eq(1u));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 3), Eq(HSET_NEW));
    ASSERT_THAT(hset_count(obj_hset), Eq(2u));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 2), Eq(HSET_NEW));
    ASSERT_THAT(hset_count(obj_hset), Eq(3u));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 1), Eq(HSET_NEW));
    ASSERT_THAT(hset_count(obj_hset), Eq(4u));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 0), Eq(HSET_NEW));
    ASSERT_THAT(hset_count(obj_hset), Eq(5u));

    EXPECT_THAT(obj_hset_find(obj_hset, 0), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 1), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 2), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 3), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 4), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 5), IsFalse());
}

TEST_F(NAME, insertion_random)
{
    ASSERT_THAT(obj_hset_insert(&obj_hset, 26), Eq(HSET_NEW));
    ASSERT_THAT(hset_count(obj_hset), Eq(1u));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 44), Eq(HSET_NEW));
    ASSERT_THAT(hset_count(obj_hset), Eq(2u));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 82), Eq(HSET_NEW));
    ASSERT_THAT(hset_count(obj_hset), Eq(3u));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 41), Eq(HSET_NEW));
    ASSERT_THAT(hset_count(obj_hset), Eq(4u));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 70), Eq(HSET_NEW));
    ASSERT_THAT(hset_count(obj_hset), Eq(5u));

    EXPECT_THAT(obj_hset_find(obj_hset, 26), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 41), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 44), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 70), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 82), IsTrue());
}

TEST_F(NAME, insert_new_with_realloc_shifts_data_correctly)
{
    int16_t midway = MIN_CAPACITY / 2;

    for (int16_t i = 0; i != MIN_CAPACITY; ++i)
    {
        if (i < midway)
            ASSERT_THAT(obj_hset_insert(&obj_hset, i), Eq(HSET_NEW));
        else
            ASSERT_THAT(obj_hset_insert(&obj_hset, i + 1), Eq(HSET_NEW));
    }

    // Make sure we didn't cause a realloc yet
    ASSERT_THAT(hset_capacity(obj_hset), Eq(MIN_CAPACITY));
    ASSERT_THAT(obj_hset_insert(&obj_hset, midway), Eq(HSET_NEW));
    // Now it should have reallocated
    ASSERT_THAT(hset_capacity(obj_hset), Gt(MIN_CAPACITY));

    // Check all keys are there
    for (int i = 0; i != MIN_CAPACITY + 1; ++i)
        EXPECT_THAT(obj_hset_find(obj_hset, i), IsTrue())
            << "i: " << i << ", midway: " << midway;
}

TEST_F(NAME, clear_keeps_underlying_buffer)
{
    obj_hset_insert(&obj_hset, 0);
    obj_hset_insert(&obj_hset, 1);
    obj_hset_insert(&obj_hset, 2);

    // this should delete all entries but keep the underlying buffer
    obj_hset_clear(obj_hset);

    EXPECT_THAT(hset_count(obj_hset), Eq(0u));
    EXPECT_THAT(hset_capacity(obj_hset), Ne(0u));
}

// TODO:
// TEST_F(NAME, compact_when_no_buffer_is_allocated_doesnt_crash)
//{
//     obj_hset_compact(&obj_hset);
// }
//
// TEST_F(NAME, compact_reduces_capacity_and_keeps_elements_in_tact)
//{
//    for (int i = 0; i != MIN_CAPACITY * 3; ++i)
//        ASSERT_THAT(obj_hset_insert(&obj_hset, i), Eq(HSET_NEW));
//    for (int i = 0; i != MIN_CAPACITY; ++i)
//        obj_hset_erase(obj_hset, i);
//
//    int16_t old_capacity = hset_capacity(obj_hset);
//    obj_hset_compact(&obj_hset);
//    EXPECT_THAT(hset_capacity(obj_hset), Lt(old_capacity));
//    EXPECT_THAT(hset_count(obj_hset), Eq(MIN_CAPACITY * 2));
//    EXPECT_THAT(hset_capacity(obj_hset), Eq(MIN_CAPACITY * 2));
//}
//
// TEST_F(NAME, clear_and_compact_deletes_underlying_buffer)
//{
//    obj_hset_insert(&obj_hset, 0);
//    obj_hset_insert(&obj_hset, 1);
//    obj_hset_insert(&obj_hset, 2);
//
//    // this should delete all entries + free the underlying buffer
//    obj_hset_clear(obj_hset);
//    obj_hset_compact(&obj_hset);
//
//    ASSERT_THAT(hset_count(obj_hset), Eq(0u));
//    ASSERT_THAT(hset_capacity(obj_hset), Eq(0u));
//}

TEST_F(NAME, insert_key_twice_returns_exists)
{
    EXPECT_THAT(obj_hset_insert(&obj_hset, 0), Eq(HSET_NEW));
    EXPECT_THAT(obj_hset_insert(&obj_hset, 0), Eq(HSET_EXISTS));
    EXPECT_THAT(hset_count(obj_hset), Eq(1));
    EXPECT_THAT(obj_hset_find(obj_hset, 0), IsTrue());
}

TEST_F(NAME, insert_with_realloc_shifts_data_correctly)
{
    int16_t midway = MIN_CAPACITY / 2;

    for (int i = 0; i != MIN_CAPACITY; ++i)
    {
        if (i < (int)midway)
            ASSERT_THAT(obj_hset_insert(&obj_hset, i), Eq(HSET_NEW));
        else
            ASSERT_THAT(obj_hset_insert(&obj_hset, i + 1), Eq(HSET_NEW));
    }

    // Make sure we didn't cause a realloc yet
    ASSERT_THAT(hset_capacity(obj_hset), Eq(MIN_CAPACITY));
    ASSERT_THAT(obj_hset_insert(&obj_hset, midway), Eq(HSET_NEW));
    // Now it should have reallocated
    ASSERT_THAT(hset_capacity(obj_hset), Gt(MIN_CAPACITY));

    // Check all keys are there
    for (int i = 0; i != MIN_CAPACITY + 1; ++i)
    {
        EXPECT_THAT(obj_hset_find(obj_hset, i), IsTrue())
            << "i: " << i << ", midway: " << midway;
    }
}

TEST_F(NAME, find_on_empty_hset_doesnt_crash)
{
    EXPECT_THAT(obj_hset_find(obj_hset, 3), IsFalse());
}

TEST_F(NAME, find_returns_false_if_key_doesnt_exist)
{
    obj_hset_insert(&obj_hset, 3);
    obj_hset_insert(&obj_hset, 8);
    obj_hset_insert(&obj_hset, 2);

    EXPECT_THAT(obj_hset_find(obj_hset, 4), IsFalse());
}

TEST_F(NAME, find_returns_true_if_key_exists)
{
    obj_hset_insert(&obj_hset, 3);
    obj_hset_insert(&obj_hset, 8);
    obj_hset_insert(&obj_hset, 2);

    EXPECT_THAT(obj_hset_find(obj_hset, 3), IsTrue());
}

TEST_F(NAME, erase)
{
    obj_hset_insert(&obj_hset, 0);
    obj_hset_insert(&obj_hset, 1);
    obj_hset_insert(&obj_hset, 2);
    obj_hset_insert(&obj_hset, 3);
    obj_hset_insert(&obj_hset, 4);

    EXPECT_THAT(obj_hset_erase(obj_hset, 2), Eq(1));
    EXPECT_THAT(obj_hset_erase(obj_hset, 2), Eq(0));

    // 4
    EXPECT_THAT(obj_hset_find(obj_hset, 0), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 1), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 2), IsFalse());
    EXPECT_THAT(obj_hset_find(obj_hset, 3), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 4), IsTrue());

    EXPECT_THAT(obj_hset_erase(obj_hset, 4), Eq(1));
    EXPECT_THAT(obj_hset_erase(obj_hset, 4), Eq(0));

    // 3
    EXPECT_THAT(obj_hset_find(obj_hset, 0), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 1), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 2), IsFalse());
    EXPECT_THAT(obj_hset_find(obj_hset, 3), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 4), IsFalse());

    EXPECT_THAT(obj_hset_erase(obj_hset, 0), Eq(1));
    EXPECT_THAT(obj_hset_erase(obj_hset, 0), Eq(0));

    // 2
    EXPECT_THAT(obj_hset_find(obj_hset, 0), IsFalse());
    EXPECT_THAT(obj_hset_find(obj_hset, 1), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 2), IsFalse());
    EXPECT_THAT(obj_hset_find(obj_hset, 3), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 4), IsFalse());

    EXPECT_THAT(obj_hset_erase(obj_hset, 1), Eq(1));
    EXPECT_THAT(obj_hset_erase(obj_hset, 1), Eq(0));

    // 1
    EXPECT_THAT(obj_hset_find(obj_hset, 0), IsFalse());
    EXPECT_THAT(obj_hset_find(obj_hset, 1), IsFalse());
    EXPECT_THAT(obj_hset_find(obj_hset, 2), IsFalse());
    EXPECT_THAT(obj_hset_find(obj_hset, 3), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 4), IsFalse());

    EXPECT_THAT(obj_hset_erase(obj_hset, 3), Eq(1));
    EXPECT_THAT(obj_hset_erase(obj_hset, 3), Eq(0));

    // 0
    EXPECT_THAT(obj_hset_find(obj_hset, 0), IsFalse());
    EXPECT_THAT(obj_hset_find(obj_hset, 1), IsFalse());
    EXPECT_THAT(obj_hset_find(obj_hset, 2), IsFalse());
    EXPECT_THAT(obj_hset_find(obj_hset, 3), IsFalse());
    EXPECT_THAT(obj_hset_find(obj_hset, 4), IsFalse());
}

TEST_F(NAME, reinsertion_forwards)
{
    ASSERT_THAT(obj_hset_insert(&obj_hset, 0), Eq(HSET_NEW));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 1), Eq(HSET_NEW));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 2), Eq(HSET_NEW));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 3), Eq(HSET_NEW));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 4), Eq(HSET_NEW));

    ASSERT_THAT(obj_hset_erase(obj_hset, 4), Eq(1));
    ASSERT_THAT(obj_hset_erase(obj_hset, 3), Eq(1));
    ASSERT_THAT(obj_hset_erase(obj_hset, 2), Eq(1));

    ASSERT_THAT(obj_hset_insert(&obj_hset, 2), Eq(HSET_NEW));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 3), Eq(HSET_NEW));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 4), Eq(HSET_NEW));

    EXPECT_THAT(obj_hset_find(obj_hset, 0), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 1), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 2), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 3), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 4), IsTrue());
}

TEST_F(NAME, reinsertion_backwards)
{
    ASSERT_THAT(obj_hset_insert(&obj_hset, 0), Eq(HSET_NEW));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 1), Eq(HSET_NEW));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 2), Eq(HSET_NEW));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 3), Eq(HSET_NEW));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 4), Eq(HSET_NEW));

    ASSERT_THAT(obj_hset_erase(obj_hset, 0), Eq(1));
    ASSERT_THAT(obj_hset_erase(obj_hset, 1), Eq(1));
    ASSERT_THAT(obj_hset_erase(obj_hset, 2), Eq(1));

    ASSERT_THAT(obj_hset_insert(&obj_hset, 2), Eq(HSET_NEW));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 1), Eq(HSET_NEW));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 0), Eq(HSET_NEW));

    EXPECT_THAT(obj_hset_find(obj_hset, 0), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 1), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 2), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 3), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 4), IsTrue());
}

TEST_F(NAME, reinsertion_random)
{
    ASSERT_THAT(obj_hset_insert(&obj_hset, 26), Eq(HSET_NEW));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 44), Eq(HSET_NEW));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 82), Eq(HSET_NEW));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 41), Eq(HSET_NEW));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 70), Eq(HSET_NEW));

    ASSERT_THAT(obj_hset_erase(obj_hset, 44), Eq(1));
    ASSERT_THAT(obj_hset_erase(obj_hset, 70), Eq(1));
    ASSERT_THAT(obj_hset_erase(obj_hset, 26), Eq(1));

    ASSERT_THAT(obj_hset_insert(&obj_hset, 26), Eq(HSET_NEW));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 70), Eq(HSET_NEW));
    ASSERT_THAT(obj_hset_insert(&obj_hset, 44), Eq(HSET_NEW));

    EXPECT_THAT(obj_hset_find(obj_hset, 26), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 44), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 82), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 41), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 70), IsTrue());
}

TEST_F(NAME, iterate_with_no_items)
{
    int16_t idx, key;
    int     counter = 0;
    hset_for_each (obj_hset, idx, key)
    {
        (void)idx;
        (void)key;
        ++counter;
    }
    ASSERT_THAT(counter, Eq(0));
}

TEST_F(NAME, iterate_5_random_items)
{
    obj_hset_insert(&obj_hset, 243);
    obj_hset_insert(&obj_hset, 256);
    obj_hset_insert(&obj_hset, 456);
    obj_hset_insert(&obj_hset, 468);
    obj_hset_insert(&obj_hset, 969);

    int16_t idx, key;
    int     counter = 0;
    hset_for_each (obj_hset, idx, key)
    {
        switch (counter)
        {
            case 0: ASSERT_THAT(key, Eq(243u)); break;
            case 4: ASSERT_THAT(key, Eq(456u)); break;
            case 3: ASSERT_THAT(key, Eq(468u)); break;
            case 1: ASSERT_THAT(key, Eq(969u)); break;
            case 2: ASSERT_THAT(key, Eq(256u)); break;
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
        obj_hset_retain(
            obj_hset,
            [](int16_t, void* counter)
            {
                ++*(int*)counter;
                return HSET_RETAIN;
            },
            &counter),
        Eq(0));
    EXPECT_THAT(hset_count(obj_hset), Eq(0));
    EXPECT_THAT(counter, Eq(0));
}

TEST_F(NAME, retain_all)
{
    for (int16_t i = 0; i != 8; ++i)
        obj_hset_insert(&obj_hset, i);

    int counter = 0;
    EXPECT_THAT(hset_count(obj_hset), Eq(8));
    EXPECT_THAT(
        obj_hset_retain(
            obj_hset,
            [](int16_t, void* counter)
            {
                ++*(int*)counter;
                return HSET_RETAIN;
            },
            &counter),
        Eq(0));
    EXPECT_THAT(hset_count(obj_hset), Eq(8));
    EXPECT_THAT(counter, Eq(8));
}

TEST_F(NAME, retain_half)
{
    for (int16_t i = 0; i != 8; ++i)
        obj_hset_insert(&obj_hset, i);

    EXPECT_THAT(hset_count(obj_hset), Eq(8));
    EXPECT_THAT(
        obj_hset_retain(
            obj_hset, [](int16_t key, void*) { return key % 2; }, NULL),
        Eq(0));
    EXPECT_THAT(hset_count(obj_hset), Eq(4));
    EXPECT_THAT(obj_hset_find(obj_hset, 0), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 2), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 4), IsTrue());
    EXPECT_THAT(obj_hset_find(obj_hset, 6), IsTrue());
}

TEST_F(NAME, retain_returning_error)
{
    for (int16_t i = 0; i != 8; ++i)
        ASSERT_THAT(obj_hset_insert(&obj_hset, i), Eq(HSET_NEW));

    ASSERT_THAT(hset_count(obj_hset), Eq(8));
    EXPECT_THAT(
        obj_hset_retain(
            obj_hset, [](int16_t, void*) { return -5; }, NULL),
        Eq(-5));
    EXPECT_THAT(hset_count(obj_hset), Eq(8));
}
