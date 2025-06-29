#include "clither/tests/LogHelper.hpp"

#include "gmock/gmock.h"

extern "C" {
#include "clither/util/vec.h"
}

#define NAME          test_vec
#define MIN_CAPACITY  32
#define EXPAND_FACTOR 2

using namespace ::testing;

namespace {
struct obj
{
    uint64_t a, b, c, d;
};

bool operator==(const struct obj& a, const struct obj& b)
{
    return a.a == b.a && a.b == b.b && a.c == b.c && a.d == b.d;
}

VEC_DECLARE(obj_vec, struct obj, 16)
VEC_DEFINE_FULL(obj_vec, struct obj, 16, MIN_CAPACITY, EXPAND_FACTOR)

void* shitty_alloc(size_t)
{
    return NULL;
}
void* shitty_realloc(void*, size_t)
{
    return NULL;
}
/* clang-format off */
#if defined(mem_alloc)
#   pragma push_macro("mem_alloc")
#   pragma push_macro("mem_realloc")
#   undef mem_alloc
#   undef mem_realloc
#   define mem_alloc   shitty_alloc
#   define mem_realloc shitty_realloc
VEC_DECLARE(shitty_obj_vec, struct obj, 16)
VEC_DEFINE(shitty_obj_vec, struct obj, 16)
#   pragma pop_macro("mem_alloc")
#   pragma pop_macro("mem_realloc")
#else
#   define mem_alloc   shitty_alloc
#   define mem_realloc shitty_realloc
VEC_DECLARE(shitty_obj_vec, struct obj, 16)
VEC_DEFINE(shitty_obj_vec, struct obj, 16)
#   undef mem_alloc
#   undef mem_realloc
#endif
/* clang-format on */
} // namespace

struct NAME : Test, LogHelper
{
public:
    void SetUp() override { obj_vec = NULL; }

    void TearDown() override { obj_vec_deinit(obj_vec); }

    struct obj_vec* obj_vec;
};

TEST_F(NAME, deinit_null_vector_works)
{
    obj_vec_deinit(obj_vec);
}

TEST_F(NAME, realloc_new_vector_sets_capacity)
{
    EXPECT_THAT(obj_vec_realloc(&obj_vec, 16), Eq(0));
    ASSERT_THAT(obj_vec, NotNull());
    EXPECT_THAT(vec_capacity(obj_vec), Eq(16));
}
TEST_F(NAME, realloc_returns_error_if_realloc_fails)
{
    EXPECT_THAT(shitty_obj_vec_realloc((shitty_obj_vec**)&obj_vec, 16), Eq(-1));
    EXPECT_THAT(obj_vec, IsNull());
    EXPECT_THAT(
        log(),
        LogEq("[Error] Failed to allocate 520 bytes in vec_realloc()\n"));
}

TEST_F(NAME, push_increments_counter)
{
    ASSERT_THAT(obj_vec_push(&obj_vec, obj{5, 5, 5, 5}), Eq(0));
    EXPECT_THAT(vec_count(obj_vec), Eq(1));
}
TEST_F(NAME, emplace_increments_counter)
{
    ASSERT_THAT(obj_vec_emplace(&obj_vec), NotNull());
    EXPECT_THAT(vec_count(obj_vec), Eq(1));
}
TEST_F(NAME, insert_increments_counter)
{
    ASSERT_THAT(obj_vec_insert(&obj_vec, 0, obj{5, 5, 5, 5}), Eq(0));
    EXPECT_THAT(vec_count(obj_vec), Eq(1));
}
TEST_F(NAME, insert_emplace_increments_counter)
{
    ASSERT_THAT(obj_vec_insert_emplace(&obj_vec, 0), NotNull());
    EXPECT_THAT(vec_count(obj_vec), Eq(1));
}

TEST_F(NAME, push_sets_capacity)
{
    ASSERT_THAT(obj_vec_insert(&obj_vec, 0, obj{5, 5, 5, 5}), Eq(0));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY));
}
TEST_F(NAME, emplace_sets_capacity)
{
    ASSERT_THAT(obj_vec_emplace(&obj_vec), NotNull());
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY));
}
TEST_F(NAME, insert_sets_capacity)
{
    ASSERT_THAT(obj_vec_insert(&obj_vec, 0, obj{5, 5, 5, 5}), Eq(0));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY));
}
TEST_F(NAME, insert_emplace_sets_capacity)
{
    ASSERT_THAT(obj_vec_insert_emplace(&obj_vec, 0), NotNull());
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY));
}

