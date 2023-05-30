#include "gmock/gmock.h"

#include "clither/controls.h"
#include "clither/msg.h"
#include "clither/snake.h"

#include "cstructures/btree.h"

#define NAME msg

using namespace testing;

bool operator==(const struct controls& a, const struct controls& b)
{
    return a.action == b.action && a.action == b.action && a.speed == b.speed;
}

TEST(NAME, compress_single_controls)
{
    struct cs_btree b;
    btree_init(&b, sizeof(struct controls));

    struct controls c0 = {25, 8, 0};

    *(struct controls*)btree_emplace_new(&b, 20) = c0;
    struct msg* m = msg_controls(&b);

    struct snake s;
    snake_init(&s, make_qwposi(0, 0), "test");
    uint16_t first_frame, last_frame;
    ASSERT_THAT(msg_controls_unpack_into(&s, 0, m->payload, m->payload_len, &first_frame, &last_frame), Eq(0));
    msg_free(m);

    ASSERT_THAT(btree_count(&s.controls_buffer), Eq(1));
    EXPECT_THAT(*(struct controls*)btree_find(&s.controls_buffer, 20), Eq(c0));

    snake_deinit(&s);
    btree_deinit(&b);
}

TEST(NAME, compress_multiple_controls)
{
    struct cs_btree b;
    btree_init(&b, sizeof(struct controls));

    struct controls c0 = {25, 8, 1};
    struct controls c1 = {28, 20, 0};
    struct controls c2 = {31, 30, 2};
    struct controls c3 = {34, 40, 3};

    *(struct controls*)btree_emplace_new(&b, 20) = c0;
    *(struct controls*)btree_emplace_new(&b, 21) = c1;
    *(struct controls*)btree_emplace_new(&b, 22) = c2;
    *(struct controls*)btree_emplace_new(&b, 23) = c3;

    struct msg* m = msg_controls(&b);
    struct snake s;
    snake_init(&s, make_qwposi(0, 0), "test");
    uint16_t first_frame, last_frame;
    ASSERT_THAT(msg_controls_unpack_into(&s, 0, m->payload, m->payload_len, &first_frame, &last_frame), Eq(0));
    msg_free(m);

    ASSERT_THAT(btree_count(&s.controls_buffer), Eq(4));
    EXPECT_THAT(*(struct controls*)btree_find(&s.controls_buffer, 20), Eq(c0));
    EXPECT_THAT(*(struct controls*)btree_find(&s.controls_buffer, 21), Eq(c1));
    EXPECT_THAT(*(struct controls*)btree_find(&s.controls_buffer, 22), Eq(c2));
    EXPECT_THAT(*(struct controls*)btree_find(&s.controls_buffer, 23), Eq(c3));

    snake_deinit(&s);
    btree_deinit(&b);
}
