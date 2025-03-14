#include "gmock/gmock.h"

extern "C" {
#include "clither/vec.h"
}

#define NAME          vec
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

VEC_DECLARE(vobj, struct obj, 16)
VEC_DEFINE_FULL(vobj, struct obj, 16, MIN_CAPACITY, EXPAND_FACTOR)

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
VEC_DECLARE_API(static, shitty_vobj, struct obj, 16)
VEC_DEFINE_API(shitty_vobj, struct obj, 16)
#   pragma pop_macro("mem_alloc")
#   pragma pop_macro("mem_realloc")
#else
#   define mem_alloc   shitty_alloc
#   define mem_realloc shitty_realloc
VEC_DECLARE(shitty_vobj, struct obj, 16)
VEC_DEFINE(shitty_vobj, struct obj, 16)
#   undef mem_alloc
#   undef mem_realloc
#endif
/* clang-format on */
} // namespace

class NAME : public Test
{
public:
    void SetUp() override { vobj = NULL; }

    void TearDown() override { vobj_deinit(vobj); }

    struct vobj* vobj;
};

TEST_F(NAME, deinit_null_vector_works)
{
    vobj_deinit(vobj);
}

TEST_F(NAME, realloc_new_vector_sets_capacity)
{
    EXPECT_THAT(vobj_realloc(&vobj, 16), Eq(0));
    ASSERT_THAT(vobj, NotNull());
    EXPECT_THAT(vec_capacity(vobj), Eq(16));
}
TEST_F(NAME, realloc_returns_error_if_realloc_fails)
{
    EXPECT_THAT(shitty_vobj_realloc((shitty_vobj**)&vobj, 16), Eq(-1));
    EXPECT_THAT(vobj, IsNull());
}

TEST_F(NAME, push_increments_counter)
{
    ASSERT_THAT(vobj_push(&vobj, obj{5, 5, 5, 5}), Eq(0));
    EXPECT_THAT(vec_count(vobj), Eq(1));
}
TEST_F(NAME, emplace_increments_counter)
{
    ASSERT_THAT(vobj_emplace(&vobj), NotNull());
    EXPECT_THAT(vec_count(vobj), Eq(1));
}
TEST_F(NAME, insert_increments_counter)
{
    ASSERT_THAT(vobj_insert(&vobj, 0, obj{5, 5, 5, 5}), Eq(0));
    EXPECT_THAT(vec_count(vobj), Eq(1));
}
TEST_F(NAME, insert_emplace_increments_counter)
{
    ASSERT_THAT(vobj_insert_emplace(&vobj, 0), NotNull());
    EXPECT_THAT(vec_count(vobj), Eq(1));
}

TEST_F(NAME, push_sets_capacity)
{
    ASSERT_THAT(vobj_insert(&vobj, 0, obj{5, 5, 5, 5}), Eq(0));
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY));
}
TEST_F(NAME, emplace_sets_capacity)
{
    ASSERT_THAT(vobj_emplace(&vobj), NotNull());
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY));
}
TEST_F(NAME, insert_sets_capacity)
{
    ASSERT_THAT(vobj_insert(&vobj, 0, obj{5, 5, 5, 5}), Eq(0));
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY));
}
TEST_F(NAME, insert_emplace_sets_capacity)
{
    ASSERT_THAT(vobj_insert_emplace(&vobj, 0), NotNull());
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY));
}

TEST_F(NAME, push_returns_error_if_realloc_fails)
{
    EXPECT_THAT(
        shitty_vobj_push((shitty_vobj**)&vobj, obj{5, 5, 5, 5}), Eq(-1));
    EXPECT_THAT(vobj, IsNull());
}
TEST_F(NAME, emplace_returns_error_if_realloc_fails)
{
    EXPECT_THAT(shitty_vobj_emplace((shitty_vobj**)&vobj), IsNull());
    EXPECT_THAT(vobj, IsNull());
}
TEST_F(NAME, insert_returns_error_if_realloc_fails)
{
    EXPECT_THAT(
        shitty_vobj_insert((shitty_vobj**)&vobj, 0, obj{5, 5, 5, 5}), Eq(-1));
    EXPECT_THAT(vobj, IsNull());
}
TEST_F(NAME, insert_emplace_returns_error_if_realloc_fails)
{
    EXPECT_THAT(shitty_vobj_insert_emplace((shitty_vobj**)&vobj, 0), IsNull());
    EXPECT_THAT(vobj, IsNull());
}