TEST_F(NAME, push_returns_error_if_realloc_fails)
{
    EXPECT_THAT(
        shitty_obj_vec_push((shitty_obj_vec**)&obj_vec, obj{5, 5, 5, 5}),
        Eq(-1));
    EXPECT_THAT(obj_vec, IsNull());
    EXPECT_THAT(
        log(),
        LogEq("[Error] Failed to allocate 1032 bytes in vec_realloc()\n"));
}
TEST_F(NAME, emplace_returns_error_if_realloc_fails)
{
    EXPECT_THAT(shitty_obj_vec_emplace((shitty_obj_vec**)&obj_vec), IsNull());
    EXPECT_THAT(obj_vec, IsNull());
    EXPECT_THAT(
        log(),
        LogEq("[Error] Failed to allocate 1032 bytes in vec_realloc()\n"));
}
TEST_F(NAME, insert_returns_error_if_realloc_fails)
{
    EXPECT_THAT(
        shitty_obj_vec_insert((shitty_obj_vec**)&obj_vec, 0, obj{5, 5, 5, 5}),
        Eq(-1));
    EXPECT_THAT(obj_vec, IsNull());
    EXPECT_THAT(
        log(),
        LogEq("[Error] Failed to allocate 1032 bytes in vec_realloc()\n"));
}
TEST_F(NAME, insert_emplace_returns_error_if_realloc_fails)
{
    EXPECT_THAT(
        shitty_obj_vec_insert_emplace((shitty_obj_vec**)&obj_vec, 0), IsNull());
    EXPECT_THAT(obj_vec, IsNull());
    EXPECT_THAT(
        log(),
        LogEq("[Error] Failed to allocate 1032 bytes in vec_realloc()\n"));
}

TEST_F(NAME, push_few_values_works)
{
    EXPECT_THAT(obj_vec_push(&obj_vec, obj{5, 5, 5, 5}), Eq(0));
    EXPECT_THAT(obj_vec_push(&obj_vec, obj{7, 7, 7, 7}), Eq(0));
    EXPECT_THAT(obj_vec_push(&obj_vec, obj{3, 3, 3, 3}), Eq(0));
    EXPECT_THAT(vec_count(obj_vec), Eq(3));
    EXPECT_THAT(vec_get(obj_vec, 0), Pointee(obj{5, 5, 5, 5}));
    EXPECT_THAT(vec_get(obj_vec, 1), Pointee(obj{7, 7, 7, 7}));
    EXPECT_THAT(vec_get(obj_vec, 2), Pointee(obj{3, 3, 3, 3}));
}
TEST_F(NAME, emplace_few_values_works)
{
    *obj_vec_emplace(&obj_vec) = obj{5, 5, 5, 5};
    *obj_vec_emplace(&obj_vec) = obj{7, 7, 7, 7};
    *obj_vec_emplace(&obj_vec) = obj{3, 3, 3, 3};
    EXPECT_THAT(vec_count(obj_vec), Eq(3));
    EXPECT_THAT(vec_get(obj_vec, 0), Pointee(obj{5, 5, 5, 5}));
    EXPECT_THAT(vec_get(obj_vec, 1), Pointee(obj{7, 7, 7, 7}));
    EXPECT_THAT(vec_get(obj_vec, 2), Pointee(obj{3, 3, 3, 3}));
}
TEST_F(NAME, insert_few_values_works)
{
    ASSERT_THAT(obj_vec_insert(&obj_vec, 0, obj{5, 5, 5, 5}), Eq(0));
    ASSERT_THAT(obj_vec_insert(&obj_vec, 0, obj{7, 7, 7, 7}), Eq(0));
    ASSERT_THAT(obj_vec_insert(&obj_vec, 0, obj{3, 3, 3, 3}), Eq(0));
    EXPECT_THAT(vec_count(obj_vec), Eq(3));
    EXPECT_THAT(vec_get(obj_vec, 0), Pointee(obj{3, 3, 3, 3}));
    EXPECT_THAT(vec_get(obj_vec, 1), Pointee(obj{7, 7, 7, 7}));
    EXPECT_THAT(vec_get(obj_vec, 2), Pointee(obj{5, 5, 5, 5}));
}
TEST_F(NAME, insert_emplace_few_values_works)
{
    *obj_vec_insert_emplace(&obj_vec, 0) = obj{5, 5, 5, 5};
    *obj_vec_insert_emplace(&obj_vec, 0) = obj{7, 7, 7, 7};
    *obj_vec_insert_emplace(&obj_vec, 0) = obj{3, 3, 3, 3};
    EXPECT_THAT(vec_count(obj_vec), Eq(3));
    EXPECT_THAT(vec_get(obj_vec, 0), Pointee(obj{3, 3, 3, 3}));
    EXPECT_THAT(vec_get(obj_vec, 1), Pointee(obj{7, 7, 7, 7}));
    EXPECT_THAT(vec_get(obj_vec, 2), Pointee(obj{5, 5, 5, 5}));
}

