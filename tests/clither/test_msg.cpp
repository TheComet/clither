#include "gmock/gmock.h"

#include "clither/controls.h"
#include "clither/msg.h"

#define NAME msg

using namespace testing;

bool operator==(const struct controls& a, const struct controls& b)
{
    return a.action == b.action && a.action == b.action && a.speed == b.speed;
}

TEST(NAME, compress_single_controls)
{
    struct controls_rb rb;
    controls_rb_init(&rb);

    struct controls c0 = {25, 8, 0};

    controls_rb_put(&rb, &c0, 20);

    struct msg* m = msg_controls(&rb);
    controls_rb_clear(&rb);

    uint16_t first_frame, last_frame;
    ASSERT_THAT(msg_controls_unpack_into(&rb, m->payload, m->payload_len, 15, &first_frame, &last_frame), Eq(0));
    msg_free(m);

    ASSERT_THAT(controls_rb_count(&rb), Eq(1));
    EXPECT_THAT(controls_rb_find_or_predict(&rb, 20), Pointee(c0));

    controls_rb_deinit(&rb);
}

TEST(NAME, compress_multiple_controls)
{
    struct controls_rb rb;
    controls_rb_init(&rb);

    struct controls c0 = {25, 8, 1};
    struct controls c1 = {28, 20, 0};
    struct controls c2 = {31, 30, 2};
    struct controls c3 = {34, 40, 3};

    controls_rb_put(&rb, &c0, 20);
    controls_rb_put(&rb, &c1, 21);
    controls_rb_put(&rb, &c2, 22);
    controls_rb_put(&rb, &c3, 23);

    struct msg* m = msg_controls(&rb);
    controls_rb_clear(&rb);

    uint16_t first_frame, last_frame;
    ASSERT_THAT(msg_controls_unpack_into(&rb, m->payload, m->payload_len, 15, &first_frame, &last_frame), Eq(0));
    msg_free(m);

    ASSERT_THAT(controls_rb_count(&rb), Eq(4));
    EXPECT_THAT(controls_rb_find_or_predict(&rb, 20), Pointee(c0));
    EXPECT_THAT(controls_rb_find_or_predict(&rb, 21), Pointee(c1));
    EXPECT_THAT(controls_rb_find_or_predict(&rb, 22), Pointee(c2));
    EXPECT_THAT(controls_rb_find_or_predict(&rb, 23), Pointee(c3));

    controls_rb_deinit(&rb);
}
