#include "gmock/gmock.h"

extern "C" {
#include "clither/btree.h"
}

#define NAME               btree
#define BTREE_MIN_CAPACITY 32

using namespace testing;

namespace {
struct value
{
    float x, y, z;
};

BTREE_DECLARE(btree_test, int16_t, struct value, 16)
BTREE_DEFINE(btree_test, int16_t, struct value, 16)
} // namespace

struct NAME : Test
{
    void SetUp() override { btree_test_init(&btree_test); }
    void TearDown() override { btree_test_deinit(btree_test); }

    struct btree_test* btree_test;
};

TEST_F(NAME, null_btree_is_set)
{
    EXPECT_THAT(btree_capacity(btree_test), Eq(0u));
    EXPECT_THAT(btree_count(btree_test), Eq(0u));
}

TEST_F(NAME, deinit_null_btree_works)
{
    btree_test_deinit(btree_test);
}

TEST_F(NAME, insertion_forwards)
{
    value a = {56, 56, 56}, b = {45, 45, 45}, c = {18, 18, 18},
          d = {27, 27, 27}, e = {84, 84, 84};
    ASSERT_THAT(btree_test_insert_new(&btree_test, 0, a), Eq(0));
    ASSERT_THAT(btree_count(btree_test), Eq(1u));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 1, b), Eq(0));
    ASSERT_THAT(btree_count(btree_test), Eq(2u));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 2, c), Eq(0));
    ASSERT_THAT(btree_count(btree_test), Eq(3u));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 3, d), Eq(0));
    ASSERT_THAT(btree_count(btree_test), Eq(4u));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 4, e), Eq(0));
    ASSERT_THAT(btree_count(btree_test), Eq(5u));

    EXPECT_THAT(btree_test_find(btree_test, 0), Eq(a));
    EXPECT_THAT(btree_test_find(btree_test, 1), Eq(b));
    EXPECT_THAT(btree_test_find(btree_test, 2), Eq(c));
    EXPECT_THAT(btree_test_find(btree_test, 3), Eq(d));
    EXPECT_THAT(btree_test_find(btree_test, 4), Eq(e));
    EXPECT_THAT(btree_test_find(btree_test, 5), IsNull());
}

TEST_F(NAME, insertion_backwards)
{
    value a = {56, 56, 56}, b = {45, 45, 45}, c = {18, 18, 18},
          d = {27, 27, 27}, e = {84, 84, 84};
    ASSERT_THAT(btree_test_insert_new(&btree_test, 4, a), Eq(0));
    ASSERT_THAT(btree_count(btree_test), Eq(1u));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 3, b), Eq(0));
    ASSERT_THAT(btree_count(btree_test), Eq(2u));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 2, c), Eq(0));
    ASSERT_THAT(btree_count(btree_test), Eq(3u));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 1, d), Eq(0));
    ASSERT_THAT(btree_count(btree_test), Eq(4u));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 0, e), Eq(0));
    ASSERT_THAT(btree_count(btree_test), Eq(5u));

    EXPECT_THAT(btree_test_find(btree_test, 0), Eq(e));
    EXPECT_THAT(btree_test_find(btree_test, 1), Eq(d));
    EXPECT_THAT(btree_test_find(btree_test, 2), Eq(c));
    EXPECT_THAT(btree_test_find(btree_test, 3), Eq(b));
    EXPECT_THAT(btree_test_find(btree_test, 4), Eq(a));
    EXPECT_THAT(btree_test_find(btree_test, 5), IsNull());
}

TEST_F(NAME, insertion_random)
{
    value a = {56, 56, 56}, b = {45, 45, 45}, c = {18, 18, 18},
          d = {27, 27, 27}, e = {84, 84, 84};
    ASSERT_THAT(btree_test_insert_new(&btree_test, 26, a), Eq(0));
    ASSERT_THAT(btree_count(btree_test), Eq(1u));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 44, b), Eq(0));
    ASSERT_THAT(btree_count(btree_test), Eq(2u));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 82, c), Eq(0));
    ASSERT_THAT(btree_count(btree_test), Eq(3u));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 41, d), Eq(0));
    ASSERT_THAT(btree_count(btree_test), Eq(4u));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 70, e), Eq(0));
    ASSERT_THAT(btree_count(btree_test), Eq(5u));

    EXPECT_THAT(btree_test_find(btree_test, 26), Eq(a));
    EXPECT_THAT(btree_test_find(btree_test, 41), Eq(d));
    EXPECT_THAT(btree_test_find(btree_test, 44), Eq(b));
    EXPECT_THAT(btree_test_find(btree_test, 70), Eq(e));
    EXPECT_THAT(btree_test_find(btree_test, 82), Eq(c));
}