TEST_F(NAME, push_with_expand_sets_count_and_capacity_correctly)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        ASSERT_THAT(obj_vec_push(&obj_vec, obj{i, i, i, i}), Eq(0));

    EXPECT_THAT(vec_count(obj_vec), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY));

    ASSERT_THAT(obj_vec_push(&obj_vec, obj{42, 42, 42, 42}), Eq(0));
    EXPECT_THAT(vec_count(obj_vec), Eq(MIN_CAPACITY + 1));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY * EXPAND_FACTOR));
}
TEST_F(NAME, emplace_with_expand_sets_count_and_capacity_correctly)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        ASSERT_THAT(obj_vec_emplace(&obj_vec), NotNull());

    EXPECT_THAT(vec_count(obj_vec), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY));

    ASSERT_THAT(obj_vec_emplace(&obj_vec), NotNull());
    EXPECT_THAT(vec_count(obj_vec), Eq(MIN_CAPACITY + 1));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY * EXPAND_FACTOR));
}
TEST_F(NAME, insert_with_expand_sets_count_and_capacity_correctly)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        ASSERT_THAT(obj_vec_insert(&obj_vec, 0, obj{i, i, i, i}), Eq(0));

    EXPECT_THAT(vec_count(obj_vec), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY));

    ASSERT_THAT(obj_vec_insert(&obj_vec, 3, obj{42, 42, 42, 42}), Eq(0));
    EXPECT_THAT(vec_count(obj_vec), Eq(MIN_CAPACITY + 1));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY * EXPAND_FACTOR));
}
TEST_F(NAME, insert_emplace_with_expand_sets_count_and_capacity_correctly)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        ASSERT_THAT(obj_vec_insert_emplace(&obj_vec, 0), NotNull());

    EXPECT_THAT(vec_count(obj_vec), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY));

    ASSERT_THAT(obj_vec_insert_emplace(&obj_vec, 3), NotNull());
    EXPECT_THAT(vec_count(obj_vec), Eq(MIN_CAPACITY + 1));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY * EXPAND_FACTOR));
}

TEST_F(NAME, push_with_expand_has_correct_values)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        ASSERT_THAT(obj_vec_push(&obj_vec, obj{i, i, i, i}), Eq(0));
    ASSERT_THAT(obj_vec_push(&obj_vec, obj{42, 42, 42, 42}), Eq(0));

    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        EXPECT_THAT(vec_get(obj_vec, i), Pointee(obj{i, i, i, i}));
    EXPECT_THAT(vec_get(obj_vec, MIN_CAPACITY), Pointee(obj{42, 42, 42, 42}));
}
TEST_F(NAME, emplace_with_expand_has_correct_values)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        *obj_vec_emplace(&obj_vec) = obj{i, i, i, i};
    *obj_vec_emplace(&obj_vec) = obj{42, 42, 42, 42};

    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        EXPECT_THAT(vec_get(obj_vec, i), Pointee(obj{i, i, i, i}));
    EXPECT_THAT(vec_get(obj_vec, MIN_CAPACITY), Pointee(obj{42, 42, 42, 42}));
}
TEST_F(NAME, insert_with_expand_has_correct_values)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        ASSERT_THAT(obj_vec_insert(&obj_vec, 0, obj{i, i, i, i}), Eq(0));
    ASSERT_THAT(obj_vec_insert(&obj_vec, 3, obj{42, 42, 42, 42}), Eq(0));

    for (uint64_t i = 0; i != 3; ++i)
        EXPECT_THAT(
            vec_get(obj_vec, i),
            Pointee(
                obj{MIN_CAPACITY - i - 1,
                    MIN_CAPACITY - i - 1,
                    MIN_CAPACITY - i - 1,
                    MIN_CAPACITY - i - 1}));
    EXPECT_THAT(vec_get(obj_vec, 3), Pointee(obj{42, 42, 42, 42}));
    for (uint64_t i = 4; i != MIN_CAPACITY + 1; ++i)
        EXPECT_THAT(
            vec_get(obj_vec, i),
            Pointee(
                obj{MIN_CAPACITY - i,
                    MIN_CAPACITY - i,
                    MIN_CAPACITY - i,
                    MIN_CAPACITY - i}));
}
TEST_F(NAME, insert_emplace_with_expand_has_correct_values)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        *obj_vec_insert_emplace(&obj_vec, 0) = obj{i, i, i, i};
    *obj_vec_insert_emplace(&obj_vec, 3) = obj{42, 42, 42, 42};

    for (uint64_t i = 0; i != 3; ++i)
        EXPECT_THAT(
            vec_get(obj_vec, i),
            Pointee(
                obj{MIN_CAPACITY - i - 1,
                    MIN_CAPACITY - i - 1,
                    MIN_CAPACITY - i - 1,
                    MIN_CAPACITY - i - 1}));
    EXPECT_THAT(vec_get(obj_vec, 3), Pointee(obj{42, 42, 42, 42}));
    for (uint64_t i = 4; i != MIN_CAPACITY + 1; ++i)
        EXPECT_THAT(
            vec_get(obj_vec, i),
            Pointee(
                obj{MIN_CAPACITY - i,
                    MIN_CAPACITY - i,
                    MIN_CAPACITY - i,
                    MIN_CAPACITY - i}));
}