TEST_F(NAME, push_few_values_works)
{
    EXPECT_THAT(vobj_push(&vobj, obj{5, 5, 5, 5}), Eq(0));
    EXPECT_THAT(vobj_push(&vobj, obj{7, 7, 7, 7}), Eq(0));
    EXPECT_THAT(vobj_push(&vobj, obj{3, 3, 3, 3}), Eq(0));
    EXPECT_THAT(vec_count(vobj), Eq(3));
    EXPECT_THAT(vec_get(vobj, 0), Pointee(obj{5, 5, 5, 5}));
    EXPECT_THAT(vec_get(vobj, 1), Pointee(obj{7, 7, 7, 7}));
    EXPECT_THAT(vec_get(vobj, 2), Pointee(obj{3, 3, 3, 3}));
}
TEST_F(NAME, emplace_few_values_works)
{
    *vobj_emplace(&vobj) = obj{5, 5, 5, 5};
    *vobj_emplace(&vobj) = obj{7, 7, 7, 7};
    *vobj_emplace(&vobj) = obj{3, 3, 3, 3};
    EXPECT_THAT(vec_count(vobj), Eq(3));
    EXPECT_THAT(vec_get(vobj, 0), Pointee(obj{5, 5, 5, 5}));
    EXPECT_THAT(vec_get(vobj, 1), Pointee(obj{7, 7, 7, 7}));
    EXPECT_THAT(vec_get(vobj, 2), Pointee(obj{3, 3, 3, 3}));
}
TEST_F(NAME, insert_few_values_works)
{
    ASSERT_THAT(vobj_insert(&vobj, 0, obj{5, 5, 5, 5}), Eq(0));
    ASSERT_THAT(vobj_insert(&vobj, 0, obj{7, 7, 7, 7}), Eq(0));
    ASSERT_THAT(vobj_insert(&vobj, 0, obj{3, 3, 3, 3}), Eq(0));
    EXPECT_THAT(vec_count(vobj), Eq(3));
    EXPECT_THAT(vec_get(vobj, 0), Pointee(obj{3, 3, 3, 3}));
    EXPECT_THAT(vec_get(vobj, 1), Pointee(obj{7, 7, 7, 7}));
    EXPECT_THAT(vec_get(vobj, 2), Pointee(obj{5, 5, 5, 5}));
}
TEST_F(NAME, insert_emplace_few_values_works)
{
    *vobj_insert_emplace(&vobj, 0) = obj{5, 5, 5, 5};
    *vobj_insert_emplace(&vobj, 0) = obj{7, 7, 7, 7};
    *vobj_insert_emplace(&vobj, 0) = obj{3, 3, 3, 3};
    EXPECT_THAT(vec_count(vobj), Eq(3));
    EXPECT_THAT(vec_get(vobj, 0), Pointee(obj{3, 3, 3, 3}));
    EXPECT_THAT(vec_get(vobj, 1), Pointee(obj{7, 7, 7, 7}));
    EXPECT_THAT(vec_get(vobj, 2), Pointee(obj{5, 5, 5, 5}));
}