TEST_F(NAME, insert_new_with_realloc_shifts_data_correctly)
{
    int16_t midway = BTREE_MIN_CAPACITY / 2;

    value value = {0x55, 0x55, 0x55};
    for (int16_t i = 0; i != BTREE_MIN_CAPACITY; ++i)
    {
        if (i < midway)
            ASSERT_THAT(btree_test_insert_new(&btree_test, i, value), Eq(0));
        else
            ASSERT_THAT(
                btree_test_insert_new(&btree_test, i + 1, value), Eq(0));
    }

    // Make sure we didn't cause a realloc yet
    ASSERT_THAT(btree_capacity(btree_test), Eq(BTREE_MIN_CAPACITY));
    ASSERT_THAT(btree_test_insert_new(&btree_test, midway, value), Eq(0));
    // Now it should have reallocated
    ASSERT_THAT(btree_capacity(btree_test), Gt(BTREE_MIN_CAPACITY));

    // Check all values are there
    for (int i = 0; i != BTREE_MIN_CAPACITY + 1; ++i)
        EXPECT_THAT(btree_test_find(btree_test, i), NotNull())
            << "i: " << i << ", midway: " << midway;
}

TEST_F(NAME, clear_keeps_underlying_buffer)
{
    value a = {53, 53, 53};
    btree_test_insert_new(&btree_test, 0, a);
    btree_test_insert_new(&btree_test, 1, a);
    btree_test_insert_new(&btree_test, 2, a);

    // this should delete all entries but keep the underlying buffer
    btree_test_clear(btree_test);

    EXPECT_THAT(btree_count(btree_test), Eq(0u));
    EXPECT_THAT(btree_capacity(btree_test), Ne(0u));
}

TEST_F(NAME, compact_when_no_buffer_is_allocated_doesnt_crash)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));
    btree_compact(&btree_test);
    btree_deinit(&btree_test);
}

TEST_F(NAME, compact_reduces_capacity_and_keeps_elements_in_tact)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 53;
    for (int i = 0; i != BTREE_MIN_CAPACITY * 3; ++i)
        ASSERT_THAT(btree_test_insert_new(&btree_test, i, &a), Eq(0));
    for (int i = 0; i != BTREE_MIN_CAPACITY; ++i)
        btree_erase(&btree_test, i);

    cs_btree_size old_capacity = btree_capacity(&btree_test);
    btree_compact(&btree_test);
    EXPECT_THAT(btree_capacity(&btree_test), Lt(old_capacity));
    EXPECT_THAT(btree_count(btree), Eq(BTREE_MIN_CAPACITY * 2));
    EXPECT_THAT(btree_capacity(&btree_test), Eq(BTREE_MIN_CAPACITY * 2));
    EXPECT_THAT(btree.data, NotNull());

    btree_deinit(&btree_test);
}

TEST_F(NAME, clear_and_compact_deletes_underlying_buffer)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 53;
    btree_test_insert_new(&btree_test, 0, &a);
    btree_test_insert_new(&btree_test, 1, &a);
    btree_test_insert_new(&btree_test, 2, &a);

    // this should delete all entries + free the underlying buffer
    btree_clear(&btree_test);
    btree_compact(&btree_test);

    ASSERT_THAT(btree_count(btree), Eq(0u));
    ASSERT_THAT(btree.data, IsNull());
    ASSERT_THAT(btree_capacity(&btree_test), Eq(0u));

    btree_deinit(&btree_test);
}

TEST_F(NAME, insert_new_existing_keys_fails)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 53;
    EXPECT_THAT(btree_test_insert_new(&btree_test, 0, &a), Eq(0));
    EXPECT_THAT(btree_test_insert_new(&btree_test, 0, &a), Eq(BTREE_EXISTS));

    btree_deinit(&btree_test);
}

