#include "gmock/gmock.h"

extern "C" {
#include "clither/client.h"
#include "clither/msg_vec.h"
#include "clither/net.h"
#include "clither/server.h"
#include "clither/server_client_hm.h"
#include "clither/server_settings.h"
#include "clither/snake_btree.h"
#include "clither/world.h"
}

#define NAME protocol_feedback

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
        cl_cmd = cmd_default();
    }

    void TearDown() override
    {
        if (cl.state != CLIENT_DISCONNECTED)
            client_disconnect(&cl);
        world_deinit(&cl_world);
        world_deinit(&sv_world);
        client_deinit(&cl);
        server_deinit(&sv);
        net_deinit();
    }

    void SimClient()
    {
        cl_cmd = cmd_make(cl_cmd, 0, 1, CMD_ACTION_NONE);
        struct snake* snake = snake_btree_find(cl_world.snakes, cl.snake_id);
        cmd_queue_put(&snake->cmdq, cl_cmd, cl.frame_number);
        snake_remove_stale_segments_with_rollback_constraint(
            &snake->data,
            &snake->head_ack,
            snake_step(
                &snake->data,
                &snake->head,
                &snake->param,
                cl_cmd,
                cl.sim_tick_rate));
        cl.frame_number++;
    }

    void SimServer(uint16_t frame_number)
    {
        int16_t       idx;
        uint16_t      uid;
        struct snake* snake;
        btree_for_each (sv_world.snakes, idx, uid, snake)
        {
            struct cmd cmd;
            (void)uid;
            if (!snake_try_reset_hold(snake, frame_number))
                continue;
            cmd = cmd_queue_take_or_predict(&snake->cmdq, frame_number);
            snake_remove_stale_segments(
                &snake->data,
                snake_step(
                    &snake->data,
                    &snake->head,
                    &snake->param,
                    cmd,
                    sv_settings.sim_tick_rate));
        }
    }

protected:
    struct server          sv;
    struct server_settings sv_settings;
    struct client          cl;
    struct world           sv_world;
    struct world           cl_world;
    struct cmd             cl_cmd;
};

TEST_F(NAME, server_holds_snake_until_catching_up_to_client_first_command_frame)
{
    uint16_t rtt = 3;
    uint16_t sv_frame = 32;
    ASSERT_THAT(client_connect(&cl, "127.0.0.1", "5555", "test"), Eq(0));
    ASSERT_THAT(client_send_pending_data(&cl), Eq(0));
    ASSERT_THAT(server_recv(&sv, &sv_settings, &sv_world, sv_frame), Eq(0));
    ASSERT_THAT(server_send_pending_data(&sv), Eq(0));
    cl.frame_number += rtt;
    ASSERT_THAT(
        client_recv(&cl, &cl_world), Eq(client_recv_tick_rate_changed()));

    // The client has calculated a frame number based on sv_frame+rtt+(some
    // buffer).
    // This means the first command sent by the client will be timestamped to
    // this frame number. This test checks if the server "holds" the snake up
    // until the server reaches this frame.
    uint16_t sv_hold_until = cl.frame_number;

    auto RunServerClient = [this, &sv_frame]()
    {
        struct snake* cl_snake = snake_btree_find(cl_world.snakes, cl.snake_id);
        client_recv(&cl, &cl_world);
        SimClient();
        SimClient();
        SimClient();
        msg_commands(&cl.pending_msgs, &cl_snake->cmdq);
        client_send_pending_data(&cl);

        server_recv(&sv, &sv_settings, &sv_world, sv_frame);
        SimServer(sv_frame++);
        SimServer(sv_frame++);
        SimServer(sv_frame);
        server_queue_snake_data(&sv, &sv_world, sv_frame);
        server_send_pending_data(&sv);
        sv_frame++;
    };

    int16_t                slot;
    const struct net_addr* addr;
    struct server_client*  sv_client;
    server_client_hm_for_each (sv.clients, slot, addr, sv_client)
    {
        (void)slot;
        (void)addr;
        struct snake* sv_snake =
            snake_btree_find(sv_world.snakes, sv_client->snake_id);
        ASSERT_THAT(snake_is_held(sv_snake), IsTrue());
    }

    while (sv_frame <= sv_hold_until)
        RunServerClient();

    server_client_hm_for_each (sv.clients, slot, addr, sv_client)
    {
        (void)slot;
        (void)addr;
        struct snake* sv_snake =
            snake_btree_find(sv_world.snakes, sv_client->snake_id);
        ASSERT_THAT(snake_is_held(sv_snake), IsFalse());
    }
}