TEST_F(NAME, push_expand_with_failed_realloc_returns_error)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        ASSERT_THAT(obj_vec_push(&obj_vec, obj{i, i, i, i}), Eq(0));

    EXPECT_THAT(vec_count(obj_vec), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY));

    EXPECT_THAT(
        shitty_obj_vec_push((shitty_obj_vec**)&obj_vec, obj{42, 42, 42, 42}),
        Eq(-1));
    EXPECT_THAT(vec_count(obj_vec), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY));
    EXPECT_THAT(
        log(),
        LogEq("[Error] Failed to allocate 2056 bytes in vec_realloc()\n"));
}
TEST_F(NAME, emplace_expand_with_failed_realloc_returns_error)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        ASSERT_THAT(obj_vec_emplace(&obj_vec), NotNull());

    EXPECT_THAT(vec_count(obj_vec), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY));

    EXPECT_THAT(shitty_obj_vec_emplace((shitty_obj_vec**)&obj_vec), IsNull());
    EXPECT_THAT(vec_count(obj_vec), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY));
    EXPECT_THAT(
        log(),
        LogEq("[Error] Failed to allocate 2056 bytes in vec_realloc()\n"));
}
TEST_F(NAME, insert_expand_with_failed_realloc_returns_error)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        ASSERT_THAT(obj_vec_insert(&obj_vec, 0, obj{i, i, i, i}), Eq(0));

    EXPECT_THAT(vec_count(obj_vec), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY));

    EXPECT_THAT(
        shitty_obj_vec_insert(
            (shitty_obj_vec**)&obj_vec, 3, obj{42, 42, 42, 42}),
        Eq(-1));
    EXPECT_THAT(vec_count(obj_vec), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY));
    EXPECT_THAT(
        log(),
        LogEq("[Error] Failed to allocate 2056 bytes in vec_realloc()\n"));
}
TEST_F(NAME, insert_emplace_expand_with_failed_realloc_returns_error)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        ASSERT_THAT(obj_vec_insert_emplace(&obj_vec, 0), NotNull());

    EXPECT_THAT(vec_count(obj_vec), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY));

    EXPECT_THAT(
        shitty_obj_vec_insert_emplace((shitty_obj_vec**)&obj_vec, 3), IsNull());
    EXPECT_THAT(vec_count(obj_vec), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY));
    EXPECT_THAT(
        log(),
        LogEq("[Error] Failed to allocate 2056 bytes in vec_realloc()\n"));
}