TEST_F(NAME, set_existing_fails_if_key_doesnt_exist)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 53;
    EXPECT_THAT(btree_set_existing(&btree_test, 0, &a), Eq(BTREE_NOT_FOUND));

    btree_deinit(&btree_test);
}

TEST_F(NAME, set_existing_key_works_on_keys_that_exist)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 53, b = 77, c = 99, d = 12;
    EXPECT_THAT(btree_test_insert_new(&btree_test, 7, &a), Eq(0));
    EXPECT_THAT(btree_test_insert_new(&btree_test, 3, &b), Eq(0));
    EXPECT_THAT(btree_set_existing(&btree_test, 3, &d), Eq(0));
    EXPECT_THAT(btree_set_existing(&btree_test, 7, &c), Eq(0));
    EXPECT_THAT(*static_cast<int*>(btree_test_find(btree, 3)), Eq(12));
    EXPECT_THAT(*static_cast<int*>(btree_test_find(btree, 7)), Eq(99));

    btree_deinit(&btree_test);
}

TEST_F(NAME, insert_or_get_works)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 53, b = 77, *get;
    EXPECT_THAT(
        btree_test_insert_or_get(&btree_test, 0, &a, (void**)&get),
        Eq(BTREE_NOT_FOUND));
    EXPECT_THAT(get, AllOf(NotNull(), Pointee(Eq(53))));
    EXPECT_THAT(
        static_cast<int*>(btree_test_find(btree, 0)),
        AllOf(NotNull(), Pointee(Eq(53))));
    EXPECT_THAT(btree_count(btree), Eq(1));

    EXPECT_THAT(
        btree_test_insert_or_get(&btree_test, 1, &b, (void**)&get),
        Eq(BTREE_NOT_FOUND));
    EXPECT_THAT(get, AllOf(NotNull(), Pointee(Eq(77))));
    EXPECT_THAT(
        static_cast<int*>(btree_test_find(btree, 1)),
        AllOf(NotNull(), Pointee(Eq(77))));
    EXPECT_THAT(btree_count(btree), Eq(2));

    EXPECT_THAT(
        btree_test_insert_or_get(&btree_test, 0, &a, (void**)&get),
        Eq(BTREE_EXISTS));
    EXPECT_THAT(get, AllOf(NotNull(), Pointee(Eq(53))));
    EXPECT_THAT(btree_count(btree), Eq(2));

    EXPECT_THAT(
        btree_test_insert_or_get(&btree_test, 1, &b, (void**)&get),
        Eq(BTREE_EXISTS));
    EXPECT_THAT(get, AllOf(NotNull(), Pointee(Eq(77))));
    EXPECT_THAT(btree_count(btree), Eq(2));

    btree_deinit(&btree_test);
}

TEST_F(NAME, insert_or_get_with_realloc_shifts_data_correctly)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    cs_btree_size midway = BTREE_MIN_CAPACITY / 2;

    int value = 0x55, *get;
    btree_reserve(&btree_test, BTREE_MIN_CAPACITY);
    for (int i = 0; i != BTREE_MIN_CAPACITY; ++i)
    {
        if (i < (int)midway)
            ASSERT_THAT(
                btree_test_insert_or_get(&btree_test, i, &value, (void**)&get),
                Eq(BTREE_NOT_FOUND));
        else
            ASSERT_THAT(
                btree_test_insert_or_get(
                    &btree_test, i + 1, &value, (void**)&get),
                Eq(BTREE_NOT_FOUND));
    }

    // Make sure we didn't cause a realloc yet
    get = nullptr;
    ASSERT_THAT(btree_capacity(&btree_test), Eq(BTREE_MIN_CAPACITY));
    ASSERT_THAT(
        btree_test_insert_or_get(&btree_test, midway, &value, (void**)&get),
        Eq(BTREE_NOT_FOUND));
    // Now it should have reallocated
    ASSERT_THAT(btree_capacity(&btree_test), Gt(BTREE_MIN_CAPACITY));

    // Check all values are there
    for (int i = 0; i != BTREE_MIN_CAPACITY + 1; ++i)
    {
        get = nullptr;
        EXPECT_THAT(
            btree_test_insert_or_get(&btree_test, i, &value, (void**)&get),
            Eq(BTREE_EXISTS))
            << "i: " << i << ", midway: " << midway;
        EXPECT_THAT(get, AllOf(NotNull(), Pointee(Eq(0x55))));
    }

    btree_deinit(&btree_test);
}