TEST_F(NAME, push_with_expand_sets_count_and_capacity_correctly)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        ASSERT_THAT(vobj_push(&vobj, obj{i, i, i, i}), Eq(0));

    EXPECT_THAT(vec_count(vobj), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY));

    ASSERT_THAT(vobj_push(&vobj, obj{42, 42, 42, 42}), Eq(0));
    EXPECT_THAT(vec_count(vobj), Eq(MIN_CAPACITY + 1));
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY * EXPAND_FACTOR));
}
TEST_F(NAME, emplace_with_expand_sets_count_and_capacity_correctly)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        ASSERT_THAT(vobj_emplace(&vobj), NotNull());

    EXPECT_THAT(vec_count(vobj), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY));

    ASSERT_THAT(vobj_emplace(&vobj), NotNull());
    EXPECT_THAT(vec_count(vobj), Eq(MIN_CAPACITY + 1));
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY * EXPAND_FACTOR));
}
TEST_F(NAME, insert_with_expand_sets_count_and_capacity_correctly)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        ASSERT_THAT(vobj_insert(&vobj, 0, obj{i, i, i, i}), Eq(0));

    EXPECT_THAT(vec_count(vobj), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY));

    ASSERT_THAT(vobj_insert(&vobj, 3, obj{42, 42, 42, 42}), Eq(0));
    EXPECT_THAT(vec_count(vobj), Eq(MIN_CAPACITY + 1));
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY * EXPAND_FACTOR));
}
TEST_F(NAME, insert_emplace_with_expand_sets_count_and_capacity_correctly)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        ASSERT_THAT(vobj_insert_emplace(&vobj, 0), NotNull());

    EXPECT_THAT(vec_count(vobj), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY));

    ASSERT_THAT(vobj_insert_emplace(&vobj, 3), NotNull());
    EXPECT_THAT(vec_count(vobj), Eq(MIN_CAPACITY + 1));
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY * EXPAND_FACTOR));
}

TEST_F(NAME, push_with_expand_has_correct_values)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        ASSERT_THAT(vobj_push(&vobj, obj{i, i, i, i}), Eq(0));
    ASSERT_THAT(vobj_push(&vobj, obj{42, 42, 42, 42}), Eq(0));

    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        EXPECT_THAT(vec_get(vobj, i), Pointee(obj{i, i, i, i}));
    EXPECT_THAT(vec_get(vobj, MIN_CAPACITY), Pointee(obj{42, 42, 42, 42}));
}
TEST_F(NAME, emplace_with_expand_has_correct_values)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        *vobj_emplace(&vobj) = obj{i, i, i, i};
    *vobj_emplace(&vobj) = obj{42, 42, 42, 42};

    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        EXPECT_THAT(vec_get(vobj, i), Pointee(obj{i, i, i, i}));
    EXPECT_THAT(vec_get(vobj, MIN_CAPACITY), Pointee(obj{42, 42, 42, 42}));
}
TEST_F(NAME, insert_with_expand_has_correct_values)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        ASSERT_THAT(vobj_insert(&vobj, 0, obj{i, i, i, i}), Eq(0));
    ASSERT_THAT(vobj_insert(&vobj, 3, obj{42, 42, 42, 42}), Eq(0));

    for (uint64_t i = 0; i != 3; ++i)
        EXPECT_THAT(
            vec_get(vobj, i),
            Pointee(
                obj{MIN_CAPACITY - i - 1,
                    MIN_CAPACITY - i - 1,
                    MIN_CAPACITY - i - 1,
                    MIN_CAPACITY - i - 1}));
    EXPECT_THAT(vec_get(vobj, 3), Pointee(obj{42, 42, 42, 42}));
    for (uint64_t i = 4; i != MIN_CAPACITY + 1; ++i)
        EXPECT_THAT(
            vec_get(vobj, i),
            Pointee(
                obj{MIN_CAPACITY - i,
                    MIN_CAPACITY - i,
                    MIN_CAPACITY - i,
                    MIN_CAPACITY - i}));
}
TEST_F(NAME, insert_emplace_with_expand_has_correct_values)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        *vobj_insert_emplace(&vobj, 0) = obj{i, i, i, i};
    *vobj_insert_emplace(&vobj, 3) = obj{42, 42, 42, 42};

    for (uint64_t i = 0; i != 3; ++i)
        EXPECT_THAT(
            vec_get(vobj, i),
            Pointee(
                obj{MIN_CAPACITY - i - 1,
                    MIN_CAPACITY - i - 1,
                    MIN_CAPACITY - i - 1,
                    MIN_CAPACITY - i - 1}));
    EXPECT_THAT(vec_get(vobj, 3), Pointee(obj{42, 42, 42, 42}));
    for (uint64_t i = 4; i != MIN_CAPACITY + 1; ++i)
        EXPECT_THAT(
            vec_get(vobj, i),
            Pointee(
                obj{MIN_CAPACITY - i,
                    MIN_CAPACITY - i,
                    MIN_CAPACITY - i,
                    MIN_CAPACITY - i}));
}

