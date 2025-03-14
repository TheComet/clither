#include <gmock/gmock.h>

extern "C" {
#include "clither/hm.h"
}

#define NAME         hm_full
#define MIN_CAPACITY 64

using namespace ::testing;

static const char KEY1[16] = "KEY1";
static const char KEY2[16] = "KEY2";
static const char KEY3[16] = "KEY3";
static const char KEY4[16] = "KEY4";

enum hash_mode
{
    NORMAL,
    SHITTY,
    COLLIDE_WITH_SHITTY_HASH,
    COLLIDE_WITH_SHITTY_HASH_SECOND_PROBE
} hash_mode;

struct hm_test_kvs
{
    char*  keys;
    float* values;
};

namespace {
static hash32 test_hash(const char* key)
{
    switch (hash_mode)
    {
        case NORMAL: break;
        case SHITTY: return 42;
        case COLLIDE_WITH_SHITTY_HASH: return MIN_CAPACITY + 42;
        case COLLIDE_WITH_SHITTY_HASH_SECOND_PROBE:
            return MIN_CAPACITY + 45; // sequence would be 42, 43, 45, 48, ...
    }
    return hash32_jenkins_oaat(key, 4);
}
static int test_storage_alloc(
    struct hm_test_kvs* kvs, struct hm_test_kvs* old_kvs, int32_t capacity)
{
    (void)old_kvs;
    kvs->keys = (char*)mem_alloc(sizeof(char) * capacity * 16);
    kvs->values = (float*)mem_alloc(sizeof(*kvs->values) * capacity);
    return 0;
}
static void test_storage_free_old(struct hm_test_kvs* kvs)
{
    mem_free(kvs->values);
    mem_free(kvs->keys);
}
static void test_storage_free(struct hm_test_kvs* kvs)
{
    mem_free(kvs->values);
    mem_free(kvs->keys);
}
static const char* test_get_key(const struct hm_test_kvs* kvs, int32_t slot)
{
    return &kvs->keys[slot * 16];
}
static void test_set_key(struct hm_test_kvs* kvs, int32_t slot, const char* key)
{
    memcpy(&kvs->keys[slot * 16], key, 16);
}
static int test_keys_equal(const char* k1, const char* k2)
{
    return memcmp(k1, k2, 16) == 0;
}
static float* test_get_value(const struct hm_test_kvs* kvs, int32_t slot)
{
    return &kvs->values[slot];
}
static void
test_set_value(struct hm_test_kvs* kvs, int32_t slot, const float* value)
{
    kvs->values[slot] = *value;
}

HM_DECLARE_FULL(hm_test, hash32, const char*, float, 32, struct hm_test_kvs)
HM_DEFINE_FULL(
    hm_test,
    hash32,
    const char*,
    float,
    32,
    test_hash,
    test_storage_alloc,
    test_storage_free_old,
    test_storage_free,
    test_get_key,
    test_set_key,
    test_keys_equal,
    test_get_value,
    test_set_value,
    MIN_CAPACITY,
    70);
} // namespace

struct NAME : Test
{
    virtual void SetUp()
    {
        hash_mode = NORMAL;
        hm_test_init(&hm);
    }

    virtual void TearDown() { hm_test_deinit(hm); }

    struct hm_test* hm;
};

TEST_F(NAME, null_hm_is_set)
{
    EXPECT_THAT(hm, IsNull());
    EXPECT_THAT(hm_count(hm), Eq(0));
    EXPECT_THAT(hm_capacity(hm), Eq(0));
}

TEST_F(NAME, deinit_null_hm_works)
{
    hm_test_deinit(hm);
}