TEST_F(NAME, find_on_empty_btree_doesnt_crash)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    EXPECT_THAT(btree_test_find(btree, 3), IsNull());

    btree_deinit(&btree_test);
}

TEST_F(NAME, find_key_on_empty_btree_doesnt_crash)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int value = 55;
    EXPECT_THAT(btree_find_key(&btree_test, &value), IsNull());

    btree_deinit(&btree_test);
}

TEST_F(NAME, find_and_compare_on_empty_btree_doesnt_crash)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int value = 55;
    EXPECT_THAT(btree_find_and_compare(&btree_test, 34, &value), IsFalse());

    btree_deinit(&btree_test);
}

TEST_F(NAME, search_for_key_using_existing_value_works)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 53, b = 77, c = 99;
    btree_test_insert_new(&btree_test, 3, &a);
    btree_test_insert_new(&btree_test, 8, &b);
    btree_test_insert_new(&btree_test, 2, &c);

    EXPECT_THAT(
        btree_find_key(&btree_test, &a), AllOf(NotNull(), Pointee(Eq(3))));
    EXPECT_THAT(
        btree_find_key(&btree_test, &b), AllOf(NotNull(), Pointee(Eq(8))));
    EXPECT_THAT(
        btree_find_key(&btree_test, &c), AllOf(NotNull(), Pointee(Eq(2))));

    btree_deinit(&btree_test);
}

TEST_F(NAME, search_for_key_using_non_existing_value_returns_null)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 53, b = 77, c = 99;
    btree_test_insert_new(&btree_test, 3, &a);
    btree_test_insert_new(&btree_test, 8, &b);
    btree_test_insert_new(&btree_test, 2, &c);

    int d = 12;
    EXPECT_THAT(btree_find_key(&btree_test, &d), IsNull());

    btree_deinit(&btree_test);
}

TEST_F(NAME, find_and_compare_non_existing_key_returns_false)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 53, b = 77, c = 99;
    btree_test_insert_new(&btree_test, 3, &a);
    btree_test_insert_new(&btree_test, 8, &b);
    btree_test_insert_new(&btree_test, 2, &c);

    int d = 53;
    EXPECT_THAT(btree_find_and_compare(&btree_test, 4, &d), IsFalse());

    btree_deinit(&btree_test);
}

TEST_F(NAME, find_and_compare_existing_key_and_different_value_returns_false)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 53, b = 77, c = 99;
    btree_test_insert_new(&btree_test, 3, &a);
    btree_test_insert_new(&btree_test, 8, &b);
    btree_test_insert_new(&btree_test, 2, &c);

    int d = 53;
    EXPECT_THAT(btree_find_and_compare(&btree_test, 8, &d), IsFalse());

    btree_deinit(&btree_test);
}

TEST_F(NAME, find_and_compare_existing_key_and_matching_value_returns_true)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 53, b = 77, c = 99;
    btree_test_insert_new(&btree_test, 3, &a);
    btree_test_insert_new(&btree_test, 8, &b);
    btree_test_insert_new(&btree_test, 2, &c);

    int d = 53;
    EXPECT_THAT(btree_find_and_compare(&btree_test, 3, &d), IsTrue());

    btree_deinit(&btree_test);
}

TEST_F(NAME, key_exists_returns_false_if_key_doesnt_exist)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 53, b = 77, c = 99;
    btree_test_insert_new(&btree_test, 3, &a);
    btree_test_insert_new(&btree_test, 8, &b);
    btree_test_insert_new(&btree_test, 2, &c);

    EXPECT_THAT(btree_key_exists(&btree_test, 4), IsFalse());

    btree_deinit(&btree_test);
}

TEST_F(NAME, key_exists_returns_true_if_key_does_exist)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 53, b = 77, c = 99;
    btree_test_insert_new(&btree_test, 3, &a);
    btree_test_insert_new(&btree_test, 8, &b);
    btree_test_insert_new(&btree_test, 2, &c);

    EXPECT_THAT(btree_key_exists(&btree_test, 3), IsTrue());

    btree_deinit(&btree_test);
}

