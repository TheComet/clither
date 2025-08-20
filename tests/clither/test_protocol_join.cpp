#include "clither/tests/LogHelper.hpp"

#include "gmock/gmock.h"

extern "C" {
#include "clither/client/client.h"
#include "clither/game/msg_vec.h"
#include "clither/game/settings.h"
#include "clither/game/snake_bmap.h"
#include "clither/game/world.h"
#include "clither/platform/net.h"
#include "clither/server/server.h"
#include "clither/server/server_client_hmap.h"
}

#define NAME test_protocol_join

using namespace testing;

static bool
operator==(const client_recv_result& r1, const client_recv_result& r2)
{
    return r1.tick_rated_changed == r2.tick_rated_changed &&
           r1.error == r2.error && r1.disconnected == r2.disconnected;
}

struct NAME : Test, LogHelper
{
public:
    void SetUp() override
    {
        ASSERT_THAT(net_init(), Eq(0));
        ASSERT_THAT(server_init(&sv, "", "5555"), Eq(0));
        settings_init(&settings);
        client_init(&cl);
        world_init(&cl_world);
        world_init(&sv_world);
    }
    void TearDown() override
    {
        if (cl.state != CLIENT_DISCONNECTED)
            client_disconnect(&cl);
        world_deinit(&cl_world);
        world_deinit(&sv_world);
        client_deinit(&cl);
        settings_deinit(&settings);
        server_deinit(&sv);
        net_deinit();
    }

protected:
    struct settings settings;
    struct server   sv;
    struct client   cl;
    struct world    sv_world;
    struct world    cl_world;
};

TEST_F(NAME, client_resends_join_request)
{
    ASSERT_THAT(
        client_connect(&cl, &settings, "127.0.0.1", "5555", "test"), Eq(0));
    ASSERT_THAT(client_send_pending_data(&cl), Eq(0));
    ASSERT_THAT(client_send_pending_data(&cl), Eq(0));

    ASSERT_THAT(cl.state, Eq(CLIENT_JOINING));
    ASSERT_THAT(vec_count(cl.pending_msgs), Eq(1));
    ASSERT_THAT((*vec_get(cl.pending_msgs, 0))->type, Eq(MSG_JOIN_REQUEST));
}

TEST_F(NAME, server_resends_join_accept)
{
    ASSERT_THAT(
        client_connect(&cl, &settings, "127.0.0.1", "5555", "test"), Eq(0));
    ASSERT_THAT(client_send_pending_data(&cl), Eq(0));
    ASSERT_THAT(client_send_pending_data(&cl), Eq(0));

    int             slot;
    const net_addr* addr;
    server_client*  svc;
    ASSERT_THAT(
        server_recv(&sv, &settings.server, &sv_world, &settings.world, 1),
        Eq(0));
    server_client_hmap_for_each (sv.clients, slot, addr, svc)
    {
        (void)addr;
        ASSERT_THAT(vec_count(svc->pending_msgs), Eq(2));
        ASSERT_THAT(
            (*vec_get(svc->pending_msgs, 0))->type, Eq(MSG_JOIN_ACCEPT));
        ASSERT_THAT(
            (*vec_get(svc->pending_msgs, 1))->type, Eq(MSG_JOIN_ACCEPT));
    }
}

TEST_F(NAME, server_denies_join_full_server)
{
    ASSERT_THAT(
        client_connect(&cl, &settings, "127.0.0.1", "5555", "test"), Eq(0));
    ASSERT_THAT(client_send_pending_data(&cl), Eq(0));

    settings.server.max_players = 0;
    ASSERT_THAT(
        server_recv(&sv, &settings.server, &sv_world, &settings.world, 1),
        Eq(0));
    ASSERT_THAT(server_send_pending_data(&sv, &sv_world), Eq(0));

    ASSERT_THAT(
        client_recv(&cl, &settings, &cl_world, NULL, NULL),
        Eq(client_recv_disconnected()));
    ASSERT_THAT(cl.state, Eq(CLIENT_DISCONNECTED));
    ASSERT_THAT(vec_count(cl.pending_msgs), Eq(0));
}

TEST_F(NAME, server_denies_join_username_too_long)
{
    ASSERT_THAT(
        client_connect(&cl, &settings, "127.0.0.1", "5555", "test"), Eq(0));
    ASSERT_THAT(client_send_pending_data(&cl), Eq(0));

    settings.server.max_username_len = 1;
    ASSERT_THAT(
        server_recv(&sv, &settings.server, &sv_world, &settings.world, 1),
        Eq(0));
    ASSERT_THAT(server_send_pending_data(&sv, &sv_world), Eq(0));

    ASSERT_THAT(
        client_recv(&cl, &settings, &cl_world, NULL, NULL),
        Eq(client_recv_disconnected()));
    ASSERT_THAT(cl.state, Eq(CLIENT_DISCONNECTED));
    ASSERT_THAT(vec_count(cl.pending_msgs), Eq(0));
}