TEST_F(NAME, insert_increases_slots_used)
{
    EXPECT_THAT(hm_count(hm), Eq(0));
    EXPECT_THAT(hm_test_insert_new(&hm, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hm_count(hm), Eq(1));
    EXPECT_THAT(hm_capacity(hm), Eq(MIN_CAPACITY));
}

TEST_F(NAME, erase_decreases_slots_used)
{
    EXPECT_THAT(hm_test_insert_new(&hm, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hm_test_erase(hm, KEY1), Pointee(5.6f));
    EXPECT_THAT(hm_count(hm), Eq(0));
    EXPECT_THAT(hm_capacity(hm), Eq(MIN_CAPACITY));
}

TEST_F(NAME, insert_same_key_twice_only_works_once)
{
    EXPECT_THAT(hm_test_insert_new(&hm, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hm_test_insert_new(&hm, KEY1, 7.6f), Eq(-1));
    EXPECT_THAT(hm_count(hm), Eq(1));
}

TEST_F(NAME, insert_or_get_returns_inserted_value)
{
    float  f = 0.0f;
    float* p = &f;
    EXPECT_THAT(hm_test_emplace_or_get(&hm, KEY1, &p), HM_NEW);
    *p = 5.6f;
    p = &f;
    EXPECT_THAT(hm_test_emplace_or_get(&hm, KEY1, &p), HM_EXISTS);
    EXPECT_THAT(f, Eq(0.0f));
    EXPECT_THAT(p, Pointee(5.6f));
    EXPECT_THAT(hm_count(hm), Eq(1));
}

TEST_F(NAME, erasing_same_key_twice_only_works_once)
{
    EXPECT_THAT(hm_test_insert_new(&hm, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hm_test_erase(hm, KEY1), Pointee(5.6f));
    EXPECT_THAT(hm_test_erase(hm, KEY1), IsNull());
    EXPECT_THAT(hm_count(hm), Eq(0));
}

TEST_F(NAME, hash_collision_insert_ab_erase_ba)
{
    EXPECT_THAT(hm_test_insert_new(&hm, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hm_test_insert_new(&hm, KEY2, 3.4f), Eq(0));
    EXPECT_THAT(hm_count(hm), Eq(2));
    EXPECT_THAT(hm_test_erase(hm, KEY2), Pointee(3.4f));
    EXPECT_THAT(hm_test_erase(hm, KEY1), Pointee(5.6f));
    EXPECT_THAT(hm_count(hm), Eq(0));
}

TEST_F(NAME, hash_collision_insert_ab_erase_ab)
{
    hash_mode = SHITTY;
    EXPECT_THAT(hm_test_insert_new(&hm, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hm_test_insert_new(&hm, KEY2, 3.4f), Eq(0));
    EXPECT_THAT(hm_count(hm), Eq(2));
    EXPECT_THAT(hm_test_erase(hm, KEY1), Pointee(5.6f));
    EXPECT_THAT(hm_test_erase(hm, KEY2), Pointee(3.4f));
    EXPECT_THAT(hm_count(hm), Eq(0));
}

TEST_F(NAME, hash_collision_insert_ab_find_ab)
{
    hash_mode = SHITTY;
    EXPECT_THAT(hm_test_insert_new(&hm, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hm_test_insert_new(&hm, KEY2, 3.4f), Eq(0));
    EXPECT_THAT(hm_test_find(hm, KEY1), Pointee(5.6f));
    EXPECT_THAT(hm_test_find(hm, KEY2), Pointee(3.4f));
}

TEST_F(NAME, hash_collision_insert_ab_erase_a_find_b)
{
    hash_mode = SHITTY;
    EXPECT_THAT(hm_test_insert_new(&hm, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hm_test_insert_new(&hm, KEY2, 3.4f), Eq(0));
    EXPECT_THAT(hm_test_erase(hm, KEY1), NotNull());
    EXPECT_THAT(hm_test_find(hm, KEY2), Pointee(3.4f));
}

TEST_F(NAME, hash_collision_insert_ab_erase_b_find_a)
{
    hash_mode = SHITTY;
    EXPECT_THAT(hm_test_insert_new(&hm, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hm_test_insert_new(&hm, KEY2, 3.4f), Eq(0));
    EXPECT_THAT(hm_test_erase(hm, KEY2), NotNull());
    EXPECT_THAT(hm_test_find(hm, KEY1), Pointee(5.6f));
}

TEST_F(NAME, hash_collision_insert_at_tombstone)
{
    hash_mode = SHITTY;
    EXPECT_THAT(hm_test_insert_new(&hm, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hm_test_insert_new(&hm, KEY2, 3.4f), Eq(0));
    EXPECT_THAT(hm_count(hm), Eq(2));
    EXPECT_THAT(hm_test_erase(hm, KEY1), Pointee(5.6f)); // creates tombstone
    EXPECT_THAT(hm_count(hm), Eq(1));
    EXPECT_THAT(
        hm_test_insert_new(&hm, KEY1, 5.6f),
        Eq(0)); // should insert at tombstone location
    EXPECT_THAT(hm_test_erase(hm, KEY1), Pointee(5.6f));
    EXPECT_THAT(hm_test_erase(hm, KEY2), Pointee(3.4f));
    EXPECT_THAT(hm_count(hm), Eq(0));
}

TEST_F(NAME, hash_collision_insert_at_tombstone_with_existing_key)
{
    hash_mode = SHITTY;
    EXPECT_THAT(hm_test_insert_new(&hm, KEY1, 5.6f), Eq(0));
    EXPECT_THAT(hm_test_insert_new(&hm, KEY2, 3.4f), Eq(0));
    EXPECT_THAT(hm_count(hm), Eq(2));
    EXPECT_THAT(hm_test_erase(hm, KEY1), Pointee(5.6f)); // creates tombstone
    EXPECT_THAT(hm_count(hm), Eq(1));
    EXPECT_THAT(hm_test_insert_new(&hm, KEY2, 5.6f), Eq(-1));
    EXPECT_THAT(hm_count(hm), Eq(1));
}

TEST_F(NAME, remove_probing_sequence_scenario_1)
{
    // Creates a tombstone in the probing sequence to KEY2
    hash_mode = SHITTY;
    hm_test_insert_new(&hm, KEY1, 5.6f);
    hm_test_insert_new(&hm, KEY2, 3.4f);
    hm_test_erase(hm, KEY1);

    // Inserts a different hash into where the tombstone is
    hash_mode = COLLIDE_WITH_SHITTY_HASH;
    hm_test_insert_new(&hm, KEY1, 5.6f);
    hash_mode = SHITTY;

    // Does this cut off access to KEY2?
    /*ASSERT_THAT(hm_test_find(hm, KEY2), NotNull());
    EXPECT_THAT(hm_test_find(hm, KEY2), Pointee(3.4f));*/
    EXPECT_THAT(hm_test_erase(hm, KEY2), Pointee(3.4f));
}

TEST_F(NAME, remove_probing_sequence_scenario_2)
{
    // First key is inserted directly, next 2 collide and are inserted along the
    // probing sequence
    hash_mode = SHITTY;
    hm_test_insert_new(&hm, KEY1, 5.6f);
    hm_test_insert_new(&hm, KEY2, 3.4f);
    hm_test_insert_new(&hm, KEY3, 1.8f);

    // Insert a key with a different hash that collides with the slot of KEY3
    hash_mode = COLLIDE_WITH_SHITTY_HASH_SECOND_PROBE;
    hm_test_insert_new(&hm, KEY4, 8.7f);

    // Erase KEY3
    hash_mode = SHITTY; // restore shitty hash
    hm_test_erase(hm, KEY3);

    // Does this cut off access to KEY4?
    hash_mode = COLLIDE_WITH_SHITTY_HASH_SECOND_PROBE;
    EXPECT_THAT(hm_test_erase(hm, KEY4), Pointee(8.7f));
}

TEST_F(NAME, rehash_test)
{
    char  key[16];
    float value = 0;
    for (int i = 0; i != MIN_CAPACITY * 128; ++i, value += 1.5f)
    {
        memset(key, 0, sizeof key);
        sprintf(key, "%d", i);
        ASSERT_THAT(hm_test_insert_new(&hm, key, value), Eq(0));
    }

    value = 0;
    for (int i = 0; i != MIN_CAPACITY * 128; ++i, value += 1.5f)
    {
        memset(key, 0, sizeof key);
        sprintf(key, "%d", i);
        EXPECT_THAT(hm_test_erase(hm, key), Pointee(value));
    }
}

TEST_F(NAME, foreach_empty)
{
    uintptr_t key;
    float*    value;
    int       counter = 0;
    int       slot;
    hm_for_each (hm, slot, key, value)
    {
        counter++;
    }
    EXPECT_THAT(counter, Eq(0));
}

TEST_F(NAME, foreach)
{
    char key[16];
    for (int i = 0; i != 16; ++i)
    {
        memset(key, 0, sizeof key);
        sprintf(key, "%d", i);
        ASSERT_THAT(hm_test_insert_new(&hm, key, float(i)), Eq(0));
    }

    auto erase_key = [&key](struct hm_test* hm, const char* k)
    {
        memset(key, 0, sizeof(key));
        memcpy(key, k, strlen(k));
        return hm_test_erase(hm, key);
    };

    ASSERT_THAT(erase_key(hm, "5"), NotNull());
    ASSERT_THAT(erase_key(hm, "8"), NotNull());
    ASSERT_THAT(erase_key(hm, "14"), NotNull());
    ASSERT_THAT(erase_key(hm, "3"), NotNull());
    ASSERT_THAT(erase_key(hm, "11"), NotNull());
    ASSERT_THAT(erase_key(hm, "6"), NotNull());

    for (int i = 16; i != 20; ++i)
    {
        memset(key, 0, sizeof key);
        sprintf(key, "%d", i);
        ASSERT_THAT(hm_test_insert_new(&hm, key, float(i)), Eq(0));
    }

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

    const char* k;
    float*      value;
    int         slot;
    hm_for_each_full (hm, slot, k, value, test_get_key, test_get_value)
    {
        (void)k;
        expected_values[*value] += 1;
    }

    EXPECT_THAT(hm_count(hm), Eq(14));
    EXPECT_THAT(expected_values.size(), Eq(14));
    for (const auto& [k, v] : expected_values)
        EXPECT_THAT(v, Eq(1)) << k;
}