TEST_F(NAME, erase_by_key)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 56, b = 45, c = 18, d = 27, e = 84;
    btree_test_insert_new(&btree_test, 0, &a);
    btree_test_insert_new(&btree_test, 1, &b);
    btree_test_insert_new(&btree_test, 2, &c);
    btree_test_insert_new(&btree_test, 3, &d);
    btree_test_insert_new(&btree_test, 4, &e);

    EXPECT_THAT(btree_erase(&btree_test, 2), Eq(0));

    // 4
    EXPECT_THAT(
        (int*)btree_test_find(btree, 0), AllOf(NotNull(), Pointee(Eq(a))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 1), AllOf(NotNull(), Pointee(Eq(b))));
    EXPECT_THAT((int*)btree_test_find(btree, 2), IsNull());
    EXPECT_THAT(
        (int*)btree_test_find(btree, 3), AllOf(NotNull(), Pointee(Eq(d))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 4), AllOf(NotNull(), Pointee(Eq(e))));

    EXPECT_THAT(btree_erase(&btree_test, 4), Eq(0));

    // 3
    EXPECT_THAT(
        (int*)btree_test_find(btree, 0), AllOf(NotNull(), Pointee(Eq(a))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 1), AllOf(NotNull(), Pointee(Eq(b))));
    EXPECT_THAT((int*)btree_test_find(btree, 2), IsNull());
    EXPECT_THAT(
        (int*)btree_test_find(btree, 3), AllOf(NotNull(), Pointee(Eq(d))));
    EXPECT_THAT((int*)btree_test_find(btree, 4), IsNull());

    EXPECT_THAT(btree_erase(&btree_test, 0), Eq(0));

    // 2
    EXPECT_THAT((int*)btree_test_find(btree, 0), IsNull());
    EXPECT_THAT(
        (int*)btree_test_find(btree, 1), AllOf(NotNull(), Pointee(Eq(b))));
    EXPECT_THAT((int*)btree_test_find(btree, 2), IsNull());
    EXPECT_THAT(
        (int*)btree_test_find(btree, 3), AllOf(NotNull(), Pointee(Eq(d))));
    EXPECT_THAT((int*)btree_test_find(btree, 4), IsNull());

    EXPECT_THAT(btree_erase(&btree_test, 1), Eq(0));

    // 1
    EXPECT_THAT((int*)btree_test_find(btree, 0), IsNull());
    EXPECT_THAT((int*)btree_test_find(btree, 1), IsNull());
    EXPECT_THAT((int*)btree_test_find(btree, 2), IsNull());
    EXPECT_THAT(
        (int*)btree_test_find(btree, 3), AllOf(NotNull(), Pointee(Eq(d))));
    EXPECT_THAT((int*)btree_test_find(btree, 4), IsNull());

    EXPECT_THAT(btree_erase(&btree_test, 3), Eq(0));

    // 0
    EXPECT_THAT((int*)btree_test_find(btree, 0), IsNull());
    EXPECT_THAT((int*)btree_test_find(btree, 1), IsNull());
    EXPECT_THAT((int*)btree_test_find(btree, 2), IsNull());
    EXPECT_THAT((int*)btree_test_find(btree, 3), IsNull());
    EXPECT_THAT((int*)btree_test_find(btree, 4), IsNull());

    EXPECT_THAT(btree_erase(&btree_test, 0), Eq(BTREE_NOT_FOUND));
    EXPECT_THAT(btree_erase(&btree_test, 1), Eq(BTREE_NOT_FOUND));
    EXPECT_THAT(btree_erase(&btree_test, 2), Eq(BTREE_NOT_FOUND));
    EXPECT_THAT(btree_erase(&btree_test, 3), Eq(BTREE_NOT_FOUND));
    EXPECT_THAT(btree_erase(&btree_test, 4), Eq(BTREE_NOT_FOUND));

    btree_deinit(&btree_test);
}

TEST_F(NAME, erase_by_value_on_empty_btree_doesnt_crash)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 56;
    EXPECT_THAT(btree_erase_value(&btree_test, &a), Eq(BTREE_INVALID_KEY));

    btree_deinit(&btree_test);
}

