#include "gmock/gmock.h"

#include "clither/controls.h"
#include "clither/msg.h"

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
    ASSERT_THAT(msg_controls_unpack_into(&b, m->payload, m->payload_len), Eq(0));
    msg_free(m);

    ASSERT_THAT(btree_count(&b), Eq(1));
    EXPECT_THAT(*(struct controls*)btree_find(&b, 20), Eq(c0));

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
    ASSERT_THAT(msg_controls_unpack_into(&b, m->payload, m->payload_len), Eq(0));
    msg_free(m);

    ASSERT_THAT(btree_count(&b), Eq(4));
    EXPECT_THAT(*(struct controls*)btree_find(&b, 20), Eq(c0));
    EXPECT_THAT(*(struct controls*)btree_find(&b, 21), Eq(c1));
    EXPECT_THAT(*(struct controls*)btree_find(&b, 22), Eq(c2));
    EXPECT_THAT(*(struct controls*)btree_find(&b, 23), Eq(c3));

    btree_deinit(&b);
}