TEST_F(NAME, push_expand_with_failed_realloc_returns_error)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        ASSERT_THAT(vobj_push(&vobj, obj{i, i, i, i}), Eq(0));

    EXPECT_THAT(vec_count(vobj), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY));

    EXPECT_THAT(
        shitty_vobj_push((shitty_vobj**)&vobj, obj{42, 42, 42, 42}), Eq(-1));
    EXPECT_THAT(vec_count(vobj), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY));
}
TEST_F(NAME, emplace_expand_with_failed_realloc_returns_error)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        ASSERT_THAT(vobj_emplace(&vobj), NotNull());

    EXPECT_THAT(vec_count(vobj), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY));

    EXPECT_THAT(shitty_vobj_emplace((shitty_vobj**)&vobj), IsNull());
    EXPECT_THAT(vec_count(vobj), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY));
}
TEST_F(NAME, insert_expand_with_failed_realloc_returns_error)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        ASSERT_THAT(vobj_insert(&vobj, 0, obj{i, i, i, i}), Eq(0));

    EXPECT_THAT(vec_count(vobj), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY));

    EXPECT_THAT(
        shitty_vobj_insert((shitty_vobj**)&vobj, 3, obj{42, 42, 42, 42}),
        Eq(-1));
    EXPECT_THAT(vec_count(vobj), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY));
}
TEST_F(NAME, insert_emplace_expand_with_failed_realloc_returns_error)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        ASSERT_THAT(vobj_insert_emplace(&vobj, 0), NotNull());

    EXPECT_THAT(vec_count(vobj), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY));

    EXPECT_THAT(shitty_vobj_insert_emplace((shitty_vobj**)&vobj, 3), IsNull());
    EXPECT_THAT(vec_count(vobj), Eq(MIN_CAPACITY));
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY));
}

