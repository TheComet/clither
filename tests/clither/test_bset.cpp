#include "gmock/gmock.h"

extern "C" {
#include "clither/bset.h"
}

#define NAME              bset
#define BSET_MIN_CAPACITY 32

using namespace testing;

namespace {
BSET_DECLARE(obj_bset, int16_t, 16)
BSET_DEFINE(obj_bset, int16_t, 16)
} // namespace

struct NAME : Test
{
    void SetUp() override { obj_bset_init(&obj_bset); }
    void TearDown() override { obj_bset_deinit(obj_bset); }

    struct obj_bset* obj_bset;
};

TEST_F(NAME, null_bset_is_set)
{
    EXPECT_THAT(bset_capacity(obj_bset), Eq(0u));
    EXPECT_THAT(bset_count(obj_bset), Eq(0u));
}

TEST_F(NAME, deinit_null_bset_works)
{
    obj_bset_deinit(obj_bset);
}

TEST_F(NAME, insertion_forwards)
{
    ASSERT_THAT(obj_bset_insert(&obj_bset, 0), Eq(BSET_NEW));
    ASSERT_THAT(bset_count(obj_bset), Eq(1u));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 1), Eq(BSET_NEW));
    ASSERT_THAT(bset_count(obj_bset), Eq(2u));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 2), Eq(BSET_NEW));
    ASSERT_THAT(bset_count(obj_bset), Eq(3u));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 3), Eq(BSET_NEW));
    ASSERT_THAT(bset_count(obj_bset), Eq(4u));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 4), Eq(BSET_NEW));
    ASSERT_THAT(bset_count(obj_bset), Eq(5u));

    EXPECT_THAT(obj_bset_find(obj_bset, 0), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 1), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 2), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 3), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 4), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 5), IsFalse());
}

TEST_F(NAME, insertion_backwards)
{
    ASSERT_THAT(obj_bset_insert(&obj_bset, 4), Eq(BSET_NEW));
    ASSERT_THAT(bset_count(obj_bset), Eq(1u));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 3), Eq(BSET_NEW));
    ASSERT_THAT(bset_count(obj_bset), Eq(2u));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 2), Eq(BSET_NEW));
    ASSERT_THAT(bset_count(obj_bset), Eq(3u));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 1), Eq(BSET_NEW));
    ASSERT_THAT(bset_count(obj_bset), Eq(4u));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 0), Eq(BSET_NEW));
    ASSERT_THAT(bset_count(obj_bset), Eq(5u));

    EXPECT_THAT(obj_bset_find(obj_bset, 0), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 1), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 2), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 3), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 4), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 5), IsFalse());
}

TEST_F(NAME, insertion_random)
{
    ASSERT_THAT(obj_bset_insert(&obj_bset, 26), Eq(BSET_NEW));
    ASSERT_THAT(bset_count(obj_bset), Eq(1u));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 44), Eq(BSET_NEW));
    ASSERT_THAT(bset_count(obj_bset), Eq(2u));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 82), Eq(BSET_NEW));
    ASSERT_THAT(bset_count(obj_bset), Eq(3u));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 41), Eq(BSET_NEW));
    ASSERT_THAT(bset_count(obj_bset), Eq(4u));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 70), Eq(BSET_NEW));
    ASSERT_THAT(bset_count(obj_bset), Eq(5u));

    EXPECT_THAT(obj_bset_find(obj_bset, 26), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 41), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 44), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 70), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 82), IsTrue());
}

TEST_F(NAME, insert_new_with_realloc_shifts_data_correctly)
{
    int16_t midway = BSET_MIN_CAPACITY / 2;

    for (int16_t i = 0; i != BSET_MIN_CAPACITY; ++i)
    {
        if (i < midway)
            ASSERT_THAT(obj_bset_insert(&obj_bset, i), Eq(BSET_NEW));
        else
            ASSERT_THAT(obj_bset_insert(&obj_bset, i + 1), Eq(BSET_NEW));
    }

    // Make sure we didn't cause a realloc yet
    ASSERT_THAT(bset_capacity(obj_bset), Eq(BSET_MIN_CAPACITY));
    ASSERT_THAT(obj_bset_insert(&obj_bset, midway), Eq(BSET_NEW));
    // Now it should have reallocated
    ASSERT_THAT(bset_capacity(obj_bset), Gt(BSET_MIN_CAPACITY));

    // Check all keys are there
    for (int i = 0; i != BSET_MIN_CAPACITY + 1; ++i)
        EXPECT_THAT(obj_bset_find(obj_bset, i), IsTrue())
            << "i: " << i << ", midway: " << midway;
}

