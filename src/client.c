#include "clither/args.h"
#include "clither/camera.h"
#include "clither/cli_colors.h"
#include "clither/client.h"
#include "clither/gfx.h"
#include "clither/input.h"
#include "clither/log.h"
#include "clither/msg.h"
#include "clither/msg_vec.h"
#include "clither/net.h"
#include "clither/resource_pack.h"
#include "clither/signals.h"
#include "clither/snake.h"
#include "clither/snake_btree.h"
#include "clither/str.h"
#include "clither/tick.h"
#include "clither/world.h"
#include <string.h> /* memcpy */
#if defined(CLITHER_MCD)
#    include "clither/mcd_wifi.h"
#    include "clither/thread.h"
#endif

/* ------------------------------------------------------------------------- */
void client_init(struct client* client)
{
    client->username = NULL;
    client->sim_tick_rate = 60;
    client->net_tick_rate = 20;
    client->timeout_counter = 0;
    client->frame_number = 0;
    client->snake_id = 0;
    client->warp = 0;
    client->state = CLIENT_DISCONNECTED;

    msg_vec_init(&client->pending_msgs);
    sockfd_vec_init(&client->udp_sockfds);
}

/* ------------------------------------------------------------------------- */
void client_deinit(struct client* client)
{
    if (client->state != CLIENT_DISCONNECTED)
        client_disconnect(client);

    str_deinit(client->username);
    sockfd_vec_deinit(client->udp_sockfds);
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
    CLITHER_DEBUG_ASSERT(vec_count(client->udp_sockfds) == 0);
    CLITHER_DEBUG_ASSERT(str_len(client->username) == 0);

    if (!*server_address)
        server_address = NET_DEFAULT_ADDRESS;
    if (!*server_address)
    {
        log_err(
            "No server IP address was specified! Can't init client socket\n");
        log_err(
            "You can use --ip <address> to specify an address to connect to\n");
        return -1;
    }

    if (!*port)
        port = NET_DEFAULT_PORT;

    if (str_set_cstr(&client->username, username) != 0)
        return -1;
    if (net_connect(&client->udp_sockfds, server_address, port) < 0)
        return -1;

    client_queue(
        client, msg_join_request(0x0000, client->frame_number, username));

    client->state = CLIENT_JOINING;

    return 0;
}

