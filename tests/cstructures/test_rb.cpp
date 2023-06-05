#include "gmock/gmock.h"
#include "cstructures/rb.h"

#define NAME rb

using namespace testing;

TEST(NAME, space_macro)
{
    cs_rb rb;
    rb.capacity = 32;
    rb.read = 0;  rb.write = 0;  EXPECT_THAT(RB_SPACE_N(&rb, 32), Eq(32 - 1));  // 1 slot is always "used" to detect when buffer is full
    rb.read = 5;  rb.write = 8;  EXPECT_THAT(RB_SPACE_N(&rb, 32), Eq(32 - 4));
    rb.read = 8;  rb.write = 5;  EXPECT_THAT(RB_SPACE_N(&rb, 32), Eq(2));
    rb.read = 16; rb.write = 15; EXPECT_THAT(RB_SPACE_N(&rb, 32), Eq(0));

    rb.read = 0; rb.write = 32 - 1;
    EXPECT_THAT(RB_SPACE_N(&rb, 32), Eq(0));

    rb.read = 32 - 1; rb.write = 0;
    EXPECT_THAT(RB_SPACE_N(&rb, 32), Eq(32 - 2));
}

TEST(NAME, space_to_end_macro)
{
    uint8_t result;
    cs_rb rb;
    rb.capacity = 32;
    rb.read = 0; rb.write = 0; RB_SPACE_TO_END_N(result, &rb, 32); EXPECT_THAT(result, Eq(32 - 1));
    rb.read = 5; rb.write = 8; RB_SPACE_TO_END_N(result, &rb, 32); EXPECT_THAT(result, Eq(32 - 8));
    rb.read = 8; rb.write = 5; RB_SPACE_TO_END_N(result, &rb, 32); EXPECT_THAT(result, Eq(2));
    rb.read = 5; rb.write = 5; RB_SPACE_TO_END_N(result, &rb, 32); EXPECT_THAT(result, Eq(32 - 5));
    rb.read = 6; rb.write = 5; RB_SPACE_TO_END_N(result, &rb, 32); EXPECT_THAT(result, Eq(0));

    rb.read = 0; rb.write = 32 - 1;
    RB_SPACE_TO_END_N(result, &rb, 32);
    EXPECT_THAT(result, Eq(0));
}

TEST(NAME, is_full_and_is_empty_macros)
{
    cs_rb rb;
    rb.capacity = 32;
    rb.read = 2; rb.write = 2;
    EXPECT_THAT(RB_IS_FULL_N(&rb, 32), IsFalse());
    EXPECT_THAT(RB_IS_EMPTY_N(&rb, 32), IsTrue());
    EXPECT_THAT(rb_is_full(&rb), IsFalse());
    EXPECT_THAT(rb_is_empty(&rb), IsTrue());

    rb.read = 2; rb.write = 3;
    EXPECT_THAT(RB_IS_FULL_N(&rb, 32), IsFalse());
    EXPECT_THAT(RB_IS_EMPTY_N(&rb, 32), IsFalse());
    EXPECT_THAT(rb_is_full(&rb), IsFalse());
    EXPECT_THAT(rb_is_empty(&rb), IsFalse());

    rb.read = 3; rb.write = 2;
    EXPECT_THAT(RB_IS_FULL_N(&rb, 32), IsTrue());
    EXPECT_THAT(RB_IS_EMPTY_N(&rb, 32), IsFalse());
    EXPECT_THAT(rb_is_full(&rb), IsTrue());
    EXPECT_THAT(rb_is_empty(&rb), IsFalse());

    rb.read = 0; rb.write = 32 - 1;
    EXPECT_THAT(RB_IS_FULL_N(&rb, 32), IsTrue());
    EXPECT_THAT(RB_IS_EMPTY_N(&rb, 32), IsFalse());
    EXPECT_THAT(rb_is_full(&rb), IsTrue());
    EXPECT_THAT(rb_is_empty(&rb), IsFalse());

    rb.read = 32 - 1; rb.write = 0;
    EXPECT_THAT(RB_IS_FULL_N(&rb, 32), IsFalse());
    EXPECT_THAT(RB_IS_EMPTY_N(&rb, 32), IsFalse());
    EXPECT_THAT(rb_is_full(&rb), IsFalse());
    EXPECT_THAT(rb_is_empty(&rb), IsFalse());
}

TEST(NAME, write_some_bytes_single)
{
    cs_rb rb;
    rb_init(&rb, sizeof(uint16_t));
    EXPECT_THAT(rb_count(&rb), Eq(0));

    uint16_t a = 0xA, b = 0xB, c = 0xC;
    EXPECT_THAT(rb_put(&rb, &a), Eq(0));
    EXPECT_THAT(rb_put(&rb, &b), Eq(0));
    EXPECT_THAT(rb_put(&rb, &c), Eq(0));

    EXPECT_THAT(rb.read, Eq(0));
    EXPECT_THAT(rb_count(&rb), Eq(3));
    EXPECT_THAT(rb.write, Eq(3));

    rb_deinit(&rb);
}