TEST_F(NAME, clear_keeps_underlying_buffer)
{
    obj_bset_insert(&obj_bset, 0);
    obj_bset_insert(&obj_bset, 1);
    obj_bset_insert(&obj_bset, 2);

    // this should delete all entries but keep the underlying buffer
    obj_bset_clear(obj_bset);

    EXPECT_THAT(bset_count(obj_bset), Eq(0u));
    EXPECT_THAT(bset_capacity(obj_bset), Ne(0u));
}

TEST_F(NAME, compact_when_no_buffer_is_allocated_doesnt_crash)
{
    obj_bset_compact(&obj_bset);
}

TEST_F(NAME, compact_reduces_capacity_and_keeps_elements_in_tact)
{
    for (int i = 0; i != BSET_MIN_CAPACITY * 3; ++i)
        ASSERT_THAT(obj_bset_insert(&obj_bset, i), Eq(BSET_NEW));
    for (int i = 0; i != BSET_MIN_CAPACITY; ++i)
        obj_bset_erase(obj_bset, i);

    int16_t old_capacity = bset_capacity(obj_bset);
    obj_bset_compact(&obj_bset);
    EXPECT_THAT(bset_capacity(obj_bset), Lt(old_capacity));
    EXPECT_THAT(bset_count(obj_bset), Eq(BSET_MIN_CAPACITY * 2));
    EXPECT_THAT(bset_capacity(obj_bset), Eq(BSET_MIN_CAPACITY * 2));
}

TEST_F(NAME, clear_and_compact_deletes_underlying_buffer)
{
    obj_bset_insert(&obj_bset, 0);
    obj_bset_insert(&obj_bset, 1);
    obj_bset_insert(&obj_bset, 2);

    // this should delete all entries + free the underlying buffer
    obj_bset_clear(obj_bset);
    obj_bset_compact(&obj_bset);

    ASSERT_THAT(bset_count(obj_bset), Eq(0u));
    ASSERT_THAT(bset_capacity(obj_bset), Eq(0u));
}

TEST_F(NAME, insert_key_twice_returns_exists)
{
    EXPECT_THAT(obj_bset_insert(&obj_bset, 0), Eq(BSET_NEW));
    EXPECT_THAT(obj_bset_insert(&obj_bset, 0), Eq(BSET_EXISTS));
    EXPECT_THAT(bset_count(obj_bset), Eq(1));
    EXPECT_THAT(obj_bset_find(obj_bset, 0), IsTrue());
}

TEST_F(NAME, insert_with_realloc_shifts_data_correctly)
{
    int16_t midway = BSET_MIN_CAPACITY / 2;

    for (int i = 0; i != BSET_MIN_CAPACITY; ++i)
    {
        if (i < (int)midway)
            ASSERT_THAT(obj_bset_insert(&obj_bset, i), Eq(BSET_NEW));
        else
            ASSERT_THAT(obj_bset_insert(&obj_bset, i + 1), Eq(BSET_NEW));
    }

    // Make sure we didn't cause a realloc yet
    ASSERT_THAT(bset_capacity(obj_bset), Eq(BSET_MIN_CAPACITY));
    ASSERT_THAT(obj_bset_insert(&obj_bset, midway), Eq(BSET_NEW));
    // Now it should have reallocated
    ASSERT_THAT(bset_capacity(obj_bset), Gt(BSET_MIN_CAPACITY));

    // Check all keys are there
    for (int i = 0; i != BSET_MIN_CAPACITY + 1; ++i)
    {
        EXPECT_THAT(obj_bset_find(obj_bset, i), IsTrue())
            << "i: " << i << ", midway: " << midway;
    }
}

TEST_F(NAME, find_on_empty_bset_doesnt_crash)
{
    EXPECT_THAT(obj_bset_find(obj_bset, 3), IsFalse());
}

TEST_F(NAME, find_returns_false_if_key_doesnt_exist)
{
    obj_bset_insert(&obj_bset, 3);
    obj_bset_insert(&obj_bset, 8);
    obj_bset_insert(&obj_bset, 2);

    EXPECT_THAT(obj_bset_find(obj_bset, 4), IsFalse());
}

TEST_F(NAME, find_returns_true_if_key_exists)
{
    obj_bset_insert(&obj_bset, 3);
    obj_bset_insert(&obj_bset, 8);
    obj_bset_insert(&obj_bset, 2);

    EXPECT_THAT(obj_bset_find(obj_bset, 3), IsTrue());
}

