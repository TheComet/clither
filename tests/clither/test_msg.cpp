#include "gmock/gmock.h"

#include "clither/controls.h"
#include "clither/msg.h"

#include "cstructures/vector.h"

#define NAME msg

using namespace testing;

bool operator==(const struct controls& a, const struct controls& b)
{
    return a.action == b.action && a.action == b.action && a.speed == b.speed;
}

TEST(NAME, compress_single_controls)
{
    struct cs_vector v;
    vector_init(&v, sizeof(struct controls));

    struct controls c0 = {25, 8, 0};

    *(struct controls*)vector_emplace(&v) = c0;
    struct msg* m = msg_controls(&v, 20);
    ASSERT_THAT(msg_controls_unpack_into(&v, m->payload, m->payload_len), Eq(0));
    msg_free(m);

    ASSERT_THAT(vector_count(&v), Eq(1));
    EXPECT_THAT(*(struct controls*)vector_get_element(&v, 0), Eq(c0));

    vector_deinit(&v);
}

TEST(NAME, compress_multiple_controls)
{
    struct cs_vector v;
    vector_init(&v, sizeof(struct controls));

    struct controls c0 = {25, 8, 1};
    struct controls c1 = {28, 20, 0};
    struct controls c2 = {31, 30, 2};
    struct controls c3 = {34, 40, 3};

    *(struct controls*)vector_emplace(&v) = c0;
    *(struct controls*)vector_emplace(&v) = c1;
    *(struct controls*)vector_emplace(&v) = c2;
    *(struct controls*)vector_emplace(&v) = c3;

    struct msg* m = msg_controls(&v, 20);
    ASSERT_THAT(msg_controls_unpack_into(&v, m->payload, m->payload_len), Eq(0));
    msg_free(m);

    ASSERT_THAT(vector_count(&v), Eq(4));
    EXPECT_THAT(*(struct controls*)vector_get_element(&v, 0), Eq(c0));
    EXPECT_THAT(*(struct controls*)vector_get_element(&v, 1), Eq(c1));
    EXPECT_THAT(*(struct controls*)vector_get_element(&v, 2), Eq(c2));
    EXPECT_THAT(*(struct controls*)vector_get_element(&v, 3), Eq(c3));

    vector_deinit(&v);
}