TEST_F(NAME, erase_by_value)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 56, b = 45, c = 18, d = 27, e = 84;
    btree_test_insert_new(&btree_test, 0, &a);
    btree_test_insert_new(&btree_test, 1, &b);
    btree_test_insert_new(&btree_test, 2, &c);
    btree_test_insert_new(&btree_test, 3, &d);
    btree_test_insert_new(&btree_test, 4, &e);

    EXPECT_THAT(btree_erase_value(&btree_test, &c), Eq(2));

    // 4
    EXPECT_THAT(
        (int*)btree_test_find(btree, 0), AllOf(NotNull(), Pointee(Eq(a))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 1), AllOf(NotNull(), Pointee(Eq(b))));
    EXPECT_THAT((int*)btree_test_find(btree, 2), IsNull());
    EXPECT_THAT(
        (int*)btree_test_find(btree, 3), AllOf(NotNull(), Pointee(Eq(d))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 4), AllOf(NotNull(), Pointee(Eq(e))));

    EXPECT_THAT(btree_erase_value(&btree_test, &e), Eq(4));

    // 3
    EXPECT_THAT(
        (int*)btree_test_find(btree, 0), AllOf(NotNull(), Pointee(Eq(a))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 1), AllOf(NotNull(), Pointee(Eq(b))));
    EXPECT_THAT((int*)btree_test_find(btree, 2), IsNull());
    EXPECT_THAT(
        (int*)btree_test_find(btree, 3), AllOf(NotNull(), Pointee(Eq(d))));
    EXPECT_THAT((int*)btree_test_find(btree, 4), IsNull());

    EXPECT_THAT(btree_erase_value(&btree_test, &a), Eq(0));

    // 2
    EXPECT_THAT((int*)btree_test_find(btree, 0), IsNull());
    EXPECT_THAT(
        (int*)btree_test_find(btree, 1), AllOf(NotNull(), Pointee(Eq(b))));
    EXPECT_THAT((int*)btree_test_find(btree, 2), IsNull());
    EXPECT_THAT(
        (int*)btree_test_find(btree, 3), AllOf(NotNull(), Pointee(Eq(d))));
    EXPECT_THAT((int*)btree_test_find(btree, 4), IsNull());

    EXPECT_THAT(btree_erase_value(&btree_test, &b), Eq(1));

    // 1
    EXPECT_THAT((int*)btree_test_find(btree, 0), IsNull());
    EXPECT_THAT((int*)btree_test_find(btree, 1), IsNull());
    EXPECT_THAT((int*)btree_test_find(btree, 2), IsNull());
    EXPECT_THAT(
        (int*)btree_test_find(btree, 3), AllOf(NotNull(), Pointee(Eq(d))));
    EXPECT_THAT((int*)btree_test_find(btree, 4), IsNull());

    EXPECT_THAT(btree_erase_value(&btree_test, &d), Eq(3));

    // 0
    EXPECT_THAT((int*)btree_test_find(btree, 0), IsNull());
    EXPECT_THAT((int*)btree_test_find(btree, 1), IsNull());
    EXPECT_THAT((int*)btree_test_find(btree, 2), IsNull());
    EXPECT_THAT((int*)btree_test_find(btree, 3), IsNull());
    EXPECT_THAT((int*)btree_test_find(btree, 4), IsNull());

    EXPECT_THAT(btree_erase_value(&btree_test, &a), Eq(BTREE_INVALID_KEY));
    EXPECT_THAT(btree_erase_value(&btree_test, &b), Eq(BTREE_INVALID_KEY));
    EXPECT_THAT(btree_erase_value(&btree_test, &c), Eq(BTREE_INVALID_KEY));
    EXPECT_THAT(btree_erase_value(&btree_test, &d), Eq(BTREE_INVALID_KEY));
    EXPECT_THAT(btree_erase_value(&btree_test, &e), Eq(BTREE_INVALID_KEY));

    btree_deinit(&btree_test);
}