TEST_F(NAME, server_accepts_join)
{
    ASSERT_THAT(
        client_connect(&cl, &settings, "127.0.0.1", "5555", "test"), Eq(0));
    ASSERT_THAT(client_send_pending_data(&cl), Eq(0));

    ASSERT_THAT(
        server_recv(&sv, &settings.server, &sv_world, &settings.world, 1),
        Eq(0));
    ASSERT_THAT(server_send_pending_data(&sv, &sv_world), Eq(0));

    ASSERT_THAT(
        client_recv(&cl, &settings, &cl_world, NULL, NULL),
        Eq(client_recv_tick_rate_changed()));
    ASSERT_THAT(cl.state, Eq(CLIENT_CONNECTED));
    ASSERT_THAT(vec_count(cl.pending_msgs), Eq(0));
    // Ensure snakes were created
    ASSERT_THAT(snake_bmap_find(cl_world.snakes, cl.snake_id), NotNull());
    ASSERT_THAT(snake_bmap_find(sv_world.snakes, cl.snake_id), NotNull());
}

TEST_F(NAME, client_calculates_frame_number_with_buffer)
{
    uint16_t sv_frame_number = 32;
    uint16_t rtt = 8;
    cl.frame_number = 8;

    ASSERT_THAT(
        client_connect(&cl, &settings, "127.0.0.1", "5555", "test"), Eq(0));
    ASSERT_THAT(client_send_pending_data(&cl), Eq(0));

    ASSERT_THAT(
        server_recv(
            &sv, &settings.server, &sv_world, &settings.world, sv_frame_number),
        Eq(0));
    ASSERT_THAT(server_send_pending_data(&sv, &sv_world), Eq(0));

    cl.frame_number += rtt; // simulate rtt frames passing since joining
    ASSERT_THAT(
        client_recv(&cl, &settings, &cl_world, NULL, NULL),
        Eq(client_recv_tick_rate_changed()));
    ASSERT_THAT(cl.state, Eq(CLIENT_CONNECTED));

    uint16_t expected_cl_frame_number = sv_frame_number + rtt;
    // Client adds some buffer initially
    expected_cl_frame_number +=
        5 * settings.server.sim_tick_rate / settings.server.net_tick_rate;
    ASSERT_THAT(cl.frame_number, Eq(expected_cl_frame_number));
}

TEST_F(NAME, client_updates_tick_rates_from_server)
{
    settings.server.sim_tick_rate = 120;
    settings.server.net_tick_rate = 80;
    ASSERT_THAT(
        client_connect(&cl, &settings, "127.0.0.1", "5555", "test"), Eq(0));
    ASSERT_THAT(client_send_pending_data(&cl), Eq(0));
    ASSERT_THAT(
        server_recv(&sv, &settings.server, &sv_world, &settings.world, 32),
        Eq(0));
    ASSERT_THAT(server_send_pending_data(&sv, &sv_world), Eq(0));
    ASSERT_THAT(
        client_recv(&cl, &settings, &cl_world, NULL, NULL),
        Eq(client_recv_tick_rate_changed()));
    ASSERT_THAT(cl.state, Eq(CLIENT_CONNECTED));

    ASSERT_THAT(cl.sim_tick_rate, Eq(settings.server.sim_tick_rate));
    ASSERT_THAT(cl.net_tick_rate, Eq(settings.server.net_tick_rate));
}

TEST_F(NAME, client_rejects_server_if_given_incorrect_rtt)
{
    uint16_t sv_frame_number = 32;
    uint16_t rtt = 8;
    cl.frame_number = 8;

    ASSERT_THAT(
        client_connect(&cl, &settings, "127.0.0.1", "5555", "test"), Eq(0));
    ASSERT_THAT(client_send_pending_data(&cl), Eq(0));

    ASSERT_THAT(
        server_recv(
            &sv, &settings.server, &sv_world, &settings.world, sv_frame_number),
        Eq(0));
    ASSERT_THAT(server_send_pending_data(&sv, &sv_world), Eq(0));

    cl.frame_number += rtt; // simulate rtt frames passing since joining
    ASSERT_THAT(
        client_recv(&cl, &settings, &cl_world, NULL, NULL),
        Eq(client_recv_tick_rate_changed()));
    ASSERT_THAT(cl.state, Eq(CLIENT_CONNECTED));

    uint16_t expected_cl_frame_number = sv_frame_number + rtt;
    // Client adds some buffer initially
    expected_cl_frame_number +=
        5 * settings.server.sim_tick_rate / settings.server.net_tick_rate;
    ASSERT_THAT(cl.frame_number, Eq(expected_cl_frame_number));
}