/* ------------------------------------------------------------------------- */
void client_disconnect(struct client* client)
{
    int*         sockfd;
    struct msg** msg;

    CLITHER_DEBUG_ASSERT(client->state != CLIENT_DISCONNECTED);
    CLITHER_DEBUG_ASSERT(vec_count(client->udp_sockfds) != 0);
    CLITHER_DEBUG_ASSERT(str_len(client->username) != 0);

    str_deinit(client->username);
    client->username = NULL;

    vec_for_each (client->udp_sockfds, sockfd)
        net_close(*sockfd);
    sockfd_vec_clear(client->udp_sockfds);

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
struct append_msgs_ctx
{
    uint16_t frame_number;
    int      len;
    uint8_t  buf[NET_MAX_UDP_PACKET_SIZE];
};
static int append_unreliable_msgs_to_buf(struct msg** pmsg, void* user)
{
    uint8_t                 type;
    struct append_msgs_ctx* ctx = user;
    struct msg*             msg = *pmsg;

    if (ctx->len + msg->payload_len + 2 > NET_MAX_UDP_PACKET_SIZE)
        return VEC_RETAIN;
    if (msg_is_reliable(msg))
        return VEC_RETAIN;

    log_net(
        "Packing msg type=%d, len=%d, resend=%d\n",
        msg->type,
        msg->payload_len,
        msg->resend_rate);

    type = (uint8_t)msg->type;
    memcpy(ctx->buf + ctx->len + 0, &type, 1);
    memcpy(ctx->buf + ctx->len + 1, &msg->payload_len, 1);
    memcpy(ctx->buf + ctx->len + 2, msg->payload, msg->payload_len);

    ctx->len += msg->payload_len + 2;
    msg_free(msg);
    return VEC_ERASE;
}
static int append_reliable_msgs_to_buf(struct msg** pmsg, void* user)
{
    uint8_t                 type;
    struct append_msgs_ctx* ctx = user;
    struct msg*             msg = *pmsg;

    if (ctx->len + msg->payload_len + 2 > NET_MAX_UDP_PACKET_SIZE)
        return VEC_RETAIN;
    if (msg_is_unreliable(msg))
        return VEC_RETAIN;

    if (--msg->resend_rate_counter > 0)
        return VEC_RETAIN;
    msg->resend_rate_counter = msg->resend_rate;

    /*
     * Some messages in the reliable queue contain the frame number they were
     * sent on as part of the message payload. These numbers need to be updated
     * to the most recent frame number every time they are resent so the server
     * is able to differentiate them.
     */
    msg_update_frame_number(msg, ctx->frame_number);

    log_net(
        "Packing msg type=%d, len=%d, resend=%d\n",
        msg->type,
        msg->payload_len,
        msg->resend_rate);

    type = (uint8_t)msg->type;
    memcpy(ctx->buf + ctx->len + 0, &type, 1);
    memcpy(ctx->buf + ctx->len + 1, &msg->payload_len, 1);
    memcpy(ctx->buf + ctx->len + 2, msg->payload, msg->payload_len);

    ctx->len += msg->payload_len + 2;
    return VEC_RETAIN;
}

/* ------------------------------------------------------------------------- */
int client_send_pending_data(struct client* client)
{
    struct append_msgs_ctx ctx;

    /* Append unreliable messages first before appending reliable */
    ctx.len = 0;
    ctx.frame_number = client->frame_number;
    msg_vec_retain(client->pending_msgs, append_unreliable_msgs_to_buf, &ctx);
    msg_vec_retain(client->pending_msgs, append_reliable_msgs_to_buf, &ctx);

    if (ctx.len > 0)
    {
        /*
         * The client was initialized with a list of possible sockets. This is
         * because we can't know without first communicating with the server
         * which protocol (IPv4 vs IPv6) is being used. If a send call fails,
         * and there are more sockets, simply close the one that failed and try
         * the next one.
         */
    retry_send:
        log_net("Sending UDP packet, size=%d\n", ctx.len);
        CLITHER_DEBUG_ASSERT(vec_count(client->udp_sockfds) > 0);
        if (net_send(*vec_last(client->udp_sockfds), ctx.buf, ctx.len) < 0)
        {
            if (vec_count(client->udp_sockfds) == 1)
                return -1;
            net_close(*sockfd_vec_pop(client->udp_sockfds));
            log_info("Attempting to use next socket\n");
            goto retry_send;
        }
        client->timeout_counter++;
    }

    if (client->timeout_counter > 100)
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
        default: {
            log_warn(
                "Received unknown message type \"%d\" from server. "
                "Malicious?\n",
                msg_type);
        }
        break;

        case MSG_JOIN_REQUEST: break;

        case MSG_JOIN_ACCEPT: {
            uint16_t rtt;

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
            if (world_create_snake(
                    world,
                    client->snake_id,
                    pp.join_accept.spawn,
                    str_cstr(client->username)) == NULL)
            {
                return client_recv_error();
            }

            log_net(
                "MSG_JOIN_ACCEPT:\n"
                "  server frame=%d, client frame=%d, rtt=%d\n"
                "  spawn=%d, %d\n",
                pp.join_accept.server_frame,
                client->frame_number,
                rtt,
                pp.join_accept.spawn.x,
                pp.join_accept.spawn.y);

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
            if (client->warp == 0)
                client->warp = pp.feedback.diff * 10;
            return client_recv_ok();
        }

        case MSG_SNAKE_HEAD: {
            struct snake* snake =
                snake_btree_find(world->snakes, client->snake_id);
            snake_ack_frame(
                &snake->data,
                &snake->head_ack,
                &snake->head,
                &pp.snake_head.head,
                &snake->param,
                &snake->cmdq,
                pp.snake_head.frame_number,
                client->sim_tick_rate);
            return client_recv_ok();
        }

        case MSG_SNAKE_BEZIER: {
            struct snake* snake =
                snake_btree_find(world->snakes, pp.snake_bezier.snake_id);
            if (snake == NULL)
                break; /* Have to wait for MSG_SNAKE_CREATE to arrive */
            return client_recv_ok();
        }

        case MSG_SNAKE_BEZIER_ACK: break;
    }

    return client_recv_ok();
}