TEST_F(NAME, reinsertion_forwards)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 56, b = 45, c = 18, d = 27, e = 84;
    ASSERT_THAT(btree_test_insert_new(&btree_test, 0, &a), Eq(0));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 1, &b), Eq(0));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 2, &c), Eq(0));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 3, &d), Eq(0));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 4, &e), Eq(0));

    ASSERT_THAT(btree_erase(&btree_test, 4), Eq(0));
    ASSERT_THAT(btree_erase(&btree_test, 3), Eq(0));
    ASSERT_THAT(btree_erase(&btree_test, 2), Eq(0));

    ASSERT_THAT(btree_test_insert_new(&btree_test, 2, &c), Eq(0));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 3, &d), Eq(0));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 4, &e), Eq(0));

    EXPECT_THAT(
        (int*)btree_test_find(btree, 0), AllOf(NotNull(), Pointee(Eq(a))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 1), AllOf(NotNull(), Pointee(Eq(b))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 2), AllOf(NotNull(), Pointee(Eq(c))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 3), AllOf(NotNull(), Pointee(Eq(d))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 4), AllOf(NotNull(), Pointee(Eq(e))));

    btree_deinit(&btree_test);
}

TEST_F(NAME, reinsertion_backwards)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 56, b = 45, c = 18, d = 27, e = 84;
    ASSERT_THAT(btree_test_insert_new(&btree_test, 0, &a), Eq(0));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 1, &b), Eq(0));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 2, &c), Eq(0));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 3, &d), Eq(0));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 4, &e), Eq(0));

    ASSERT_THAT(btree_erase(&btree_test, 0), Eq(0));
    ASSERT_THAT(btree_erase(&btree_test, 1), Eq(0));
    ASSERT_THAT(btree_erase(&btree_test, 2), Eq(0));

    ASSERT_THAT(btree_test_insert_new(&btree_test, 2, &c), Eq(0));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 1, &b), Eq(0));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 0, &a), Eq(0));

    EXPECT_THAT(
        (int*)btree_test_find(btree, 0), AllOf(NotNull(), Pointee(Eq(a))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 1), AllOf(NotNull(), Pointee(Eq(b))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 2), AllOf(NotNull(), Pointee(Eq(c))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 3), AllOf(NotNull(), Pointee(Eq(d))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 4), AllOf(NotNull(), Pointee(Eq(e))));

    btree_deinit(&btree_test);
}

TEST_F(NAME, reinsertion_random)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 56, b = 45, c = 18, d = 27, e = 84;
    ASSERT_THAT(btree_test_insert_new(&btree_test, 26, &a), Eq(0));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 44, &b), Eq(0));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 82, &c), Eq(0));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 41, &d), Eq(0));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 70, &e), Eq(0));

    ASSERT_THAT(btree_erase(&btree_test, 44), Eq(0));
    ASSERT_THAT(btree_erase(&btree_test, 70), Eq(0));
    ASSERT_THAT(btree_erase(&btree_test, 26), Eq(0));

    ASSERT_THAT(btree_test_insert_new(&btree_test, 26, &a), Eq(0));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 70, &e), Eq(0));
    ASSERT_THAT(btree_test_insert_new(&btree_test, 44, &b), Eq(0));

    EXPECT_THAT(
        (int*)btree_test_find(btree, 26), AllOf(NotNull(), Pointee(Eq(a))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 44), AllOf(NotNull(), Pointee(Eq(b))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 82), AllOf(NotNull(), Pointee(Eq(c))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 41), AllOf(NotNull(), Pointee(Eq(d))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 70), AllOf(NotNull(), Pointee(Eq(e))));

    btree_deinit(&btree_test);
}

TEST_F(NAME, get_any_value)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 6;
    EXPECT_THAT(btree_get_any_value(&btree_test), IsNull());
    btree_test_insert_new(&btree_test, 45, &a);
    EXPECT_THAT(
        (int*)btree_get_any_value(&btree_test),
        AllOf(NotNull(), Pointee(Eq(6))));
    btree_erase(&btree_test, 45);
    EXPECT_THAT(btree_get_any_value(&btree_test), IsNull());

    btree_deinit(&btree_test);
}

TEST_F(NAME, iterate_with_no_items)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int counter = 0;
    btree_for_each (&btree_test, int, key, value)
        ++counter;
    BTREE_END_EACH
    ASSERT_THAT(counter, Eq(0));

    btree_deinit(&btree_test);
}

