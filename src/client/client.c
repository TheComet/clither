#include "clither/bot/bot.h"
#include "clither/client/client.h"
#include "clither/game/camera.h"
#include "clither/game/input.h"
#include "clither/game/msg.h"
#include "clither/game/msg_vec.h"
#include "clither/game/resource_pack.h"
#include "clither/game/settings.h"
#include "clither/game/snake.h"
#include "clither/game/snake_bmap.h"
#include "clither/game/world.h"
#include "clither/game/wrap.h"
#include "clither/gfx/gfx.h"
#include "clither/platform/fs.h"
#include "clither/platform/net.h"
#include "clither/platform/signals.h"
#include "clither/platform/tick.h"
#include "clither/ui/ui.h"
#include "clither/util/bmap.h"
#include "clither/util/cli_colors.h"
#include "clither/util/log.h"
#include "clither/util/morton.h"
#include "clither/util/str.h"
#include <string.h> /* memcpy */

/* ------------------------------------------------------------------------- */
void client_init(struct client* client)
{
    client->inet = NULL;
    client->connection = NULL;
    client->username = NULL;
    client->sim_tick_rate = 60;
    client->net_tick_rate = 20;
    client->timeout_counter = 0;
    client->frame_number = 0;
    client->snake_id = 0;
    client->warp = 0;
    client->state = CLIENT_DISCONNECTED;

    msg_vec_init(&client->pending_msgs);
}

/* ------------------------------------------------------------------------- */
void client_deinit(struct client* client)
{
    if (client->state != CLIENT_DISCONNECTED)
        client_disconnect(client);

    client->connection = NULL;
    str_deinit(client->username);
    msg_vec_deinit(client->pending_msgs);
}

/* ------------------------------------------------------------------------- */
int client_connect(
    struct client* client,
    const char*    server_address,
    const char*    port,
    const char*    username)
{
    CLITHER_DEBUG_ASSERT(client->state == CLIENT_DISCONNECTED);
    CLITHER_DEBUG_ASSERT(client->connection == NULL);
    CLITHER_DEBUG_ASSERT(str_len(client->username) == 0);

    if (server_address == NULL || !*server_address)
    {
        log_err("No server address was specified! Can't init client socket\n");
        log_err(
            "You can use --addr <address> to specify an address to connect "
            "to\n");
        return -1;
    }

    if (port == NULL || !*port)
    {
        log_err("No server port was specified! Can't init client socket\n");
        log_err("You can use --port <port> to specify a port to connect to\n");
        return -1;
    }

#if defined(__EMSCRIPTEN__)
    client->inet = &net_ws_client;
#else
    client->inet = &net_udp_client;
#endif

    if (str_set_cstr(&client->username, username) != 0)
        return -1;
    client->connection = client->inet->create(server_address, port);
    if (client->connection == NULL)
        return -1;

    client_queue(
        client, msg_join_request(0x0000, client->frame_number, username));

    client->state = CLIENT_JOINING;

    return 0;
}

/* ------------------------------------------------------------------------- */
void client_disconnect(struct client* client)
{
    struct msg** msg;

    CLITHER_DEBUG_ASSERT(client->state != CLIENT_DISCONNECTED);
    CLITHER_DEBUG_ASSERT(client->connection != NULL);
    CLITHER_DEBUG_ASSERT(client->inet != NULL);
    CLITHER_DEBUG_ASSERT(str_len(client->username) != 0);

    client->inet->destroy(client->connection);
    client->connection = NULL;
    client->inet = NULL;

    str_deinit(client->username);
    client->username = NULL;

    vec_for_each (client->pending_msgs, msg)
        msg_free(*msg);
    msg_vec_clear(client->pending_msgs);

    client->state = CLIENT_DISCONNECTED;
}

/* ------------------------------------------------------------------------- */
int client_queue(struct client* client, struct msg* m)
{
    return msg_vec_push(&client->pending_msgs, m);
}