TEST(NAME, read_some_bytes_single)
{
    cs_rb rb;
    rb_init(&rb, sizeof(uint16_t));

    uint16_t a = 0xA, b = 0xB, c = 0xC;
    EXPECT_THAT(rb_put(&rb, &a), Eq(0));
    EXPECT_THAT(rb_put(&rb, &b), Eq(0));
    EXPECT_THAT(rb_put(&rb, &c), Eq(0));

    uint16_t *d, *e, *f;
    ASSERT_THAT((d = (uint16_t*)rb_take(&rb)), NotNull());
    ASSERT_THAT((e = (uint16_t*)rb_take(&rb)), NotNull());
    ASSERT_THAT((f = (uint16_t*)rb_take(&rb)), NotNull());
    EXPECT_THAT(rb_take(&rb), IsNull());

    EXPECT_THAT(rb_count(&rb), Eq(0));
    EXPECT_THAT(rb.read, Eq(3));
    EXPECT_THAT(rb.write, Eq(3));

    EXPECT_THAT(d, Pointee(a));
    EXPECT_THAT(e, Pointee(b));
    EXPECT_THAT(f, Pointee(c));

    rb_deinit(&rb);
}

TEST(NAME, read_from_empty_rb_single)
{
    cs_rb rb;
    rb_init(&rb, sizeof(uint16_t));

    uint16_t a = 0xA;
    EXPECT_THAT(rb_put(&rb, &a), Eq(0));

    uint16_t* b;
    EXPECT_THAT((b = (uint16_t*)rb_take(&rb)), NotNull());
    EXPECT_THAT(b, Pointee(a));
    EXPECT_THAT(rb_take(&rb), IsNull());
    EXPECT_THAT(rb_take(&rb), IsNull());
    EXPECT_THAT(rb_take(&rb), IsNull());
    EXPECT_THAT(b, Pointee(a));

    rb_deinit(&rb);
}

TEST(NAME, write_and_read_bytes_with_wrap_around_single)
{
    cs_rb rb;
    rb_init(&rb, sizeof(uint16_t));
    rb_realloc(&rb, 32);

    // We want to write an odd number of bytes to the ring buffer,
    // so all possible ways in which the pointers wrap are tested.
    // For this to work, the buffer size must be an even number.
    ASSERT_THAT(rb.capacity % 2, Eq(0));

    uint32_t a = 0xA, b = 0xB, c = 0xC;
    for (int i = 0; i != rb.capacity * 64; ++i)
    {
        // write 3 bytes
        EXPECT_THAT(rb_put(&rb, &a), Eq(0));
        EXPECT_THAT(rb_put(&rb, &b), Eq(0));
        EXPECT_THAT(rb_put(&rb, &c), Eq(0));

        // read 3 bytes
        EXPECT_THAT((uint16_t*)rb_take(&rb), Pointee(a));
        EXPECT_THAT((uint16_t*)rb_take(&rb), Pointee(b));
        EXPECT_THAT((uint16_t*)rb_take(&rb), Pointee(c));

        EXPECT_THAT(rb_count(&rb), Eq(0));
        EXPECT_THAT(rb.write, Eq(rb.read));
        EXPECT_THAT(rb.write, Lt(rb.capacity));
        EXPECT_THAT(rb.read, Lt(rb.capacity));
    }

    rb_deinit(&rb);
}

TEST(NAME, if_it_dont_fit_dont_shit_single)
{
    cs_rb rb;
    rb_init(&rb, sizeof(uint16_t));
    rb_realloc(&rb, 32);

    // We want to test the wraparound behavior of realloc, so move read/write
    // pointers to middle
    rb.read = 16;
    rb.write = 16;

    // Fill the buffer completely
    uint16_t num_left = 32 - 1;
    while (num_left--)
        ASSERT_THAT(rb_put(&rb, &num_left), Eq(0));

    EXPECT_THAT(RB_SPACE_N(&rb, 32), Eq(0));
    EXPECT_THAT(rb_is_full(&rb), IsTrue());
    EXPECT_THAT(rb.read, Eq(16));
    EXPECT_THAT(rb.write, Eq(15));

    // Write one entry, this should cause a realloc
    uint16_t a = 0xA;
    EXPECT_THAT(rb_put(&rb, &a), Eq(0));

    // Check to see if pointers make sense
    EXPECT_THAT(rb.capacity, Eq(64));
    EXPECT_THAT(rb.read, Eq(16));
    EXPECT_THAT(rb.write, Eq(48));

    // Read everything back
    num_left = 32 - 1;
    while (num_left--)
        EXPECT_THAT((uint16_t*)rb_take(&rb), Pointee(num_left));

    EXPECT_THAT((uint16_t*)rb_take(&rb), Pointee(a));

    EXPECT_THAT(RB_SPACE_N(&rb, rb.capacity), Eq(rb.capacity - 1));
    EXPECT_THAT(rb_is_empty(&rb), IsTrue());
    EXPECT_THAT(rb.read, Eq(48));
    EXPECT_THAT(rb.write, Eq(48));

    rb_deinit(&rb);
}

TEST(NAME, for_each_with_wrap)
{
    cs_rb rb;
    rb_init(&rb, sizeof(uint16_t));
    rb_realloc(&rb, 32);

    // We want to test the wraparound behavior of realloc, so move read/write
    // pointers to middle
    rb.read = 16;
    rb.write = 16;

    // Fill the buffer completely
    uint16_t num_left = 32 - 1;
    while (num_left--)
        ASSERT_THAT(rb_put(&rb, &num_left), Eq(0));

    // Read everything back
    num_left = 32 - 1;
    RB_FOR_EACH(&rb, uint16_t, value)
        EXPECT_THAT(value, Pointee(num_left));
        num_left--;
    RB_END_EACH
}