TEST_F(NAME, erase)
{
    obj_bset_insert(&obj_bset, 0);
    obj_bset_insert(&obj_bset, 1);
    obj_bset_insert(&obj_bset, 2);
    obj_bset_insert(&obj_bset, 3);
    obj_bset_insert(&obj_bset, 4);

    EXPECT_THAT(obj_bset_erase(obj_bset, 2), Eq(1));
    EXPECT_THAT(obj_bset_erase(obj_bset, 2), Eq(0));

    // 4
    EXPECT_THAT(obj_bset_find(obj_bset, 0), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 1), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 2), IsFalse());
    EXPECT_THAT(obj_bset_find(obj_bset, 3), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 4), IsTrue());

    EXPECT_THAT(obj_bset_erase(obj_bset, 4), Eq(1));
    EXPECT_THAT(obj_bset_erase(obj_bset, 4), Eq(0));

    // 3
    EXPECT_THAT(obj_bset_find(obj_bset, 0), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 1), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 2), IsFalse());
    EXPECT_THAT(obj_bset_find(obj_bset, 3), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 4), IsFalse());

    EXPECT_THAT(obj_bset_erase(obj_bset, 0), Eq(1));
    EXPECT_THAT(obj_bset_erase(obj_bset, 0), Eq(0));

    // 2
    EXPECT_THAT(obj_bset_find(obj_bset, 0), IsFalse());
    EXPECT_THAT(obj_bset_find(obj_bset, 1), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 2), IsFalse());
    EXPECT_THAT(obj_bset_find(obj_bset, 3), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 4), IsFalse());

    EXPECT_THAT(obj_bset_erase(obj_bset, 1), Eq(1));
    EXPECT_THAT(obj_bset_erase(obj_bset, 1), Eq(0));

    // 1
    EXPECT_THAT(obj_bset_find(obj_bset, 0), IsFalse());
    EXPECT_THAT(obj_bset_find(obj_bset, 1), IsFalse());
    EXPECT_THAT(obj_bset_find(obj_bset, 2), IsFalse());
    EXPECT_THAT(obj_bset_find(obj_bset, 3), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 4), IsFalse());

    EXPECT_THAT(obj_bset_erase(obj_bset, 3), Eq(1));
    EXPECT_THAT(obj_bset_erase(obj_bset, 3), Eq(0));

    // 0
    EXPECT_THAT(obj_bset_find(obj_bset, 0), IsFalse());
    EXPECT_THAT(obj_bset_find(obj_bset, 1), IsFalse());
    EXPECT_THAT(obj_bset_find(obj_bset, 2), IsFalse());
    EXPECT_THAT(obj_bset_find(obj_bset, 3), IsFalse());
    EXPECT_THAT(obj_bset_find(obj_bset, 4), IsFalse());
}

TEST_F(NAME, reinsertion_forwards)
{
    ASSERT_THAT(obj_bset_insert(&obj_bset, 0), Eq(BSET_NEW));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 1), Eq(BSET_NEW));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 2), Eq(BSET_NEW));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 3), Eq(BSET_NEW));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 4), Eq(BSET_NEW));

    ASSERT_THAT(obj_bset_erase(obj_bset, 4), Eq(1));
    ASSERT_THAT(obj_bset_erase(obj_bset, 3), Eq(1));
    ASSERT_THAT(obj_bset_erase(obj_bset, 2), Eq(1));

    ASSERT_THAT(obj_bset_insert(&obj_bset, 2), Eq(BSET_NEW));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 3), Eq(BSET_NEW));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 4), Eq(BSET_NEW));

    EXPECT_THAT(obj_bset_find(obj_bset, 0), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 1), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 2), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 3), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 4), IsTrue());
}

TEST_F(NAME, reinsertion_backwards)
{
    ASSERT_THAT(obj_bset_insert(&obj_bset, 0), Eq(BSET_NEW));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 1), Eq(BSET_NEW));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 2), Eq(BSET_NEW));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 3), Eq(BSET_NEW));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 4), Eq(BSET_NEW));

    ASSERT_THAT(obj_bset_erase(obj_bset, 0), Eq(1));
    ASSERT_THAT(obj_bset_erase(obj_bset, 1), Eq(1));
    ASSERT_THAT(obj_bset_erase(obj_bset, 2), Eq(1));

    ASSERT_THAT(obj_bset_insert(&obj_bset, 2), Eq(BSET_NEW));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 1), Eq(BSET_NEW));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 0), Eq(BSET_NEW));

    EXPECT_THAT(obj_bset_find(obj_bset, 0), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 1), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 2), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 3), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 4), IsTrue());
}