/* ------------------------------------------------------------------------- */
static struct client_recv_result unpack_packet(
    struct client*               client,
    struct world*                world,
    const struct net_udp_packet* packet)
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
    struct net_udp_packet packet;

    CLITHER_DEBUG_ASSERT(vec_count(client->udp_sockfds) > 0);

    log_net("client_recv() frame=%d\n", client->frame_number);

retry_recv:
    packet.len = net_recv(
        *vec_last(client->udp_sockfds), packet.data, sizeof(packet.data));
    if (packet.len < 0)
    {
        if (vec_count(client->udp_sockfds) == 1)
            return client_recv_error();
        net_close(*sockfd_vec_pop(client->udp_sockfds));
        log_info("Attempting to use next socket\n");
        goto retry_recv;
    }
    log_net("Received UDP packet, size=%d\n", packet.len);

    /* Don't let client time out */
    client->timeout_counter = 0;

    return unpack_packet(client, world, &packet);
}

/* ------------------------------------------------------------------------- */
#if defined(CLITHER_GFX)
void* client_run(const struct args* a)
{
#    if defined(CLITHER_MCD)
    struct thread* mcd_thread;
#    endif
    struct world                world;
    struct input                input;
    struct cmd                  cmd;
    const struct gfx_interface* gfx_iface;
    struct gfx*                 gfx;
    struct resource_pack*       pack;
    struct camera               camera;
    struct client               client;
    struct tick                 sim_tick;
    struct tick                 net_tick;
    int                         tick_lag;

    /* Change log prefix and color for server log messages */
    log_set_prefix("Client: ");
    log_set_colors(COL_B_GREEN, COL_RESET);

    /* If McDonald's WiFi is enabled, start that */
    client_init(&client);
#    if defined(CLITHER_MCD)
    if (a->mcd_latency > 0)
    {
        mcd_thread = thread_start(run_mcd_wifi, a);
        if (mcd_thread == NULL)
            goto start_mcd_failed;
        if (client_connect(&client, a->ip, a->mcd_port, "username") < 0)
            goto client_connect_failed;
    }
    else
#    endif
    {
        /*
         * TODO: In the future the GUI will take care of connecting. Here we do
         * it immediately because there is no menu.
         */
        if (client_connect(&client, a->ip, a->port, "username") < 0)
            goto client_connect_failed;
    }

    /* Init all graphics and create window */
    gfx_iface = gfx_backends[a->gfx_backend];
    log_info("Using graphics backend: %s\n", gfx_iface->name);
    if (gfx_iface->global_init() < 0)
        goto init_gfx_failed;
    gfx = gfx_iface->create(800, 600);
    if (gfx == NULL)
        goto create_gfx_failed;

    pack = resource_pack_parse("packs/horror");
    if (pack == NULL)
        goto parse_resource_pack_failed;
    if (gfx_iface->load_resource_pack(gfx, pack) < 0)
        goto load_resource_pack_failed;

    input_init(&input);
    camera_init(&camera);
    cmd = cmd_default();
    world_init(&world);

    log_info("Client started\n");

    tick_cfg(&sim_tick, client.sim_tick_rate);
    tick_cfg(&net_tick, client.net_tick_rate);
    while (signals_exit_requested() == 0)
    {
        int net_update;

        gfx_iface->poll_input(gfx, &input);
        if (input.quit)
            break;

        /* Switch graphics backends */
        if (input.next_gfx_backend || input.prev_gfx_backend)
        {
            int count;
            int idx, new_idx;

            for (count = 0; gfx_backends[count]; ++count)
            {
            }
            for (idx = 0; gfx_backends[idx]; ++idx)
                if (gfx_iface == gfx_backends[idx])
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
            gfx_iface->destroy(gfx);
            gfx_iface->global_deinit();

            gfx_iface = gfx_backends[new_idx];
            if (gfx_iface->global_init() < 0)
                goto init_new_gfx_failed;

            gfx = gfx_iface->create(640, 480);
            if (gfx == NULL)
                goto create_new_gfx_failed;

            if (gfx_iface->load_resource_pack(gfx, pack) < 0)
                goto load_new_resource_pack_failed;

            /* Clears the button press for switching graphics backends */
            input_init(&input);
            gfx_iface->poll_input(gfx, &input);

            goto create_new_gfx_success;

        load_new_resource_pack_failed:
            gfx_iface->destroy(gfx);
        create_new_gfx_failed:
            gfx_iface->global_deinit();
        init_new_gfx_failed:
            /* Try to restore to previous backend. Shouldn't fail but who knows
             */
            gfx_iface = gfx_backends[idx];
            if (gfx_iface->global_init() < 0)
                break;
            gfx = gfx_iface->create(640, 480);
            if (gfx == NULL)
            {
                gfx_iface->global_deinit();
                break;
            }
        }
    create_new_gfx_success:;

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
                snake_btree_find(world.snakes, client.snake_id);

            /*
             * Map "input" to "command". This converts the mouse and keyboard
             * information into a structure that lets us step the snake forwards
             * in time.
             */
            cmd = gfx_iface->input_to_cmd(
                cmd, &input, gfx, &camera, snake->head.pos);

            /*
             * Append the new command to the ring buffer of unconfirmed
             * commands. This entire list is sent to the server every network
             * update so in the event of packet loss, the server always has a
             * complete history of what our snake has done, frame by frame. When
             * the server acknowledges our move, we remove all commands that
             * date back before and up to that point in time from the list
             * again.
             */
            cmd_queue_put(&snake->cmdq, cmd, client.frame_number);

            /* Update snake */
            //snake_param_update(
            //    &snake->param,
            //    snake->param.upgrades,
            //    snake->param.food_eaten + 1);
            snake_remove_stale_segments_with_rollback_constraint(
                &snake->data,
                &snake->head_ack,
                snake_step(
                    &snake->data,
                    &snake->head,
                    &snake->param,
                    cmd,
                    client.sim_tick_rate));

            /* Update world */
            world_step(&world, client.frame_number, client.sim_tick_rate);

            camera_update(
                &camera, &snake->head, &snake->param, client.sim_tick_rate);

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

        gfx_iface->step_anim(gfx, client.sim_tick_rate);

        /*
         * Skip rendering if we are lagging, as this is most likely the source
         * of the delay. If for some reason we end up 3 seconds behind where we
         * should be, quit.
         */
        tick_lag =
            tick_wait_warp(&sim_tick, client.warp, client.sim_tick_rate * 10);
        if (tick_lag == 0)
            gfx_iface->draw_world(gfx, &world, &camera);
        else
        {
            log_dbg("Client is lagging! %d frames behind\n", tick_lag);
            if (tick_lag > client.sim_tick_rate * 3) /* 3 seconds */
            {
                tick_skip(&sim_tick);
                break;
            }
        }

        if (client.warp > 0)
            client.warp--;
        if (client.warp < 0)
            client.warp++;

        client.frame_number++;
    }
    log_info("Stopping client\n");

    /* Send quit message to server to be nice */
    client_queue(&client, msg_leave());
    client_send_pending_data(&client);

    world_deinit(&world);
load_resource_pack_failed:
    resource_pack_destroy(pack);
parse_resource_pack_failed:
    gfx_iface->destroy(gfx);
create_gfx_failed:
    gfx_iface->global_deinit();
init_gfx_failed:
    if (client.state != CLIENT_DISCONNECTED)
        client_disconnect(&client);
client_connect_failed:
    /* Stop McDonald's WiFi if necessary */
#    if defined(CLITHER_MCD)
    if (a->mcd_latency > 0)
    {
        thread_join(mcd_thread);
        log_dbg("Joined McDonald's WiFi thread\n");
    }
start_mcd_failed:
#    endif
    client_deinit(&client);
    log_set_colors("", "");
    log_set_prefix("");

    return NULL;
}
#endif