TEST_F(NAME, inserting_preserves_existing_elements)
{
    vobj_push(&vobj, obj{53, 53, 53, 53});
    vobj_push(&vobj, obj{24, 24, 24, 24});
    vobj_push(&vobj, obj{73, 73, 73, 73});
    vobj_push(&vobj, obj{43, 43, 43, 43});
    vobj_push(&vobj, obj{65, 65, 65, 65});

    vobj_insert(&vobj, 2, obj{68, 68, 68, 68}); // middle insertion
    EXPECT_THAT(vec_get(vobj, 0), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_get(vobj, 1), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(vec_get(vobj, 2), Pointee(obj{68, 68, 68, 68}));
    EXPECT_THAT(vec_get(vobj, 3), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_get(vobj, 4), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(vobj, 5), Pointee(obj{65, 65, 65, 65}));

    vobj_insert(&vobj, 0, obj{16, 16, 16, 16}); // beginning insertion
    EXPECT_THAT(vec_get(vobj, 0), Pointee(obj{16, 16, 16, 16}));
    EXPECT_THAT(vec_get(vobj, 1), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_get(vobj, 2), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(vec_get(vobj, 3), Pointee(obj{68, 68, 68, 68}));
    EXPECT_THAT(vec_get(vobj, 4), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_get(vobj, 5), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(vobj, 6), Pointee(obj{65, 65, 65, 65}));

    vobj_insert(&vobj, 7, obj{82, 82, 82, 82}); // end insertion
    EXPECT_THAT(vec_get(vobj, 0), Pointee(obj{16, 16, 16, 16}));
    EXPECT_THAT(vec_get(vobj, 1), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_get(vobj, 2), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(vec_get(vobj, 3), Pointee(obj{68, 68, 68, 68}));
    EXPECT_THAT(vec_get(vobj, 4), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_get(vobj, 5), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(vobj, 6), Pointee(obj{65, 65, 65, 65}));
    EXPECT_THAT(vec_get(vobj, 7), Pointee(obj{82, 82, 82, 82}));

    vobj_insert(&vobj, 7, obj{37, 37, 37, 37}); // end insertion
    EXPECT_THAT(vec_get(vobj, 0), Pointee(obj{16, 16, 16, 16}));
    EXPECT_THAT(vec_get(vobj, 1), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_get(vobj, 2), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(vec_get(vobj, 3), Pointee(obj{68, 68, 68, 68}));
    EXPECT_THAT(vec_get(vobj, 4), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_get(vobj, 5), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(vobj, 6), Pointee(obj{65, 65, 65, 65}));
    EXPECT_THAT(vec_get(vobj, 7), Pointee(obj{37, 37, 37, 37}));
    EXPECT_THAT(vec_get(vobj, 8), Pointee(obj{82, 82, 82, 82}));
}

TEST_F(NAME, insert_emplacing_preserves_existing_elements)
{
    vobj_push(&vobj, obj{53, 53, 53, 53});
    vobj_push(&vobj, obj{24, 24, 24, 24});
    vobj_push(&vobj, obj{73, 73, 73, 73});
    vobj_push(&vobj, obj{43, 43, 43, 43});
    vobj_push(&vobj, obj{65, 65, 65, 65});

    *vobj_insert_emplace(&vobj, 2) = obj{68, 68, 68, 68}; // middle insertion
    EXPECT_THAT(vec_get(vobj, 0), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_get(vobj, 1), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(vec_get(vobj, 2), Pointee(obj{68, 68, 68, 68}));
    EXPECT_THAT(vec_get(vobj, 3), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_get(vobj, 4), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(vobj, 5), Pointee(obj{65, 65, 65, 65}));

    *vobj_insert_emplace(&vobj, 0) = obj{16, 16, 16, 16}; // beginning insertion
    EXPECT_THAT(vec_get(vobj, 0), Pointee(obj{16, 16, 16, 16}));
    EXPECT_THAT(vec_get(vobj, 1), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_get(vobj, 2), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(vec_get(vobj, 3), Pointee(obj{68, 68, 68, 68}));
    EXPECT_THAT(vec_get(vobj, 4), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_get(vobj, 5), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(vobj, 6), Pointee(obj{65, 65, 65, 65}));

    *vobj_insert_emplace(&vobj, 7) = obj{82, 82, 82, 82}; // end insertion
    EXPECT_THAT(vec_get(vobj, 0), Pointee(obj{16, 16, 16, 16}));
    EXPECT_THAT(vec_get(vobj, 1), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_get(vobj, 2), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(vec_get(vobj, 3), Pointee(obj{68, 68, 68, 68}));
    EXPECT_THAT(vec_get(vobj, 4), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_get(vobj, 5), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(vobj, 6), Pointee(obj{65, 65, 65, 65}));
    EXPECT_THAT(vec_get(vobj, 7), Pointee(obj{82, 82, 82, 82}));

    *vobj_insert_emplace(&vobj, 7) = obj{37, 37, 37, 37}; // end insertion
    EXPECT_THAT(vec_get(vobj, 0), Pointee(obj{16, 16, 16, 16}));
    EXPECT_THAT(vec_get(vobj, 1), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_get(vobj, 2), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(vec_get(vobj, 3), Pointee(obj{68, 68, 68, 68}));
    EXPECT_THAT(vec_get(vobj, 4), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_get(vobj, 5), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(vobj, 6), Pointee(obj{65, 65, 65, 65}));
    EXPECT_THAT(vec_get(vobj, 7), Pointee(obj{37, 37, 37, 37}));
    EXPECT_THAT(vec_get(vobj, 8), Pointee(obj{82, 82, 82, 82}));
}

TEST_F(NAME, clear_null_vector_works)
{
    vobj_clear(vobj);
}

TEST_F(NAME, clear_keeps_buffer_and_resets_count)
{
    for (uint64_t i = 0; i != MIN_CAPACITY * 2; ++i)
        ASSERT_THAT(vobj_push(&vobj, obj{i, i, i, i}), Eq(0));

    EXPECT_THAT(vec_count(vobj), Eq(MIN_CAPACITY * 2));
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY * 2));
    vobj_clear(vobj);
    EXPECT_THAT(vec_count(vobj), Eq(0u));
    EXPECT_THAT(vec_capacity(vobj), Eq(MIN_CAPACITY * 2));
}

TEST_F(NAME, compact_null_vector_works)
{
    vobj_compact(&vobj);
}

TEST_F(NAME, compact_sets_capacity)
{
    vobj_push(&vobj, obj{9, 9, 9, 9});
    vobj_compact(&vobj);
    ASSERT_THAT(vobj, NotNull());
    EXPECT_THAT(vec_count(vobj), Eq(1));
    EXPECT_THAT(vec_capacity(vobj), Eq(1));
}

TEST_F(NAME, compact_removes_excess_space)
{
    vobj_push(&vobj, obj{1, 1, 1, 1});
    vobj_push(&vobj, obj{2, 2, 2, 2});
    vobj_push(&vobj, obj{3, 3, 3, 3});
    vobj_pop(vobj);
    vobj_pop(vobj);
    vobj_compact(&vobj);
    ASSERT_THAT(vobj, NotNull());
    EXPECT_THAT(vec_count(vobj), Eq(1));
    EXPECT_THAT(vec_capacity(vobj), Eq(1));
}

TEST_F(NAME, clear_and_compact_deletes_buffer)
{
    vobj_push(&vobj, obj{9, 9, 9, 9});
    vobj_clear(vobj);
    vobj_compact(&vobj);
    EXPECT_THAT(vobj, IsNull());
}

TEST_F(NAME, clear_compact_null_vector_works)
{
    vobj_clear_compact(&vobj);
}

TEST_F(NAME, clear_compact_deletes_buffer)
{
    vobj_push(&vobj, obj{9, 9, 9, 9});
    vobj_clear_compact(&vobj);
    EXPECT_THAT(vobj, IsNull());
}

TEST_F(NAME, pop_returns_pushed_values)
{
    vobj_push(&vobj, obj{3, 3, 3, 3});
    vobj_push(&vobj, obj{2, 2, 2, 2});
    vobj_push(&vobj, obj{6, 6, 6, 6});
    EXPECT_THAT(vobj_pop(vobj), Pointee(obj{6, 6, 6, 6}));
    vobj_push(&vobj, obj{23, 23, 23, 23});
    vobj_push(&vobj, obj{21, 21, 21, 21});
    EXPECT_THAT(vobj_pop(vobj), Pointee(obj{21, 21, 21, 21}));
    EXPECT_THAT(vobj_pop(vobj), Pointee(obj{23, 23, 23, 23}));
    EXPECT_THAT(vobj_pop(vobj), Pointee(obj{2, 2, 2, 2}));
    EXPECT_THAT(vobj_pop(vobj), Pointee(obj{3, 3, 3, 3}));

    ASSERT_THAT(vobj, NotNull());
    EXPECT_THAT(vec_count(vobj), Eq(0u));
}

TEST_F(NAME, pop_returns_emplaced_values)
{
    *vobj_emplace(&vobj) = obj{53, 53, 53, 53};
    *vobj_emplace(&vobj) = obj{24, 24, 24, 24};
    *vobj_emplace(&vobj) = obj{73, 73, 73, 73};
    EXPECT_THAT(vobj_pop(vobj), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_count(vobj), Eq(2));
    *vobj_emplace(&vobj) = obj{28, 28, 28, 28};
    *vobj_emplace(&vobj) = obj{72, 72, 72, 72};
    EXPECT_THAT(vobj_pop(vobj), Pointee(obj{72, 72, 72, 72}));
    EXPECT_THAT(vobj_pop(vobj), Pointee(obj{28, 28, 28, 28}));
    EXPECT_THAT(vobj_pop(vobj), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(vobj_pop(vobj), Pointee(obj{53, 53, 53, 53}));

    EXPECT_THAT(vec_count(vobj), Eq(0u));
    EXPECT_THAT(vobj, NotNull());
}

TEST_F(NAME, popping_preserves_existing_elements)
{
    vobj_push(&vobj, obj{53, 53, 53, 53});
    vobj_push(&vobj, obj{24, 24, 24, 24});
    vobj_push(&vobj, obj{73, 73, 73, 73});
    vobj_push(&vobj, obj{43, 43, 43, 43});
    vobj_push(&vobj, obj{24, 24, 24, 24});

    vobj_pop(vobj);
    EXPECT_THAT(vec_get(vobj, 1), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(vec_get(vobj, 3), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(vobj, 2), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_get(vobj, 0), Pointee(obj{53, 53, 53, 53}));
}

TEST_F(NAME, get_last_element)
{
    vobj_push(&vobj, obj{53, 53, 53, 53});
    EXPECT_THAT((vec_last(vobj)), Pointee(obj{53, 53, 53, 53}));
    vobj_push(&vobj, obj{24, 24, 24, 24});
    EXPECT_THAT((vec_last(vobj)), Pointee(obj{24, 24, 24, 24}));
    vobj_push(&vobj, obj{73, 73, 73, 73});
    EXPECT_THAT((vec_last(vobj)), Pointee(obj{73, 73, 73, 73}));
}

TEST_F(NAME, get_first_element)
{
    vobj_push(&vobj, obj{53, 53, 53, 53});
    EXPECT_THAT((vec_first(vobj)), Pointee(obj{53, 53, 53, 53}));
    vobj_push(&vobj, obj{24, 24, 24, 24});
    EXPECT_THAT((vec_first(vobj)), Pointee(obj{53, 53, 53, 53}));
    vobj_push(&vobj, obj{73, 73, 73, 73});
    EXPECT_THAT((vec_first(vobj)), Pointee(obj{53, 53, 53, 53}));
}

TEST_F(NAME, get_element_random_access)
{
    vobj_push(&vobj, obj{53, 53, 53, 53});
    vobj_push(&vobj, obj{24, 24, 24, 24});
    vobj_push(&vobj, obj{73, 73, 73, 73});
    vobj_push(&vobj, obj{43, 43, 43, 43});
    EXPECT_THAT((vec_get(vobj, 1)), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT((vec_get(vobj, 3)), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT((vec_get(vobj, 2)), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT((vec_get(vobj, 0)), Pointee(obj{53, 53, 53, 53}));
}

TEST_F(NAME, rget_element_random_access)
{
    vobj_push(&vobj, obj{53, 53, 53, 53});
    vobj_push(&vobj, obj{24, 24, 24, 24});
    vobj_push(&vobj, obj{73, 73, 73, 73});
    vobj_push(&vobj, obj{43, 43, 43, 43});
    EXPECT_THAT(vec_rget(vobj, 1), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_rget(vobj, 3), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_rget(vobj, 2), Pointee(obj{24, 24, 24, 24}));
    EXPECT_THAT(vec_rget(vobj, 0), Pointee(obj{43, 43, 43, 43}));
}

TEST_F(NAME, erasing_by_index_preserves_existing_elements)
{
    vobj_push(&vobj, obj{53, 53, 53, 53});
    vobj_push(&vobj, obj{24, 24, 24, 24});
    vobj_push(&vobj, obj{73, 73, 73, 73});
    vobj_push(&vobj, obj{43, 43, 43, 43});
    vobj_push(&vobj, obj{65, 65, 65, 65});

    vobj_erase(vobj, 1);
    EXPECT_THAT(vec_get(vobj, 0), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_get(vobj, 1), Pointee(obj{73, 73, 73, 73}));
    EXPECT_THAT(vec_get(vobj, 2), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(vobj, 3), Pointee(obj{65, 65, 65, 65}));

    vobj_erase(vobj, 1);
    EXPECT_THAT(vec_get(vobj, 0), Pointee(obj{53, 53, 53, 53}));
    EXPECT_THAT(vec_get(vobj, 1), Pointee(obj{43, 43, 43, 43}));
    EXPECT_THAT(vec_get(vobj, 2), Pointee(obj{65, 65, 65, 65}));
}

TEST_F(NAME, for_each_zero_elements)
{
    obj* value;
    int  counter = 0;
    vec_for_each (vobj, value)
        counter++;

    EXPECT_THAT(counter, Eq(0));
}

TEST_F(NAME, for_each_one_element)
{
    vobj_push(&vobj, obj{1, 1, 1, 1});

    int  counter = 0;
    obj* value;
    vec_for_each (vobj, value)
    {
        counter++;
        EXPECT_THAT(value, Pointee(obj{1, 1, 1, 1}));
    }

    EXPECT_THAT(counter, Eq(1));
}

TEST_F(NAME, for_each_three_elements)
{
    vobj_push(&vobj, obj{1, 1, 1, 1});
    vobj_push(&vobj, obj{2, 2, 2, 2});
    vobj_push(&vobj, obj{3, 3, 3, 3});

    uint64_t counter = 0;
    obj*     value;
    vec_for_each (vobj, value)
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
        vobj_retain(
            vobj,
            [](obj*, void* counter)
            {
                ++*(int*)counter;
                return VEC_RETAIN;
            },
            &counter),
        Eq(0));
    EXPECT_THAT(vec_count(vobj), Eq(0));
    EXPECT_THAT(counter, Eq(0));
}

TEST_F(NAME, retain_all)
{
    for (uint64_t i = 0; i != 8; ++i)
        vobj_push(&vobj, obj{i, i, i, i});

    int counter = 0;
    EXPECT_THAT(vec_count(vobj), Eq(8));
    EXPECT_THAT(
        vobj_retain(
            vobj,
            [](obj*, void* counter)
            {
                ++*(int*)counter;
                return VEC_RETAIN;
            },
            &counter),
        Eq(0));
    EXPECT_THAT(vec_count(vobj), Eq(8));
    EXPECT_THAT(counter, Eq(8));
}

TEST_F(NAME, retain_half)
{
    for (uint64_t i = 0; i != 8; ++i)
        vobj_push(&vobj, obj{i, i, i, i});

    EXPECT_THAT(vec_count(vobj), Eq(8));
    int counter = 0;
    EXPECT_THAT(
        vobj_retain(
            vobj,
            [](obj*, void* user) { return (*(int*)user)++ % 2; },
            &counter),
        Eq(0));
    EXPECT_THAT(vec_count(vobj), Eq(4));
    EXPECT_THAT(counter, Eq(8));
    EXPECT_THAT(vec_get(vobj, 0), Pointee(obj{0, 0, 0, 0}));
    EXPECT_THAT(vec_get(vobj, 1), Pointee(obj{2, 2, 2, 2}));
    EXPECT_THAT(vec_get(vobj, 2), Pointee(obj{4, 4, 4, 4}));
    EXPECT_THAT(vec_get(vobj, 3), Pointee(obj{6, 6, 6, 6}));
}

TEST_F(NAME, retain_returning_error)
{
    for (uint64_t i = 0; i != 8; ++i)
        vobj_push(&vobj, obj{i, i, i, i});

    EXPECT_THAT(vec_count(vobj), Eq(8));
    EXPECT_THAT(
        vobj_retain(vobj, [](obj*, void*) { return -5; }, NULL), Eq(-5));
    EXPECT_THAT(vec_count(vobj), Eq(8));
}

TEST_F(NAME, insert_up_to_realloc_doesnt_cause_invalid_memmove)
{
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        vobj_insert(&vobj, i, obj{i, i, i, i});
    for (uint64_t i = 0; i != MIN_CAPACITY; ++i)
        EXPECT_THAT(vobj->data[i], Eq(obj{i, i, i, i}));
}

TEST_F(NAME, realloc_from_0_to_0)
{
    vobj_realloc(&vobj, 0);
    EXPECT_THAT(vobj, IsNull());
}

TEST_F(NAME, realloc_from_8_to_0)
{
    vobj_realloc(&vobj, 8);
    EXPECT_THAT(vec_count(vobj), Eq(0));
    EXPECT_THAT(vec_capacity(vobj), Eq(8));
    EXPECT_THAT(vobj->data, NotNull());

    vobj_realloc(&vobj, 0);
    EXPECT_THAT(vec_count(vobj), Eq(0));
    EXPECT_THAT(vec_capacity(vobj), Eq(0));
    EXPECT_THAT(vobj, IsNull());
}
