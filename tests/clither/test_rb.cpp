#include "gmock/gmock.h"

extern "C" {
#include "clither/util/rb.h"
}

struct obj
{
    uint64_t a, b, c, d;
};

bool operator==(const struct obj& a, const struct obj& b)
{
    return a.a == b.a && a.b == b.b && a.c == b.c && a.d == b.d;
}

RB_DECLARE(obj_rb, struct obj, 16)
RB_DEFINE(obj_rb, struct obj, 16)

#define NAME test_rb

using namespace testing;

struct NAME : public Test
{
    void SetUp() override
    {
        obj_rb_init(&obj_rb);
        obj_rb_resize(&obj_rb, 32), Eq(0);
    }

    void TearDown() override { obj_rb_deinit(obj_rb); }

    struct obj_rb* obj_rb;
};

TEST_F(NAME, resize_initializes_members)
{
    obj_rb_deinit(obj_rb);
    obj_rb_init(&obj_rb);
    ASSERT_THAT(obj_rb_resize(&obj_rb, 32), Eq(0));
    ASSERT_THAT(obj_rb, NotNull());
    ASSERT_THAT(obj_rb->capacity, Eq(32));
    ASSERT_THAT(obj_rb->read, Eq(0));
    ASSERT_THAT(obj_rb->write, Eq(0));
}

TEST_F(NAME, space_left)
{
    obj_rb->read = 0;
    obj_rb->write = 0;
    EXPECT_THAT(
        rb_space(obj_rb),
        Eq(32 - 1)); // 1 slot is always "used" to detect when buffer is full
    obj_rb->read = 5;
    obj_rb->write = 8;
    EXPECT_THAT(rb_space(obj_rb), Eq(32 - 4));
    obj_rb->read = 8;
    obj_rb->write = 5;
    EXPECT_THAT(rb_space(obj_rb), Eq(2));
    obj_rb->read = 16;
    obj_rb->write = 15;
    EXPECT_THAT(rb_space(obj_rb), Eq(0));

    obj_rb->read = 0;
    obj_rb->write = 32 - 1;
    EXPECT_THAT(rb_space(obj_rb), Eq(0));

    obj_rb->read = 32 - 1;
    obj_rb->write = 0;
    EXPECT_THAT(rb_space(obj_rb), Eq(32 - 2));
}

TEST_F(NAME, is_full_and_is_empty_macros)
{
    obj_rb->read = 2;
    obj_rb->write = 2;
    EXPECT_THAT(rb_is_full(obj_rb), IsFalse());
    EXPECT_THAT(rb_is_empty(obj_rb), IsTrue());
    EXPECT_THAT(rb_is_full(obj_rb), IsFalse());
    EXPECT_THAT(rb_is_empty(obj_rb), IsTrue());

    obj_rb->read = 2;
    obj_rb->write = 3;
    EXPECT_THAT(rb_is_full(obj_rb), IsFalse());
    EXPECT_THAT(rb_is_empty(obj_rb), IsFalse());
    EXPECT_THAT(rb_is_full(obj_rb), IsFalse());
    EXPECT_THAT(rb_is_empty(obj_rb), IsFalse());

    obj_rb->read = 3;
    obj_rb->write = 2;
    EXPECT_THAT(rb_is_full(obj_rb), IsTrue());
    EXPECT_THAT(rb_is_empty(obj_rb), IsFalse());
    EXPECT_THAT(rb_is_full(obj_rb), IsTrue());
    EXPECT_THAT(rb_is_empty(obj_rb), IsFalse());

    obj_rb->read = 0;
    obj_rb->write = 32 - 1;
    EXPECT_THAT(rb_is_full(obj_rb), IsTrue());
    EXPECT_THAT(rb_is_empty(obj_rb), IsFalse());
    EXPECT_THAT(rb_is_full(obj_rb), IsTrue());
    EXPECT_THAT(rb_is_empty(obj_rb), IsFalse());

    obj_rb->read = 32 - 1;
    obj_rb->write = 0;
    EXPECT_THAT(rb_is_full(obj_rb), IsFalse());
    EXPECT_THAT(rb_is_empty(obj_rb), IsFalse());
    EXPECT_THAT(rb_is_full(obj_rb), IsFalse());
    EXPECT_THAT(rb_is_empty(obj_rb), IsFalse());
}

TEST_F(NAME, put)
{
    EXPECT_THAT(rb_count(obj_rb), Eq(0));

    EXPECT_THAT(obj_rb_put(obj_rb, obj{0xa, 0xa, 0xa, 0xa}), Eq(0));
    EXPECT_THAT(obj_rb_put(obj_rb, obj{0xb, 0xb, 0xb, 0xb}), Eq(0));
    EXPECT_THAT(obj_rb_put(obj_rb, obj{0xc, 0xc, 0xc, 0xc}), Eq(0));

    EXPECT_THAT(obj_rb->read, Eq(0));
    EXPECT_THAT(rb_count(obj_rb), Eq(3));
    EXPECT_THAT(obj_rb->write, Eq(3));
}