/* ------------------------------------------------------------------------- */
static int append_unreliable_msgs_to_buf(struct msg** pmsg, void* user)
{
    uint8_t            type;
    struct net_packet* pkt = user;
    struct msg*        msg = *pmsg;

    if (pkt->len + msg->payload_len + 2 > (int)sizeof(pkt->data))
        return -1; /* Triggers a send() */
    if (msg_is_reliable(msg))
        return VEC_RETAIN;

    type = (uint8_t)msg->type;
    memcpy(pkt->data + pkt->len + 0, &type, 1);
    memcpy(pkt->data + pkt->len + 1, &msg->payload_len, 1);
    memcpy(pkt->data + pkt->len + 2, msg->payload, msg->payload_len);
    pkt->len += msg->payload_len + 2;

    msg_free(msg);
    return VEC_ERASE;
}

/* ------------------------------------------------------------------------- */
int client_send_pending_data(struct client* client)
{
    struct net_packet pkt;
    struct msg**      pmsg;
    uint8_t           type;
    int               status;

    CLITHER_DEBUG_ASSERT(client->state != CLIENT_DISCONNECTED);
    CLITHER_DEBUG_ASSERT(client->connection != NULL);
    CLITHER_DEBUG_ASSERT(client->inet != NULL);

    /* Append unreliable messages first before appending reliable */
packet_full:
    pkt.len = 0;
    status = msg_vec_retain(
        client->pending_msgs, append_unreliable_msgs_to_buf, &pkt);
    if (status == -1)
    {
        client->inet->send(client->connection, &pkt);
        goto packet_full;
    }

    /* Reliable messages */
    vec_for_each (client->pending_msgs, pmsg)
    {
        struct msg* msg = *pmsg;
        if (msg_is_unreliable(msg))
            continue;

        if (pkt.len + msg->payload_len + 2 > (int)sizeof(pkt.data))
        {
            client->inet->send(client->connection, &pkt);
            pkt.len = 0;
        }

        if (--msg->resend_period_counter > 0)
            continue;

        msg->resend_period_counter = msg->resend_period;
        if (--msg->resend_retry_counter == 0)
            return log_err(
                "Server did not acknowledge reliable message: type=%d\n",
                msg->type);

        type = (uint8_t)msg->type;
        memcpy(pkt.data + pkt.len + 0, &type, 1);
        memcpy(pkt.data + pkt.len + 1, &msg->payload_len, 1);
        memcpy(pkt.data + pkt.len + 2, msg->payload, msg->payload_len);
        pkt.len += msg->payload_len + 2;
    }
    if (pkt.len > 0)
        client->inet->send(client->connection, &pkt);

    /* 3 second timeout */
    client->timeout_counter++;
    if (client->timeout_counter > client->net_tick_rate * 3)
    {
        log_err("Server timed out\n");
        return -1;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
static struct client_recv_result process_message(
    struct client* client,
    struct world*  world,
    enum msg_type  msg_type,
    const uint8_t* msg_data,
    uint8_t        msg_len)
{
    union parsed_payload pp;

    log_net("Parsing msg type=%d, len=%d\n", msg_type, msg_len);
    switch (msg_parse_payload(&pp, msg_type, msg_data, msg_len))
    {
        case MSG_JOIN_REQUEST: break;

        case MSG_JOIN_ACCEPT: {
            struct settings_world settings_world;
            struct snake*         snake;
            uint16_t              rtt;

            if (client->state != CLIENT_JOINING)
                return client_recv_ok();

            /* Stop sending join request messages */
            msg_vec_remove_type(client->pending_msgs, MSG_JOIN_REQUEST);

            /*
             * The server will be on a different frame number than we are,
             * since we joined at some random time. In our join request
             * message we sent our current frame number, and the server has
             * sent back this number plus the server's frame number. From
             * this it's possible to figure out our offset and synchronize.
             */

            /*
             * Round trip time is our current frame number minus the frame
             * on which the join request was sent.
             */
            rtt = client->frame_number - pp.join_accept.client_frame;
            if (rtt > client->net_tick_rate * 5) /* 5 seconds */
            {
                log_err(
                    "Server sent back a client frame number that is "
                    "unlikely or impossible (ours: %d, theirs: %d).\n",
                    client->frame_number,
                    pp.join_accept.client_frame);
                log_err(
                    "This may be a bug, or the server is possibly "
                    "malicious.\n");

                client_disconnect(client);
                return client_recv_disconnected();
            }

            /*
             * We will be simulating half rtt in the future, relative to the
             * server, however, the server is now also half rtt further into
             * the future relative to the frame it sent back. Therefore, we
             * are a full rtt into the future now.
             */
            client->frame_number = pp.join_accept.server_frame + rtt;
            /* Add some buffer so the client doesn't mispredict in the
             * beginning. MSG_FEEDBACK takes care of reducing this if the
             * connection is good */
            client->frame_number +=
                5 * client->sim_tick_rate / client->net_tick_rate;

            client->snake_id = pp.join_accept.snake_id;
            snake = world_create_snake(
                world,
                client->snake_id,
                pp.join_accept.spawn,
                str_cstr(client->username));
            if (snake == NULL)
                return client_recv_error();
            snake_head_init(&snake->remote.ack.head, pp.join_accept.spawn);

            log_net(
                "MSG_JOIN_ACCEPT:\n"
                "  server frame=%d, client frame=%d, rtt=%d\n"
                "  spawn=%d, %d\n",
                pp.join_accept.server_frame,
                client->frame_number,
                rtt,
                pp.join_accept.spawn.x,
                pp.join_accept.spawn.y);

            /* Apply world settings from server */
            settings_world_set_defaults(&settings_world);
            settings_world.inner_radius = pp.join_accept.world_inner_radius;
            settings_world.ring_start = pp.join_accept.world_ring_start;
            settings_world.ring_end = pp.join_accept.world_ring_end;
            world_update_settings(world, &settings_world);

            /* Server may also be running on a different tick rate */
            client->sim_tick_rate = pp.join_accept.sim_tick_rate;
            client->net_tick_rate = pp.join_accept.net_tick_rate;

            client->state = CLIENT_CONNECTED;

            return client_recv_tick_rate_changed();
        }

        case MSG_JOIN_DENY_BAD_PROTOCOL:
        case MSG_JOIN_DENY_BAD_USERNAME:
        case MSG_JOIN_DENY_SERVER_FULL: {
            log_err("Failed to join server: %s\n", pp.join_deny.error);
            client_disconnect(client);

            /* We don't want to process any more messages */
            return client_recv_disconnected();
        }

        case MSG_LEAVE:
        case MSG_COMMANDS: break;

        case MSG_FEEDBACK: {
            client->warp = pp.feedback.diff;
            return client_recv_ok();
        }

        case MSG_SNAKE_USERNAME: {
            struct snake* snake =
                snake_bmap_find(world->snakes, pp.snake_username.snake_id);
            if (snake == NULL)
                return client_recv_ok();

            if (str_set_cstr(&snake->data.name, pp.snake_username.username) !=
                0)
            {
                return client_recv_error();
            }

            client_queue(
                client, msg_snake_username_ack(pp.snake_username.snake_id));

            return client_recv_ok();
        }
        case MSG_SNAKE_USERNAME_ACK: break;

        case MSG_SNAKE_DESTROY: {
            if (pp.snake_destroy.snake_id == client->snake_id)
            {
                log_warn("Received MSG_SNAKE_DESTROY for self\n");
                return client_recv_ok();
            }

            world_remove_snake(world, pp.snake_destroy.snake_id);
            client_queue(
                client, msg_snake_destroy_ack(pp.snake_destroy.snake_id));
            return client_recv_ok();
        }
        case MSG_SNAKE_DESTROY_ACK: break;

        case MSG_SNAKE_DEATH: {
            struct snake* snake =
                snake_bmap_find(world->snakes, client->snake_id);
            CLITHER_DEBUG_ASSERT(snake != NULL);

            snake_set_dead(snake);
            client_queue(client, msg_snake_death_ack());
            return client_recv_ok();
        }
        case MSG_SNAKE_DEATH_ACK: break;

        case MSG_SNAKE_HEAD: {
            struct snake_head head_auth;
            struct snake*     snake =
                snake_bmap_find(world->snakes, client->snake_id);
            if (snake == NULL)
                return client_recv_ok();

            head_auth.pos = pp.snake_head.pos;
            head_auth.angle = pp.snake_head.angle;
            head_auth.speed = pp.snake_head.speed;

            snake_param_update(
                &snake->param, snake->param.upgrades, pp.snake_head.food_eaten);

            snake_ack_frame(
                &snake->data,
                &snake->remote.ack,
                &snake->head,
                &head_auth,
                &snake->param,
                &snake->cmdq,
                pp.snake_head.frame_number,
                client->sim_tick_rate);

            return client_recv_ok();
        }

        case MSG_SNAKE_PARAM: {
            struct snake* snake =
                snake_bmap_find(world->snakes, pp.snake_param.snake_id);
            if (snake == NULL)
                return client_recv_ok();

            snake_param_update(
                &snake->param,
                snake->param.upgrades,
                pp.snake_param.food_eaten);

            return client_recv_ok();
        }

        case MSG_BEZIER: {
            struct snake* snake;
            int           i;

            snake = snake_bmap_find(world->snakes, pp.bezier.snake_id);
            /* MSG_KNOT is responsible for creating the snake */
            if (snake == NULL)
                return client_recv_ok();

            if (u16_le_wrap(
                    pp.bezier.frame_number,
                    snake->remote.replica.head_frame_numbers[0]))
            {
                log_dbg(
                    "Received outdated MSG_BEZIER: received frame=%d, replica "
                    "frame=%d\n",
                    pp.bezier.frame_number,
                    snake->remote.replica.head_frame_numbers[0]);
                return client_recv_ok();
            }

            snake_unextrapolate(
                &snake->data, &snake->head, &snake->remote.replica);

            for (i = CLITHER_ARRAY_SIZE(snake->remote.replica.head_history) - 1;
                 i != 0;
                 --i)
            {
                snake->remote.replica.head_history[i] =
                    snake->remote.replica.head_history[i - 1];
                snake->remote.replica.head_frame_numbers[i] =
                    snake->remote.replica.head_frame_numbers[i - 1];
            }

            snake->remote.replica.head_history[0].pos = pp.bezier.pos;
            snake->remote.replica.head_history[0].angle = pp.bezier.angle;
            snake->remote.replica.head_history[0].speed = pp.bezier.speed;
            snake->remote.replica.head_frame_numbers[0] =
                pp.bezier.frame_number;

            snake_update_bezier_extents(
                &snake->data,
                pp.bezier.rb_read,
                pp.bezier.rb_write,
                pp.bezier.head_len_backwards,
                pp.bezier.second_len_forwards);

            return client_recv_ok();
        }

        case MSG_KNOT: {
            struct snake* snake;
            if (pp.knot.snake_id == client->snake_id)
            {
                log_warn("Received MSG_KNOT for own snake, ignoring\n");
                return client_recv_ok();
            }

            snake = snake_bmap_find(world->snakes, pp.knot.snake_id);
            if (snake == NULL)
            {
                int i;
                snake = world_create_snake(
                    world, pp.knot.snake_id, make_qwposi(0, 0), "");
                if (snake == NULL)
                    return client_recv_error();
                for (i = 0; i != CLITHER_ARRAY_SIZE(
                                     snake->remote.replica.head_history);
                     ++i)
                {
                    snake_head_init(
                        &snake->remote.replica.head_history[i],
                        make_qwposi(0, 0));
                    snake->remote.replica.head_frame_numbers[i] = 0;
                }
            }

            if (snake_create_or_update_knot(
                    &snake->data,
                    pp.knot.knot_idx,
                    pp.knot.pos,
                    pp.knot.angle,
                    pp.knot.len_backwards,
                    pp.knot.len_forwards) != 0)
            {
                return client_recv_error();
            }

            client_queue(
                client, msg_knot_ack(pp.knot.snake_id, pp.knot.knot_idx));

            return client_recv_ok();
        }

        case MSG_KNOT_ACK: break;

        case MSG_FOOD_CREATE: {
            food_bmap_create_food(
                &world->food_bmap, pp.food_create.pos, pp.food_create.dir);
            client_queue(client, msg_food_create_ack(pp.food_create.pos));
            return client_recv_ok();
        }
        case MSG_FOOD_CREATE_ACK: break;

        case MSG_FOOD_DESTROY: {
            morton morton = morton_encode_qwpos(pp.food_destroy.pos);
            food_bmap_erase(world->food_bmap, morton);
            client_queue(client, msg_food_destroy_ack(pp.food_destroy.pos));
            return client_recv_ok();
        }
        case MSG_FOOD_DESTROY_ACK: break;
    }

    log_warn(
        "Received unknown message type \"%d\" from server. "
        "Malicious?\n",
        msg_type);
    return client_recv_ok();
}

/* ------------------------------------------------------------------------- */
static struct client_recv_result unpack_packet(
    struct client* client, struct world* world, const struct net_packet* packet)
{
    int                       i;
    struct client_recv_result result = client_recv_ok();

    /*
     * Packet can contain multiple message objects.
     * buf[0] == message type
     * buf[1] == message payload length
     * buf[2] == beginning of message payload
     */
    for (i = 0; i < packet->len - 1;)
    {
        enum msg_type  msg_type = packet->data[i + 0];
        uint8_t        msg_len = packet->data[i + 1];
        const uint8_t* msg_data = &packet->data[i + 2];
        if (i + 2 + msg_len > packet->len)
        {
            log_warn(
                "Invalid payload length \"%d\" received from server\n",
                (int)msg_len);
            log_warn("Dropping rest of packet\n");
            break;
        }

        result = client_recv_result_combine(
            result,
            process_message(client, world, msg_type, msg_data, msg_len));
        /* Want to stop processing messages if an error occurred, or if the
         * client disconnected. */
        if (result.error || result.disconnected)
            return result;

        i += msg_len + 2;
    }

    return result;
}

/* ------------------------------------------------------------------------- */
struct client_recv_result
client_recv(struct client* client, struct world* world)
{
    struct net_packet         packet;
    struct client_recv_result result = client_recv_ok();

    CLITHER_DEBUG_ASSERT(client->connection != NULL);

    /* We may need to read more than one UDP packet */
    while (1)
    {
        switch (client->inet->receive(client->connection, &packet))
        {
            case NET_RECEIVE_ERROR: return client_recv_error();
            case NET_RECEIVE_NO_DATA: return result;
            case NET_RECEIVE_DATA: break;
        }

        /* Don't let client time out */
        client->timeout_counter = 0;

        result = client_recv_result_combine(
            result, unpack_packet(client, world, &packet));

        /* Want to stop processing messages if an error occurred, or if the
         * client disconnected. */
        if (result.error || result.disconnected)
            break;
    }

    return result;
}

/* ------------------------------------------------------------------------- */
#if defined(CLITHER_CLIENT)
struct sim_other_snakes_ctx
{
    const struct client* client;
    struct world*        world;
};
static int sim_other_snakes(uint16_t snake_id, struct snake* snake, void* user)
{
    uint32_t                     frames_extrapolated;
    struct sim_other_snakes_ctx* ctx = user;

    if (snake_id == ctx->client->snake_id)
        return BMAP_RETAIN;

    snake_unextrapolate(&snake->data, &snake->head, &snake->remote.replica);
    frames_extrapolated = snake_extrapolate(
        &snake->data,
        &snake->head,
        &snake->remote.replica,
        &snake->param,
        ctx->client->frame_number);

    /*
     * This is a failsafe for when MSG_KNOT happens to be received after
     * MSG_SNAKE_DESTROY. If the snake is extrapolated for more than 1 second,
     * we assume it is dead.
     */
    if (frames_extrapolated > 1 * ctx->client->sim_tick_rate)
    {
        snake_deinit(snake);
        return BMAP_ERASE;
    }

    return BMAP_RETAIN;
}
#endif

/* ------------------------------------------------------------------------- */
#if defined(CLITHER_CLIENT)
void* client_run(
#    if defined(CLITHER_GFX)
    const struct gfx_interface** igfx,
    struct gfx**                 gfx,
#    endif
#    if defined(CLITHER_BOT_API)
    const struct bot_interface* ibot,
    struct bot*                 bot,
#    endif
    const struct settings_client* settings,
    struct resource_pack**        pack)
{
    struct fs_watch* pack_watch;
    struct world     world;
    struct ui*       ui;
    struct input     input;
    struct cmd       cmd;
    struct camera    camera;
    struct client    client;
    struct tick      sim_tick;
    struct tick      net_tick;
    int              tick_lag;
    int              retval = -1;

    /* Change log prefix and color for server log messages */
    log_set_prefix(settings->log_prefix);
    log_set_colors(COL_B_GREEN, COL_RESET);

    pack_watch = NULL;
    if (*pack != NULL)
    {
        pack_watch = resource_pack_watch(*pack);
        if (pack_watch == NULL)
            goto watch_resource_pack_failed;
    }

    ui = ui_create();
    if (ui == NULL)
        goto create_ui_failed;

    client_init(&client);
    /*
     * TODO: In the future the GUI will take care of connecting. Here we do
     * it immediately because there is no menu.
     */
    if (client_connect(
            &client,
            settings->connect_addr,
            settings->connect_port,
            settings->username) < 0)
    {
        goto client_connect_failed;
    }

    input_init(&input);
    camera_init(&camera);
    world_init(&world);
    cmd = cmd_default();

    log_info("Client started\n");

    tick_cfg(&sim_tick, client.sim_tick_rate);
    tick_cfg(&net_tick, client.net_tick_rate);
    while (signals_exit_requested() == 0)
    {
        int net_update;

#    if defined(CLITHER_GFX)
        if (*gfx != NULL)
            (*igfx)->poll_input(*gfx, &input);
        ui_update(ui, &input, client.sim_tick_rate);
#    endif
        if (input.quit)
        {
            retval = 0;
            break;
        }

#    if defined(CLITHER_GFX)
        /* Switch graphics backends */
        if (*gfx != NULL && (input.next_gfx_backend || input.prev_gfx_backend))
        {
            int count;
            int idx, new_idx;

            for (count = 0; gfx_backends[count]; ++count)
                ;
            for (idx = 0; gfx_backends[idx]; ++idx)
                if (*igfx == gfx_backends[idx])
                    break;

            if (input.next_gfx_backend)
                new_idx = idx + 1 >= count ? 0 : idx + 1;
            else
                new_idx = idx - 1 < 0 ? count - 1 : idx - 1;

            /*
             * On Windows it is possible to create a new backend and then
             * destroy the previous backend, however, on linux this doesn't
             * seem to work. GL contexts aren't properly transferred to the
             * new instance. This is why we destroy first - then create
             */
            (*igfx)->unload_resource_pack(*gfx, *pack);
            (*igfx)->destroy(*gfx);
            (*igfx)->deinit();

            *igfx = gfx_backends[new_idx];
            if ((*igfx)->init() < 0)
                goto init_new_gfx_failed;
            *gfx = (*igfx)->create(640, 480);
            if (*gfx == NULL)
                goto create_new_gfx_failed;
            if ((*igfx)->load_resource_pack(*gfx, *pack) < 0)
                goto load_new_resource_pack_failed;

            /* Clears the button press for switching graphics backends */
            input_init(&input);
            (*igfx)->poll_input(*gfx, &input);

            goto create_new_gfx_success;

        load_new_resource_pack_failed:
            (*igfx)->destroy(*gfx);
        create_new_gfx_failed:
            (*igfx)->deinit();
        init_new_gfx_failed:
            /* Try to restore to previous backend. Shouldn't fail but who knows
             */
            (*igfx) = gfx_backends[idx];
            if ((*igfx)->init() < 0)
                break;
            *gfx = (*igfx)->create(640, 480);
            if (*gfx == NULL)
                break;
            if ((*igfx)->load_resource_pack(*gfx, *pack) < 0)
                break;
        }
    create_new_gfx_success:;

        /* Check for resource pack changes */
        if (pack_watch != NULL && fs_watch_check(pack_watch) > 0)
        {
            struct resource_pack* new_pack;

            log_info("Resource pack changed, reloading\n");
            fs_watch_deinit(pack_watch);

            new_pack = resource_pack_parse((*pack)->path);
            if (new_pack && *gfx != NULL)
            {
                (*igfx)->unload_resource_pack(*gfx, *pack);
                resource_pack_destroy(*pack);
                (*igfx)->load_resource_pack(*gfx, new_pack);
                *pack = new_pack;
            }
            pack_watch = resource_pack_watch(*pack);
        }
#    endif

        /* Receive net data */
        net_update = tick_advance(&net_tick);
        if (net_update && client.state != CLIENT_DISCONNECTED)
        {
            struct client_recv_result result = client_recv(&client, &world);
            if (result.error)
                break;

            if (result.tick_rated_changed)
            {
                /*
                 * We may have to match our tick rates to the server, because
                 * the server can freely configure these values. If the client
                 * disconnected then sim_tick_rate and net_tick_rate are reset
                 * to their default values, so in this case we also want to
                 * update the tick rate.
                 */
                tick_cfg(&sim_tick, client.sim_tick_rate);
                tick_cfg(&net_tick, client.net_tick_rate);
                log_dbg(
                    "Sim tick rate: %d, net tick rate: %d\n",
                    client.sim_tick_rate,
                    client.net_tick_rate);
            }
        }

        /* sim_update */
        if (client.state == CLIENT_CONNECTED)
        {
            struct snake* snake =
                snake_bmap_find(world.snakes, client.snake_id);
            CLITHER_DEBUG_ASSERT(snake != NULL);
            if (!snake_is_dead(snake))
            {
                /*
                 * Map "input" to "command". This converts the mouse and
                 * keyboard information into a structure that lets us step the
                 * snake forwards in time.
                 *
                 * If a bot was created, then it has precedence over the mouse
                 * controls of the graphics interface.
                 */
#    if defined(CLITHER_BOT_API)
                if (bot != NULL)
                {
                    if (ibot->next_cmd(
                            bot,
                            &cmd,
                            cmd,
                            &world,
                            snake,
                            client.sim_tick_rate) != 0)
                        break;
                }
#    endif
#    if defined(CLITHER_GFX)
#        if defined(CLITHER_BOT_API)
                if (*gfx != NULL && bot == NULL)
#        else
                if (*gfx != NULL)
#        endif
                {
                    cmd = cmd_next(cmd, &input);
                }
#    endif

                /*
                 * Append the new command to the ring buffer of unconfirmed
                 * commands. This entire list is sent to the server every
                 * network update so in the event of packet loss, the server
                 * always has a complete history of what our snake has done,
                 * frame by frame. When the server acknowledges our move, we
                 * remove all commands that date back before and up to that
                 * point in time from the list again.
                 */
                cmd_queue_put(&snake->cmdq, cmd, client.frame_number);

                /* Update snake */
                snake_eat_food(&snake->head, &snake->param, world.food_bmap);
                snake_remove_stale_segments_with_rollback_constraint(
                    &snake->data,
                    &snake->remote.ack,
                    snake_step(
                        &snake->data,
                        &snake->head,
                        &snake->param,
                        cmd,
                        client.sim_tick_rate));
            }

            camera_update(
                &camera,
                &snake->head,
                &snake->param,
                &input,
                client.sim_tick_rate);

            /* Simulate other snakes */
            {
                struct sim_other_snakes_ctx ctx;
                ctx.client = &client;
                ctx.world = &world;
                snake_bmap_retain(world.snakes, sim_other_snakes, &ctx);
            }

            if (net_update)
            {
                /* Send all unconfirmed commands (unreliable) */
                msg_commands(&client.pending_msgs, &snake->cmdq);
            }
        }

        if (net_update && client.state != CLIENT_DISCONNECTED)
        {
            if (client_send_pending_data(&client) < 0)
                break;
        }

#    if defined(CLITHER_GFX)
        if (*gfx != NULL)
            (*igfx)->step_anim(*gfx, client.sim_tick_rate);
#    endif

        /*
         * Skip rendering if we are lagging, as this is most likely the source
         * of the delay. If for some reason we end up 3 seconds behind where we
         * should be, quit.
         */
        tick_lag =
            tick_wait_warp(&sim_tick, client.warp, client.sim_tick_rate * 10);
        client.warp = 0;
        if (tick_lag == 0)
        {
#    if defined(CLITHER_GFX)
            if (*gfx != NULL)
                (*igfx)->draw(*gfx, &world, ui, &camera);
#    endif
        }
        else
        {
            log_dbg("Client is lagging! %d frames behind\n", tick_lag);
            if (tick_lag > client.sim_tick_rate * 3) /* 3 seconds */
            {
                tick_skip(&sim_tick);
                break;
            }
        }

        client.frame_number++;
    }
    log_info("Stopping client\n");

    /* Send quit message to server to be nice */
    if (client.state == CLIENT_CONNECTED)
    {
        client.timeout_counter = 0;
        client_queue(&client, msg_leave());
        client_send_pending_data(&client);
    }

    world_deinit(&world);
    if (client.state != CLIENT_DISCONNECTED)
        client_disconnect(&client);
    input_deinit(&input);
client_connect_failed:
    client_deinit(&client);
    ui_destroy(ui);
create_ui_failed:
    if (pack_watch != NULL)
        fs_watch_deinit(pack_watch);
watch_resource_pack_failed:
    log_set_prefix("");
    log_set_colors("", "");
    return (void*)(intptr_t)retval;
}
#endif