TEST_F(NAME, reinsertion_random)
{
    ASSERT_THAT(obj_bset_insert(&obj_bset, 26), Eq(BSET_NEW));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 44), Eq(BSET_NEW));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 82), Eq(BSET_NEW));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 41), Eq(BSET_NEW));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 70), Eq(BSET_NEW));

    ASSERT_THAT(obj_bset_erase(obj_bset, 44), Eq(1));
    ASSERT_THAT(obj_bset_erase(obj_bset, 70), Eq(1));
    ASSERT_THAT(obj_bset_erase(obj_bset, 26), Eq(1));

    ASSERT_THAT(obj_bset_insert(&obj_bset, 26), Eq(BSET_NEW));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 70), Eq(BSET_NEW));
    ASSERT_THAT(obj_bset_insert(&obj_bset, 44), Eq(BSET_NEW));

    EXPECT_THAT(obj_bset_find(obj_bset, 26), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 44), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 82), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 41), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 70), IsTrue());
}

TEST_F(NAME, iterate_with_no_items)
{
    int16_t idx, key;
    int     counter = 0;
    bset_for_each (obj_bset, idx, key)
    {
        (void)idx;
        (void)key;
        ++counter;
    }
    ASSERT_THAT(counter, Eq(0));
}

TEST_F(NAME, iterate_5_random_items)
{
    obj_bset_insert(&obj_bset, 243);
    obj_bset_insert(&obj_bset, 256);
    obj_bset_insert(&obj_bset, 456);
    obj_bset_insert(&obj_bset, 468);
    obj_bset_insert(&obj_bset, 969);

    int16_t idx, key;
    int     counter = 0;
    bset_for_each (obj_bset, idx, key)
    {
        switch (counter)
        {
            case 0: ASSERT_THAT(key, Eq(243u)); break;
            case 1: ASSERT_THAT(key, Eq(256u)); break;
            case 2: ASSERT_THAT(key, Eq(456u)); break;
            case 3: ASSERT_THAT(key, Eq(468u)); break;
            case 4: ASSERT_THAT(key, Eq(969u)); break;
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
        obj_bset_retain(
            obj_bset,
            [](int16_t, void* counter)
            {
                ++*(int*)counter;
                return BSET_RETAIN;
            },
            &counter),
        Eq(0));
    EXPECT_THAT(bset_count(obj_bset), Eq(0));
    EXPECT_THAT(counter, Eq(0));
}

TEST_F(NAME, retain_all)
{
    for (int16_t i = 0; i != 8; ++i)
        obj_bset_insert(&obj_bset, i);

    int counter = 0;
    EXPECT_THAT(bset_count(obj_bset), Eq(8));
    EXPECT_THAT(
        obj_bset_retain(
            obj_bset,
            [](int16_t, void* counter)
            {
                ++*(int*)counter;
                return BSET_RETAIN;
            },
            &counter),
        Eq(0));
    EXPECT_THAT(bset_count(obj_bset), Eq(8));
    EXPECT_THAT(counter, Eq(8));
}

TEST_F(NAME, retain_half)
{
    for (int16_t i = 0; i != 8; ++i)
        obj_bset_insert(&obj_bset, i);

    int counter = 0;
    EXPECT_THAT(bset_count(obj_bset), Eq(8));
    EXPECT_THAT(
        obj_bset_retain(
            obj_bset,
            [](int16_t, void* user) { return (*(int*)user)++ % 2; },
            &counter),
        Eq(0));
    EXPECT_THAT(bset_count(obj_bset), Eq(4));
    EXPECT_THAT(counter, Eq(8));
    EXPECT_THAT(obj_bset_find(obj_bset, 0), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 2), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 4), IsTrue());
    EXPECT_THAT(obj_bset_find(obj_bset, 6), IsTrue());
}

TEST_F(NAME, retain_returning_error)
{
    for (int16_t i = 0; i != 8; ++i)
        ASSERT_THAT(obj_bset_insert(&obj_bset, i), Eq(BSET_NEW));

    ASSERT_THAT(bset_count(obj_bset), Eq(8));
    EXPECT_THAT(
        obj_bset_retain(
            obj_bset, [](int16_t, void*) { return -5; }, NULL),
        Eq(-5));
    EXPECT_THAT(bset_count(obj_bset), Eq(8));
}