TEST_F(NAME, iterate_5_random_items)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 79579, b = 235, c = 347, d = 124, e = 457;
    btree_test_insert_new(&btree_test, 243, &a);
    btree_test_insert_new(&btree_test, 256, &b);
    btree_test_insert_new(&btree_test, 456, &c);
    btree_test_insert_new(&btree_test, 468, &d);
    btree_test_insert_new(&btree_test, 969, &e);

    int counter = 0;
    BTREE_FOR_EACH(&btree_test, int, key, value)
    switch (counter)
    {
        case 0:
            ASSERT_THAT(key, Eq(243u));
            ASSERT_THAT(a, Eq(*value));
            break;
        case 1:
            ASSERT_THAT(key, Eq(256u));
            ASSERT_THAT(b, Eq(*value));
            break;
        case 2:
            ASSERT_THAT(key, Eq(456u));
            ASSERT_THAT(c, Eq(*value));
            break;
        case 3:
            ASSERT_THAT(key, Eq(468u));
            ASSERT_THAT(d, Eq(*value));
            break;
        case 4:
            ASSERT_THAT(key, Eq(969u));
            ASSERT_THAT(e, Eq(*value));
            break;
        default: ASSERT_THAT(1, Eq(0)); break;
    }
    ++counter;
    BTREE_END_EACH
    ASSERT_THAT(counter, Eq(5));

    btree_deinit(&btree_test);
}

TEST_F(NAME, erase_in_for_loop)
{
    struct cs_btree btree;
    btree_init(&btree_test, sizeof(int));

    int a = 79579, b = 235, c = 347, d = 124, e = 457;
    btree_test_insert_new(&btree_test, 243, &a);
    btree_test_insert_new(&btree_test, 256, &b);
    btree_test_insert_new(&btree_test, 456, &c);
    btree_test_insert_new(&btree_test, 468, &d);
    btree_test_insert_new(&btree_test, 969, &e);

    int counter = 0;
    BTREE_FOR_EACH(&btree_test, int, key, value)
    if (key == 256u)
        BTREE_ERASE_CURRENT_ITEM_IN_FOR_LOOP(&btree_test, key);
    ++counter;
    BTREE_END_EACH
    EXPECT_THAT(counter, Eq(5u));

    EXPECT_THAT(
        (int*)btree_test_find(btree, 243), AllOf(NotNull(), Pointee(Eq(a))));
    EXPECT_THAT((int*)btree_test_find(btree, 256), IsNull());
    EXPECT_THAT(
        (int*)btree_test_find(btree, 456), AllOf(NotNull(), Pointee(Eq(c))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 468), AllOf(NotNull(), Pointee(Eq(d))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 969), AllOf(NotNull(), Pointee(Eq(e))));

    counter = 0;
    BTREE_FOR_EACH(&btree_test, int, key, value)
    if (key == 969)
        BTREE_ERASE_CURRENT_ITEM_IN_FOR_LOOP(&btree_test, key);
    ++counter;
    BTREE_END_EACH
    EXPECT_THAT(counter, Eq(4u));

    EXPECT_THAT(
        (int*)btree_test_find(btree, 243), AllOf(NotNull(), Pointee(Eq(a))));
    EXPECT_THAT((int*)btree_test_find(btree, 256), IsNull());
    EXPECT_THAT(
        (int*)btree_test_find(btree, 456), AllOf(NotNull(), Pointee(Eq(c))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 468), AllOf(NotNull(), Pointee(Eq(d))));
    EXPECT_THAT((int*)btree_test_find(btree, 969), IsNull());

    counter = 0;
    BTREE_FOR_EACH(&btree_test, int, key, value)
    if (key == 243)
        BTREE_ERASE_CURRENT_ITEM_IN_FOR_LOOP(&btree_test, key);
    ++counter;
    BTREE_END_EACH
    EXPECT_THAT(counter, Eq(3u));

    EXPECT_THAT((int*)btree_test_find(btree, 243), IsNull());
    EXPECT_THAT((int*)btree_test_find(btree, 256), IsNull());
    EXPECT_THAT(
        (int*)btree_test_find(btree, 456), AllOf(NotNull(), Pointee(Eq(c))));
    EXPECT_THAT(
        (int*)btree_test_find(btree, 468), AllOf(NotNull(), Pointee(Eq(d))));
    EXPECT_THAT((int*)btree_test_find(btree, 969), IsNull());

    btree_deinit(&btree_test);
}