TEST_F(NAME, take)
{
    EXPECT_THAT(obj_rb_put(obj_rb, obj{0xa, 0xa, 0xa, 0xa}), Eq(0));
    EXPECT_THAT(obj_rb_put(obj_rb, obj{0xb, 0xb, 0xb, 0xb}), Eq(0));
    EXPECT_THAT(obj_rb_put(obj_rb, obj{0xc, 0xc, 0xc, 0xc}), Eq(0));

    ASSERT_THAT(obj_rb_take(obj_rb), Eq(obj{0xa, 0xa, 0xa, 0xa}));
    ASSERT_THAT(obj_rb_take(obj_rb), Eq(obj{0xb, 0xb, 0xb, 0xb}));
    ASSERT_THAT(obj_rb_take(obj_rb), Eq(obj{0xc, 0xc, 0xc, 0xc}));

    EXPECT_THAT(rb_count(obj_rb), Eq(0));
    EXPECT_THAT(obj_rb->read, Eq(3));
    EXPECT_THAT(obj_rb->write, Eq(3));
}

TEST_F(NAME, put_and_take_wrap)
{
    // We want to write an odd number of bytes to the ring buffer,
    // so all possible ways in which the pointers wrap are tested.
    // For this to work, the buffer size must be an even number.
    ASSERT_THAT(obj_rb->capacity % 2, Eq(0));

    for (int i = 0; i != obj_rb->capacity * 64; ++i)
    {
        // write 3 bytes
        EXPECT_THAT(obj_rb_put(obj_rb, obj{0xa, 0xa, 0xa, 0xa}), Eq(0));
        EXPECT_THAT(obj_rb_put(obj_rb, obj{0xb, 0xb, 0xb, 0xb}), Eq(0));
        EXPECT_THAT(obj_rb_put(obj_rb, obj{0xc, 0xc, 0xc, 0xc}), Eq(0));

        // read 3 bytes
        EXPECT_THAT(obj_rb_take(obj_rb), Eq(obj{0xa, 0xa, 0xa, 0xa}));
        EXPECT_THAT(obj_rb_take(obj_rb), Eq(obj{0xb, 0xb, 0xb, 0xb}));
        EXPECT_THAT(obj_rb_take(obj_rb), Eq(obj{0xc, 0xc, 0xc, 0xc}));

        EXPECT_THAT(rb_count(obj_rb), Eq(0));
        EXPECT_THAT(obj_rb->write, Eq(obj_rb->read));
        EXPECT_THAT(obj_rb->write, Lt(obj_rb->capacity));
        EXPECT_THAT(obj_rb->read, Lt(obj_rb->capacity));
    }
}

TEST_F(NAME, if_it_dont_fit_dont_shit_single)
{
    // We want to test the wraparound behavior of realloc, so move read/write
    // pointers to middle
    obj_rb->read = 16;
    obj_rb->write = 16;

    // Fill the buffer completely
    uint16_t num_left = 32 - 1;
    while (num_left--)
        ASSERT_THAT(
            obj_rb_put(obj_rb, obj{num_left, num_left, num_left, num_left}),
            Eq(0));

    ASSERT_THAT(rb_space(obj_rb), Eq(0));
    ASSERT_THAT(rb_is_full(obj_rb), IsTrue());
    ASSERT_THAT(obj_rb->read, Eq(16));
    ASSERT_THAT(obj_rb->write, Eq(15));

    // Write one entry, this should cause a realloc
    ASSERT_THAT(obj_rb_put_realloc(&obj_rb, obj{0xa, 0xa, 0xa, 0xa}), Eq(0));

    // Check to see if pointers make sense
    ASSERT_THAT(obj_rb->capacity, Eq(64));
    ASSERT_THAT(obj_rb->read, Eq(16));
    ASSERT_THAT(obj_rb->write, Eq(48));

    // Read everything back
    num_left = 32 - 1;
    while (num_left--)
        ASSERT_THAT(
            obj_rb_take(obj_rb), Eq(obj{num_left, num_left, num_left, num_left}));

    ASSERT_THAT(obj_rb_take(obj_rb), Eq(obj{0xa, 0xa, 0xa, 0xa}));

    ASSERT_THAT(rb_space(obj_rb), Eq(obj_rb->capacity - 1));
    ASSERT_THAT(rb_is_empty(obj_rb), IsTrue());
    ASSERT_THAT(obj_rb->read, Eq(48));
    ASSERT_THAT(obj_rb->write, Eq(48));
}

TEST_F(NAME, peek)
{
    // We want to test the wraparound behavior, so move read/write pointers to
    // middle
    obj_rb->read = 16;
    obj_rb->write = 16;

    // Fill the buffer completely
    uint16_t num_left = 32 - 1;
    while (num_left--)
        ASSERT_THAT(
            obj_rb_put(obj_rb, obj{num_left, num_left, num_left, num_left}),
            Eq(0));

    // Read everything back
    num_left = 32 - 1;
    for (int i = 0; i != rb_count(obj_rb); ++i)
    {
        num_left--;
        ASSERT_THAT(
            rb_peek(obj_rb, i),
            Pointee(obj{num_left, num_left, num_left, num_left}))
            << "num_left: " << num_left;
    }
}

TEST_F(NAME, for_each_with_wrap)
{
    // We want to test the wraparound behavior of realloc, so move read/write
    // pointers to middle
    obj_rb->read = 16;
    obj_rb->write = 16;

    // Fill the buffer completely
    uint16_t num_left = 32 - 1;
    while (num_left--)
        ASSERT_THAT(
            obj_rb_put(obj_rb, obj{num_left, num_left, num_left, num_left}),
            Eq(0));

    // Read everything back
    num_left = 32 - 1;
    int         i;
    struct obj* value;
    rb_for_each (obj_rb, i, value)
    {
        num_left--;
        ASSERT_THAT(value, Pointee(obj{num_left, num_left, num_left, num_left}))
            << "num_left: " << num_left;
    }
}