TEST_F(NAME, inserting_preserves_existing_elements)
{
    obj_vec_push(&obj_vec, obj{53, 53, 53, 53});
    obj_vec_push(&obj_vec, obj{24, 24, 24, 24});
    obj_vec_push(&obj_vec, obj{73, 73, 73, 73});
    obj_vec_push(&obj_vec, obj{43, 43, 43, 43});
    obj_vec_push(&obj_vec, obj{65, 65, 65, 65});

    obj_vec_insert(&obj_vec, 2, obj{68, 68, 68, 68}); // middle insertion
    EXPECT_THAT(vec_get(obj_vec, 0), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_get(obj_vec, 1), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(vec_get(obj_vec, 2), Pointee(obj{68, 68, 68, 68}));
    EXPECT_THAT(vec_get(obj_vec, 3), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_get(obj_vec, 4), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(obj_vec, 5), Pointee(obj{65, 65, 65, 65}));

    obj_vec_insert(&obj_vec, 0, obj{16, 16, 16, 16}); // beginning insertion
    EXPECT_THAT(vec_get(obj_vec, 0), Pointee(obj{16, 16, 16, 16}));
    EXPECT_THAT(vec_get(obj_vec, 1), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_get(obj_vec, 2), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(vec_get(obj_vec, 3), Pointee(obj{68, 68, 68, 68}));
    EXPECT_THAT(vec_get(obj_vec, 4), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_get(obj_vec, 5), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(obj_vec, 6), Pointee(obj{65, 65, 65, 65}));

    obj_vec_insert(&obj_vec, 7, obj{82, 82, 82, 82}); // end insertion
    EXPECT_THAT(vec_get(obj_vec, 0), Pointee(obj{16, 16, 16, 16}));
    EXPECT_THAT(vec_get(obj_vec, 1), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_get(obj_vec, 2), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(vec_get(obj_vec, 3), Pointee(obj{68, 68, 68, 68}));
    EXPECT_THAT(vec_get(obj_vec, 4), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_get(obj_vec, 5), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(obj_vec, 6), Pointee(obj{65, 65, 65, 65}));
    EXPECT_THAT(vec_get(obj_vec, 7), Pointee(obj{82, 82, 82, 82}));

    obj_vec_insert(&obj_vec, 7, obj{37, 37, 37, 37}); // end insertion
    EXPECT_THAT(vec_get(obj_vec, 0), Pointee(obj{16, 16, 16, 16}));
    EXPECT_THAT(vec_get(obj_vec, 1), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_get(obj_vec, 2), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(vec_get(obj_vec, 3), Pointee(obj{68, 68, 68, 68}));
    EXPECT_THAT(vec_get(obj_vec, 4), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_get(obj_vec, 5), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(obj_vec, 6), Pointee(obj{65, 65, 65, 65}));
    EXPECT_THAT(vec_get(obj_vec, 7), Pointee(obj{37, 37, 37, 37}));
    EXPECT_THAT(vec_get(obj_vec, 8), Pointee(obj{82, 82, 82, 82}));
}

TEST_F(NAME, insert_emplacing_preserves_existing_elements)
{
    obj_vec_push(&obj_vec, obj{53, 53, 53, 53});
    obj_vec_push(&obj_vec, obj{24, 24, 24, 24});
    obj_vec_push(&obj_vec, obj{73, 73, 73, 73});
    obj_vec_push(&obj_vec, obj{43, 43, 43, 43});
    obj_vec_push(&obj_vec, obj{65, 65, 65, 65});

    *obj_vec_insert_emplace(&obj_vec, 2) =
        obj{68, 68, 68, 68}; // middle insertion
    EXPECT_THAT(vec_get(obj_vec, 0), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_get(obj_vec, 1), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(vec_get(obj_vec, 2), Pointee(obj{68, 68, 68, 68}));
    EXPECT_THAT(vec_get(obj_vec, 3), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_get(obj_vec, 4), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(obj_vec, 5), Pointee(obj{65, 65, 65, 65}));

    *obj_vec_insert_emplace(&obj_vec, 0) =
        obj{16, 16, 16, 16}; // beginning insertion
    EXPECT_THAT(vec_get(obj_vec, 0), Pointee(obj{16, 16, 16, 16}));
    EXPECT_THAT(vec_get(obj_vec, 1), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_get(obj_vec, 2), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(vec_get(obj_vec, 3), Pointee(obj{68, 68, 68, 68}));
    EXPECT_THAT(vec_get(obj_vec, 4), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_get(obj_vec, 5), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(obj_vec, 6), Pointee(obj{65, 65, 65, 65}));

    *obj_vec_insert_emplace(&obj_vec, 7) = obj{82, 82, 82, 82}; // end insertion
    EXPECT_THAT(vec_get(obj_vec, 0), Pointee(obj{16, 16, 16, 16}));
    EXPECT_THAT(vec_get(obj_vec, 1), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_get(obj_vec, 2), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(vec_get(obj_vec, 3), Pointee(obj{68, 68, 68, 68}));
    EXPECT_THAT(vec_get(obj_vec, 4), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_get(obj_vec, 5), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(obj_vec, 6), Pointee(obj{65, 65, 65, 65}));
    EXPECT_THAT(vec_get(obj_vec, 7), Pointee(obj{82, 82, 82, 82}));

    *obj_vec_insert_emplace(&obj_vec, 7) = obj{37, 37, 37, 37}; // end insertion
    EXPECT_THAT(vec_get(obj_vec, 0), Pointee(obj{16, 16, 16, 16}));
    EXPECT_THAT(vec_get(obj_vec, 1), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_get(obj_vec, 2), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(vec_get(obj_vec, 3), Pointee(obj{68, 68, 68, 68}));
    EXPECT_THAT(vec_get(obj_vec, 4), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_get(obj_vec, 5), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(obj_vec, 6), Pointee(obj{65, 65, 65, 65}));
    EXPECT_THAT(vec_get(obj_vec, 7), Pointee(obj{37, 37, 37, 37}));
    EXPECT_THAT(vec_get(obj_vec, 8), Pointee(obj{82, 82, 82, 82}));
}

TEST_F(NAME, clear_null_vector_works)
{
    obj_vec_clear(obj_vec);
}

TEST_F(NAME, clear_keeps_buffer_and_resets_count)
{
    for (uint64_t i = 0; i != MIN_CAPACITY * 2; ++i)
        ASSERT_THAT(obj_vec_push(&obj_vec, obj{i, i, i, i}), Eq(0));

    EXPECT_THAT(vec_count(obj_vec), Eq(MIN_CAPACITY * 2));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY * 2));
    obj_vec_clear(obj_vec);
    EXPECT_THAT(vec_count(obj_vec), Eq(0u));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(MIN_CAPACITY * 2));
}

TEST_F(NAME, compact_null_vector_works)
{
    obj_vec_compact(&obj_vec);
}

TEST_F(NAME, compact_sets_capacity)
{
    obj_vec_push(&obj_vec, obj{9, 9, 9, 9});
    obj_vec_compact(&obj_vec);
    ASSERT_THAT(obj_vec, NotNull());
    EXPECT_THAT(vec_count(obj_vec), Eq(1));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(1));
}

TEST_F(NAME, compact_removes_excess_space)
{
    obj_vec_push(&obj_vec, obj{1, 1, 1, 1});
    obj_vec_push(&obj_vec, obj{2, 2, 2, 2});
    obj_vec_push(&obj_vec, obj{3, 3, 3, 3});
    obj_vec_pop(obj_vec);
    obj_vec_pop(obj_vec);
    obj_vec_compact(&obj_vec);
    ASSERT_THAT(obj_vec, NotNull());
    EXPECT_THAT(vec_count(obj_vec), Eq(1));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(1));
}

TEST_F(NAME, clear_and_compact_deletes_buffer)
{
    obj_vec_push(&obj_vec, obj{9, 9, 9, 9});
    obj_vec_clear(obj_vec);
    obj_vec_compact(&obj_vec);
    EXPECT_THAT(obj_vec, IsNull());
}

TEST_F(NAME, clear_compact_null_vector_works)
{
    obj_vec_clear_compact(&obj_vec);
}

TEST_F(NAME, clear_compact_deletes_buffer)
{
    obj_vec_push(&obj_vec, obj{9, 9, 9, 9});
    obj_vec_clear_compact(&obj_vec);
    EXPECT_THAT(obj_vec, IsNull());
}

TEST_F(NAME, pop_returns_pushed_values)
{
    obj_vec_push(&obj_vec, obj{3, 3, 3, 3});
    obj_vec_push(&obj_vec, obj{2, 2, 2, 2});
    obj_vec_push(&obj_vec, obj{6, 6, 6, 6});
    EXPECT_THAT(obj_vec_pop(obj_vec), Pointee(obj{6, 6, 6, 6}));
    obj_vec_push(&obj_vec, obj{23, 23, 23, 23});
    obj_vec_push(&obj_vec, obj{21, 21, 21, 21});
    EXPECT_THAT(obj_vec_pop(obj_vec), Pointee(obj{21, 21, 21, 21}));
    EXPECT_THAT(obj_vec_pop(obj_vec), Pointee(obj{23, 23, 23, 23}));
    EXPECT_THAT(obj_vec_pop(obj_vec), Pointee(obj{2, 2, 2, 2}));
    EXPECT_THAT(obj_vec_pop(obj_vec), Pointee(obj{3, 3, 3, 3}));

    ASSERT_THAT(obj_vec, NotNull());
    EXPECT_THAT(vec_count(obj_vec), Eq(0u));
}

TEST_F(NAME, pop_returns_emplaced_values)
{
    *obj_vec_emplace(&obj_vec) = obj{53, 53, 53, 53};
    *obj_vec_emplace(&obj_vec) = obj{24, 24, 24, 24};
    *obj_vec_emplace(&obj_vec) = obj{73, 73, 73, 73};
    EXPECT_THAT(obj_vec_pop(obj_vec), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_count(obj_vec), Eq(2));
    *obj_vec_emplace(&obj_vec) = obj{28, 28, 28, 28};
    *obj_vec_emplace(&obj_vec) = obj{72, 72, 72, 72};
    EXPECT_THAT(obj_vec_pop(obj_vec), Pointee(obj{72, 72, 72, 72}));
    EXPECT_THAT(obj_vec_pop(obj_vec), Pointee(obj{28, 28, 28, 28}));
    EXPECT_THAT(obj_vec_pop(obj_vec), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(obj_vec_pop(obj_vec), Pointee(obj{53, 53, 53, 53}));

    EXPECT_THAT(vec_count(obj_vec), Eq(0u));
    EXPECT_THAT(obj_vec, NotNull());
}

TEST_F(NAME, popping_preserves_existing_elements)
{
    obj_vec_push(&obj_vec, obj{53, 53, 53, 53});
    obj_vec_push(&obj_vec, obj{24, 24, 24, 24});
    obj_vec_push(&obj_vec, obj{73, 73, 73, 73});
    obj_vec_push(&obj_vec, obj{43, 43, 43, 43});
    obj_vec_push(&obj_vec, obj{24, 24, 24, 24});

    obj_vec_pop(obj_vec);
    EXPECT_THAT(vec_get(obj_vec, 1), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(vec_get(obj_vec, 3), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(obj_vec, 2), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_get(obj_vec, 0), Pointee(obj{53, 53, 53, 53}));
}

TEST_F(NAME, get_last_element)
{
    obj_vec_push(&obj_vec, obj{53, 53, 53, 53});
    EXPECT_THAT((vec_last(obj_vec)), Pointee(obj{53, 53, 53, 53}));
    obj_vec_push(&obj_vec, obj{24, 24, 24, 24});
    EXPECT_THAT((vec_last(obj_vec)), Pointee(obj{24, 24, 24, 24}));
    obj_vec_push(&obj_vec, obj{73, 73, 73, 73});
    EXPECT_THAT((vec_last(obj_vec)), Pointee(obj{73, 73, 73, 73}));
}

TEST_F(NAME, get_first_element)
{
    obj_vec_push(&obj_vec, obj{53, 53, 53, 53});
    EXPECT_THAT((vec_first(obj_vec)), Pointee(obj{53, 53, 53, 53}));
    obj_vec_push(&obj_vec, obj{24, 24, 24, 24});
    EXPECT_THAT((vec_first(obj_vec)), Pointee(obj{53, 53, 53, 53}));
    obj_vec_push(&obj_vec, obj{73, 73, 73, 73});
    EXPECT_THAT((vec_first(obj_vec)), Pointee(obj{53, 53, 53, 53}));
}

TEST_F(NAME, get_element_random_access)
{
    obj_vec_push(&obj_vec, obj{53, 53, 53, 53});
    obj_vec_push(&obj_vec, obj{24, 24, 24, 24});
    obj_vec_push(&obj_vec, obj{73, 73, 73, 73});
    obj_vec_push(&obj_vec, obj{43, 43, 43, 43});
    EXPECT_THAT((vec_get(obj_vec, 1)), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT((vec_get(obj_vec, 3)), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT((vec_get(obj_vec, 2)), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT((vec_get(obj_vec, 0)), Pointee(obj{53, 53, 53, 53}));
}

TEST_F(NAME, rget_element_random_access)
{
    obj_vec_push(&obj_vec, obj{53, 53, 53, 53});
    obj_vec_push(&obj_vec, obj{24, 24, 24, 24});
    obj_vec_push(&obj_vec, obj{73, 73, 73, 73});
    obj_vec_push(&obj_vec, obj{43, 43, 43, 43});
    EXPECT_THAT(vec_rget(obj_vec, 1), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_rget(obj_vec, 3), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_rget(obj_vec, 2), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(vec_rget(obj_vec, 0), Pointee(obj{43, 43, 43, 43}));
}

TEST_F(NAME, erasing_by_index_preserves_existing_elements)
{
    obj_vec_push(&obj_vec, obj{53, 53, 53, 53});
    obj_vec_push(&obj_vec, obj{24, 24, 24, 24});
    obj_vec_push(&obj_vec, obj{73, 73, 73, 73});
    obj_vec_push(&obj_vec, obj{43, 43, 43, 43});
    obj_vec_push(&obj_vec, obj{65, 65, 65, 65});

    obj_vec_erase(obj_vec, 1);
    EXPECT_THAT(vec_get(obj_vec, 0), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_get(obj_vec, 1), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_get(obj_vec, 2), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(obj_vec, 3), Pointee(obj{65, 65, 65, 65}));

    obj_vec_erase(obj_vec, 1);
    EXPECT_THAT(vec_get(obj_vec, 0), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_get(obj_vec, 1), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(obj_vec, 2), Pointee(obj{65, 65, 65, 65}));
}

TEST_F(NAME, for_each_zero_elements)
{
    obj* value;
    int  counter = 0;
    vec_for_each (obj_vec, value)
        counter++;

    EXPECT_THAT(counter, Eq(0));
}

TEST_F(NAME, for_each_one_element)
{
    obj_vec_push(&obj_vec, obj{1, 1, 1, 1});

    int  counter = 0;
    obj* value;
    vec_for_each (obj_vec, value)
    {
        counter++;
        EXPECT_THAT(value, Pointee(obj{1, 1, 1, 1}));
    }

    EXPECT_THAT(counter, Eq(1));
}

TEST_F(NAME, for_each_three_elements)
{
    obj_vec_push(&obj_vec, obj{1, 1, 1, 1});
    obj_vec_push(&obj_vec, obj{2, 2, 2, 2});
    obj_vec_push(&obj_vec, obj{3, 3, 3, 3});

    uint64_t counter = 0;
    obj*     value;
    vec_for_each (obj_vec, value)
    {
        counter++;
        EXPECT_THAT(value, Pointee(obj{counter, counter, counter, counter}));
    }

    EXPECT_THAT(counter, Eq(3));
}

TEST_F(NAME, retain_empty_vec)
{
    int counter = 0;
    EXPECT_THAT(
        obj_vec_retain(
            obj_vec,
            [](obj*, void* counter)
            {
                ++*(int*)counter;
                return VEC_RETAIN;
            },
            &counter),
        Eq(0));
    EXPECT_THAT(vec_count(obj_vec), Eq(0));
    EXPECT_THAT(counter, Eq(0));
}

TEST_F(NAME, retain_all)
{
    for (uint64_t i = 0; i != 8; ++i)
        obj_vec_push(&obj_vec, obj{i, i, i, i});

    int counter = 0;
    EXPECT_THAT(vec_count(obj_vec), Eq(8));
    EXPECT_THAT(
        obj_vec_retain(
            obj_vec,
            [](obj*, void* counter)
            {
                ++*(int*)counter;
                return VEC_RETAIN;
            },
            &counter),
        Eq(0));
    EXPECT_THAT(vec_count(obj_vec), Eq(8));
    EXPECT_THAT(counter, Eq(8));
}

TEST_F(NAME, retain_half)
{
    for (uint64_t i = 0; i != 8; ++i)
        obj_vec_push(&obj_vec, obj{i, i, i, i});

    EXPECT_THAT(vec_count(obj_vec), Eq(8));
    int counter = 0;
    EXPECT_THAT(
        obj_vec_retain(
            obj_vec,
            [](obj*, void* user) { return (*(int*)user)++ % 2; },
            &counter),
        Eq(0));
    EXPECT_THAT(vec_count(obj_vec), Eq(4));
    EXPECT_THAT(counter, Eq(8));
    EXPECT_THAT(vec_get(obj_vec, 0), Pointee(obj{0, 0, 0, 0}));
    EXPECT_THAT(vec_get(obj_vec, 1), Pointee(obj{2, 2, 2, 2}));
    EXPECT_THAT(vec_get(obj_vec, 2), Pointee(obj{4, 4, 4, 4}));
    EXPECT_THAT(vec_get(obj_vec, 3), Pointee(obj{6, 6, 6, 6}));
}

TEST_F(NAME, retain_returning_error)
{
    for (uint64_t i = 0; i != 8; ++i)
        obj_vec_push(&obj_vec, obj{i, i, i, i});

    EXPECT_THAT(vec_count(obj_vec), Eq(8));
    EXPECT_THAT(
        obj_vec_retain(obj_vec, [](obj*, void*) { return -5; }, NULL), Eq(-5));
    EXPECT_THAT(vec_count(obj_vec), Eq(8));
}

TEST_F(NAME, insert_up_to_realloc_doesnt_cause_invalid_memmove)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        obj_vec_insert(&obj_vec, i, obj{i, i, i, i});
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        EXPECT_THAT(obj_vec->data[i], Eq(obj{i, i, i, i}));
}

TEST_F(NAME, realloc_from_0_to_0)
{
    obj_vec_realloc(&obj_vec, 0);
    EXPECT_THAT(obj_vec, IsNull());
}

TEST_F(NAME, realloc_from_8_to_0)
{
    obj_vec_realloc(&obj_vec, 8);
    EXPECT_THAT(vec_count(obj_vec), Eq(0));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(8));
    EXPECT_THAT(obj_vec->data, NotNull());

    obj_vec_realloc(&obj_vec, 0);
    EXPECT_THAT(vec_count(obj_vec), Eq(0));
    EXPECT_THAT(vec_capacity(obj_vec), Eq(0));
    EXPECT_THAT(obj_vec, IsNull());
}
