#include "gmock/gmock.h"

extern "C" {
#include "clither/client.h"
#include "clither/msg_vec.h"
#include "clither/net.h"
#include "clither/server.h"
#include "clither/server_client_hm.h"
#include "clither/server_settings.h"
#include "clither/world.h"
}

#define NAME protocol_join

using namespace testing;

static bool
operator==(const client_recv_result& r1, const client_recv_result& r2)
{
    return r1.tick_rated_changed == r2.tick_rated_changed &&
           r1.error == r2.error && r1.disconnected == r2.disconnected;
}

class NAME : public Test
{
public:
    void SetUp() override
    {
        ASSERT_THAT(net_init(), Eq(0));
        ASSERT_THAT(server_init(&sv, "", "5555"), Eq(0));
        server_settings_set_defaults(&sv_settings);
        client_init(&cl);
        world_init(&cl_world);
        world_init(&sv_world);
    }
    void TearDown() override
    {
        client_disconnect(&cl);
        world_deinit(&cl_world);
        world_deinit(&sv_world);
        client_deinit(&cl);
        server_deinit(&sv);
        net_deinit();
    }

protected:
    struct server          sv;
    struct server_settings sv_settings;
    struct client          cl;
    struct world           sv_world;
    struct world           cl_world;
};

TEST_F(NAME, client_resends_join_request)
{
    ASSERT_THAT(client_connect(&cl, "127.0.0.1", "5555", "test"), Eq(0));
    ASSERT_THAT(client_send_pending_data(&cl), Eq(0));
    ASSERT_THAT(server_recv(&sv, &sv_settings, &sv_world, 1), Eq(0));
    ASSERT_THAT(client_send_pending_data(&cl), Eq(0));
    ASSERT_THAT(server_recv(&sv, &sv_settings, &sv_world, 1), Eq(0));

    ASSERT_THAT(cl.state, Eq(CLIENT_JOINING));
    ASSERT_THAT(vec_count(cl.pending_msgs), Eq(1));
    ASSERT_THAT((*vec_get(cl.pending_msgs, 0))->type, Eq(MSG_JOIN_REQUEST));

    int             slot;
    const net_addr* addr;
    server_client*  svc;
    server_client_hm_for_each (sv.clients, slot, addr, svc)
    {
        (void)addr;
        ASSERT_THAT(vec_count(svc->pending_msgs), Eq(2));
        ASSERT_THAT(
            (*vec_get(svc->pending_msgs, 0))->type, Eq(MSG_JOIN_ACCEPT));
        ASSERT_THAT(
            (*vec_get(svc->pending_msgs, 1))->type, Eq(MSG_JOIN_ACCEPT));
    }
}

TEST_F(NAME, server_resends_join_accept)
{
    ASSERT_THAT(client_connect(&cl, "127.0.0.1", "5555", "test"), Eq(0));
    ASSERT_THAT(client_send_pending_data(&cl), Eq(0));
    ASSERT_THAT(server_recv(&sv, &sv_settings, &sv_world, 1), Eq(0));
    ASSERT_THAT(client_send_pending_data(&cl), Eq(0));
    ASSERT_THAT(server_recv(&sv, &sv_settings, &sv_world, 1), Eq(0));

    ASSERT_THAT(cl.state, Eq(CLIENT_JOINING));
    ASSERT_THAT(vec_count(cl.pending_msgs), Eq(1));
    ASSERT_THAT((*vec_get(cl.pending_msgs, 0))->type, Eq(MSG_JOIN_REQUEST));

    int             slot;
    const net_addr* addr;
    server_client*  svc;
    server_client_hm_for_each (sv.clients, slot, addr, svc)
    {
        (void)addr;
        ASSERT_THAT(vec_count(svc->pending_msgs), Eq(2));
        ASSERT_THAT(
            (*vec_get(svc->pending_msgs, 0))->type, Eq(MSG_JOIN_ACCEPT));
        ASSERT_THAT(
            (*vec_get(svc->pending_msgs, 1))->type, Eq(MSG_JOIN_ACCEPT));
    }
}

TEST_F(NAME, client_accepts_join)
{
    ASSERT_THAT(client_connect(&cl, "127.0.0.1", "5555", "test"), Eq(0));
    ASSERT_THAT(client_send_pending_data(&cl), Eq(0));

    ASSERT_THAT(server_recv(&sv, &sv_settings, &sv_world, 1), Eq(0));
    ASSERT_THAT(server_send_pending_data(&sv), Eq(0));

    ASSERT_THAT(
        client_recv(&cl, &cl_world), Eq(client_recv_tick_rate_changed()));
    ASSERT_THAT(cl.state, Eq(CLIENT_CONNECTED));
    ASSERT_THAT(vec_count(cl.pending_msgs), Eq(0));
}
