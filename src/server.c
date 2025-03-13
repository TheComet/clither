#include "clither/args.h"
#include "clither/cli_colors.h"
#include "clither/log.h"
#include "clither/msg_vec.h"
#include "clither/net.h"
#include "clither/net_addr_hm.h"
#include "clither/server.h"
#include "clither/server_client.h"
#include "clither/server_client_hm.h"
#include "clither/server_instance.h"
#include "clither/server_instance_btree.h"
#include "clither/server_settings.h"
#include "clither/snake.h"
#include "clither/snake_btree.h"
#include "clither/thread.h"
#include "clither/world.h"
#include "clither/wrap.h"
#include <stdlib.h> /* atoi */
#include <string.h> /* memcpy */

/* ------------------------------------------------------------------------- */
static void client_remove(
    struct server*              server,
    struct world*               world,
    const struct net_addr*      addr,
    const struct server_client* client)
{
    struct msg** pmsg;

    world_remove_snake(world, client->snake_id);
    vec_for_each (client->pending_msgs, pmsg)
        msg_free(*pmsg);
    msg_vec_deinit(client->pending_msgs);
    server_client_hm_erase(server->clients, addr);
}

/* ------------------------------------------------------------------------- */
static void mark_client_as_malicious_and_drop(
    struct server*              server,
    const struct net_addr*      addr,
    const struct server_client* client,
    struct world*               world,
    int                         timeout)
{
    net_addr_hm_insert_update(&server->malicious_clients, addr, timeout);
    client_remove(server, world, addr, client);
}

/* ------------------------------------------------------------------------- */
int server_init(
    struct server* server, const char* bind_address, const char* port)
{
    server->udp_sock = net_bind(bind_address, port);
    if (server->udp_sock < 0)
        return -1;

    server_client_hm_init(&server->clients);
    net_addr_hm_init(&server->malicious_clients);
    net_addr_hm_init(&server->banned_clients);

    return 0;
}

/* ------------------------------------------------------------------------- */
void server_deinit(struct server* server)
{
    const struct net_addr* addr;
    struct server_client*  client;
    int                    slot;

    net_close(server->udp_sock);

    net_addr_hm_deinit(server->banned_clients);
    net_addr_hm_deinit(server->malicious_clients);

    server_client_hm_for_each (server->clients, slot, addr, client)
    {
        struct msg** pmsg;
        (void)addr;

        vec_for_each (client->pending_msgs, pmsg)
            msg_free(*pmsg);
        msg_vec_deinit(client->pending_msgs);
    }
    server_client_hm_deinit(server->clients);
}

/* ------------------------------------------------------------------------- */
static void cbf_add(struct server_client* client, int value)
{
    memmove(
        &client->cbf_window[1],
        &client->cbf_window[0],
        sizeof(int) * (CBF_WINDOW_SIZE - 1));
    client->cbf_window[0] = value;
}

/* ------------------------------------------------------------------------- */
static int cbf_lower_bound(struct server_client* client)
{
    int i;
    int lower = INT32_MAX;
    for (i = 0; i != CBF_WINDOW_SIZE; ++i)
        if (lower > client->cbf_window[i])
            lower = client->cbf_window[i];
    return lower;
}

/* ------------------------------------------------------------------------- */
struct append_msgs_ctx
{
    int  len;
    char buf[NET_MAX_UDP_PACKET_SIZE];
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
int server_send_pending_data(struct server* server)
{
    int                    slot;
    const struct net_addr* addr;
    struct server_client*  client;
    struct append_msgs_ctx ctx;

    server_client_hm_for_each (server->clients, slot, addr, client)
    {
        /* Append unreliable messages first */
        ctx.len = 0;
        msg_vec_retain(
            client->pending_msgs, append_unreliable_msgs_to_buf, &ctx);
        msg_vec_retain(client->pending_msgs, append_reliable_msgs_to_buf, &ctx);

        if (ctx.len == 0)
            continue;

        /* NOTE: The hashmap's key size contains the length of the stored
         * address */
        log_net("Sending UDP packet, size=%d\n", ctx.len);
        net_sendto(server->udp_sock, ctx.buf, ctx.len, addr);
        client->timeout_counter++;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
static int server_queue(struct server_client* client, struct msg* msg)
{
    return msg_vec_push(&client->pending_msgs, msg);
}

/* ------------------------------------------------------------------------- */
void server_queue_snake_data(
    struct server* server, const struct world* world, uint16_t frame_number)
{
    int                    slot;
    int                    other_slot;
    const struct net_addr* addr;
    const struct net_addr* other_addr;
    struct server_client*  client;
    struct server_client*  other_client;

    server_client_hm_for_each (server->clients, slot, addr, client)
    {
        struct snake* snake = snake_btree_find(world->snakes, client->snake_id);
        if (snake_is_held(snake))
            continue;
        server_queue(client, msg_snake_head(&snake->head, frame_number));
    }

    server_client_hm_for_each (server->clients, slot, addr, client)
    {
        server_client_hm_for_each (
            server->clients, other_slot, other_addr, other_client)
        {
            struct snake* snake;
            struct snake* other_snake;
            qw            dx, dy, dist_sq;

            /* OK to compare pointers here -- they're from the same hashmap */
            if (addr == other_addr)
                continue;

            snake = snake_btree_find(world->snakes, client->snake_id);
            other_snake =
                snake_btree_find(world->snakes, other_client->snake_id);

            /* TODO better distance check using bezier AABBs */
            dx = qw_sub(snake->head.pos.x, other_snake->head.pos.x);
            dy = qw_sub(snake->head.pos.y, other_snake->head.pos.y);
            dist_sq = qw_add(qw_mul(dx, dx), qw_mul(dy, dy));
            if (dist_sq > make_qw(1 * 1))
                continue;

            /* TODO queue bezier handles */
        }
    }
}

/* ------------------------------------------------------------------------- */
static int process_message(
    struct server*                server,
    const struct server_settings* settings,
    struct server_client*         client,
    const struct net_addr*        client_addr,
    struct world*                 world,
    enum msg_type                 msg_type,
    const uint8_t*                msg_data,
    uint8_t                       msg_len,
    uint16_t                      frame_number)
{
    /*
     * NOTE: Beyond this point, "client" won't be NULL *unless* the
     * message is MSG_JOIN_REQUEST. This makes the switch/case handling
     * a little easier for all other cases
     */

    union parsed_payload pp;
    switch (msg_parse_payload(&pp, msg_type, msg_data, msg_len))
    {
        case MSG_JOIN_REQUEST: {
            if (hm_count(server->clients) + 1 > settings->max_players)
            {
                struct net_udp_packet pkt;
                struct msg* msg = msg_join_deny_server_full("Server full");
                pkt.len = msg->payload_len + 2;
                pkt.data[0] = msg->type;
                pkt.data[1] = msg->payload_len;
                memcpy(pkt.data + 2, msg->payload, msg->payload_len);
                net_sendto(server->udp_sock, pkt.data, pkt.len, client_addr);
                msg_free(msg);
                return 0;
            }

            if (pp.join_request.username_len > settings->max_username_len)
            {
                struct net_udp_packet pkt;
                struct msg*           msg =
                    msg_join_deny_bad_username("Username too long");
                pkt.len = msg->payload_len + 2;
                pkt.data[0] = msg->type;
                pkt.data[1] = msg->payload_len;
                memcpy(pkt.data + 2, msg->payload, msg->payload_len);
                net_sendto(server->udp_sock, pkt.data, pkt.len, client_addr);
                msg_free(msg);
                return 0;
            }

            /* Create new client. This code is not refactored into a
             * separate function because this is the only location where
             * clients are created. Clients are destroyed by the
             * function client_remove() */
            if (client == NULL)
            {
                struct snake* snake;
                int           cbf_idx;
                log_net("MSG_JOIN_REQUEST \"%s\"\n", pp.join_request.username);

                client =
                    server_client_hm_emplace_new(&server->clients, client_addr);
                CLITHER_DEBUG_ASSERT(client != NULL);

                msg_vec_init(&client->pending_msgs);
                /* hashmap_init(&client->bezier_handles_ack,*/
                /* sizeof(struct bezier_handle), 0);*/
                client->timeout_counter = 0;
                client->snake_id =
                    world_spawn_snake(world, pp.join_request.username);
                client->last_command_msg_frame = frame_number;

                /* Hold the snake in place until we receive the first
                 * command */
                snake = snake_btree_find(world->snakes, client->snake_id);
                CLITHER_DEBUG_ASSERT(snake != NULL);
                snake_set_hold(snake);

                /*
                 * Init "Command Buffer Fullness" queue with minimum
                 * granularity. This assumes the client has the most
                 * stable connection initially.
                 */
                for (cbf_idx = 0; cbf_idx != CBF_WINDOW_SIZE; ++cbf_idx)
                    client->cbf_window[cbf_idx] =
                        settings->sim_tick_rate / settings->net_tick_rate;
            }

            /* (Re-)send join accept response */
            {
                struct snake* snake =
                    snake_btree_find(world->snakes, client->snake_id);
                struct msg* response = msg_join_accept(
                    settings->sim_tick_rate,
                    settings->net_tick_rate,
                    pp.join_request.frame,
                    frame_number,
                    client->snake_id,
                    &snake->head.pos);
                msg_vec_push(&client->pending_msgs, response);
            }
            return 0;
        }

        case MSG_JOIN_ACCEPT:
        case MSG_JOIN_DENY_BAD_PROTOCOL:
        case MSG_JOIN_DENY_BAD_USERNAME:
        case MSG_JOIN_DENY_SERVER_FULL: {
            log_warn("Server received unexpected message type %d\n", msg_type);
            break;
        }

        case MSG_LEAVE: {
            client_remove(server, world, client_addr, client);
            return 0;
        }

        case MSG_COMMANDS: {
            uint16_t      first_frame, last_frame;
            int           lower;
            struct snake* snake =
                snake_btree_find(world->snakes, client->snake_id);
            int granularity = settings->sim_tick_rate / settings->net_tick_rate;

            /*
             * Measure how many frames are in the client's command
             * buffer.
             *
             * A negative value indicates the client is lagging behind,
             * and the server will have to make a prediction, which will
             * lead to the client having to re-simulate. A positive
             * value indicates there are commands in the buffer.
             * Depending on how stable the connection is, the client
             * will be instructed to shrink the buffer.
             */
            int client_commands_queued =
                u16_sub_wrap(cmd_queue_frame_end(&snake->cmdq), frame_number);

            /* Returns the first and last frame numbers that were
             * unpacked from the message */
            if (msg_commands_unpack_into(
                    &snake->cmdq,
                    msg_data,
                    msg_len,
                    frame_number,
                    &first_frame,
                    &last_frame) != 0)
            {
                break; /* something is wrong with the message */
            }

            /*
             * This handles packets being reordered by dropping any
             * commands older than the last command received
             */
            if (u16_le_wrap(last_frame, client->last_command_msg_frame))
                return 0;
            client->last_command_msg_frame = last_frame;

            /*
             * Compare the very last frame received with the current
             * frame number. By tracking the lower and upper boundaries
             * of this difference over time, it can give a good estimate
             * of how "healthy" the client's connection is and whether
             * the command buffer size needs to be increased or
             * decreased.
             */
            cbf_add(client, client_commands_queued);
            lower = cbf_lower_bound(client);
            if (client_commands_queued < 0)
            {
                /*
                 * Means we do NOT have the command of the current frame
                 *  -> server is going to make a prediction
                 *  -> will probably lead to a client-side roll back
                 * The client needs to warp forwards in time.
                 */
                int8_t diff =
                    client_commands_queued < -10 ? -10 : client_commands_queued;
                server_queue(client, msg_feedback(diff, frame_number));
            }
            else if (lower - granularity * 2 > 0)
            {
                int8_t diff = lower - granularity * 2;
                diff = diff > 10 ? 10 : diff;
                server_queue(client, msg_feedback(diff, frame_number));
            }
            return 0;
        }
    }

    mark_client_as_malicious_and_drop(
        server, client_addr, client, world, settings->malicious_timeout);

    return 0;
}

/* ------------------------------------------------------------------------- */
static int unpack_packet(
    struct server*                server,
    const struct server_settings* settings,
    struct server_client*         client,
    const struct net_addr*        client_addr,
    struct world*                 world,
    const uint8_t*                udp_buf,
    int                           udp_len,
    uint16_t                      frame_number)
{
    /*
     * Packet can contain multiple message objects.
     * buf[0] == message type
     * buf[1] == message payload length
     * buf[2] == beginning of message payload
     */
    int i;
    for (i = 0; i < udp_len - 1;)
    {
        enum msg_type  type = udp_buf[i + 0];
        const uint8_t  msg_len = udp_buf[i + 1];
        const uint8_t* msg = &udp_buf[i + 2];

        log_net("Unpacking msg type=%d, len=%d\n", type, msg_len);

        if (i + 2 + msg_len > udp_len)
        {
            struct net_addr_str ipstr;
            net_addr_to_str(&ipstr, client_addr);
            log_warn(
                "Invalid payload length \"%d\" received from client %s\n",
                (int)msg_len,
                ipstr.cstr);
            log_warn("Dropping rest of packet\n");
            mark_client_as_malicious_and_drop(
                server,
                client_addr,
                client,
                world,
                settings->malicious_timeout);
            break;
        }

        /*
         * Disallow receiving packets from clients that are not registered
         * with the exception of the "join game request" message.
         */
        if (client == NULL && type != MSG_JOIN_REQUEST)
        {
            struct net_addr_str ipstr;
            net_addr_to_str(&ipstr, client_addr);
            log_warn(
                "Received packet from unknown client %s, ignoring\n",
                ipstr.cstr);
            break;
        }

        if (process_message(
                server,
                settings,
                client,
                client_addr,
                world,
                type,
                msg,
                msg_len,
                frame_number) != 0)
        {
            return 0;
        }

        i += msg_len + 2;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
int server_recv(
    struct server*                server,
    const struct server_settings* settings,
    struct world*                 world,
    uint16_t                      frame_number)
{
    uint8_t                udp_buf[NET_MAX_UDP_PACKET_SIZE];
    const struct net_addr* server_addr;
    struct net_addr        client_addr;
    struct server_client*  client;
    int                    slot;
    int*                   timeout;

    log_net("server_recv() frame=%d\n", frame_number);

    /* Update timeout counters of every client that we've communicated with */
    server_client_hm_for_each (server->clients, slot, server_addr, client)
    {
        client->timeout_counter++;

        if (client->timeout_counter >
            settings->client_timeout * settings->net_tick_rate)
        {
            struct net_addr_str ipstr;
            net_addr_to_str(&ipstr, server_addr);
            log_warn("Client %s timed out\n", ipstr.cstr);
            client_remove(server, world, server_addr, client);
        }
    }

    /* Update malicious client timeouts */
    net_addr_hm_for_each (server->malicious_clients, slot, server_addr, timeout)
    {
        struct net_addr_str ipstr;

        if (--(*timeout) > 0)
            continue;

        net_addr_to_str(&ipstr, server_addr);
        log_info("Client %s removed from malicious list\n", ipstr.cstr);
        net_addr_hm_erase(server->malicious_clients, server_addr);
    }

    /* We may need to read more than one UDP packet */
    while (1)
    {
        struct server_client* client;
        int                   udp_len;

        udp_len = net_recvfrom(
            server->udp_sock, udp_buf, sizeof(udp_buf), &client_addr);

        /* Nothing received or error */
        if (udp_len <= 0)
            return udp_len;
        log_net("Received UDP packet, size=%d\n", udp_len);

        /*
         * If we received a packet from a banned client, ignore packet
         */
        if (net_addr_hm_find(server->banned_clients, &client_addr))
            continue;

        /*
         * If we received a packet from a potentially malicious client,
         * increase their timeout
         */
        {
            int* timeout =
                net_addr_hm_find(server->malicious_clients, &client_addr);
            if (timeout != NULL)
            {
                *timeout +=
                    settings->malicious_timeout * settings->net_tick_rate;
                continue;
            }
        }

        /*
         * If we received a packet from a registered client, reset their timeout
         * counter
         */
        client = server_client_hm_find(server->clients, &client_addr);
        if (client != NULL)
            client->timeout_counter = 0;

        if (unpack_packet(
                server,
                settings,
                client,
                &client_addr,
                world,
                udp_buf,
                udp_len,
                frame_number) != 0)
        {
            return -1;
        }
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
void* server_run(const void* args)
{
    struct server_instance_btree* instances;
    struct server_settings        settings;
    const struct args*            a = args;

    /* Change log prefix and color for server log messages */
    log_set_prefix("Server: ");
    log_set_colors(COL_B_CYAN, COL_RESET);

    mem_init_threadlocal();

    server_instance_btree_init(&instances);

    if (server_settings_load_or_set_defaults(&settings, a->config_file) < 0)
        goto load_settings_failed;

    /*
     * Create the default server instance. This is always active, regardless of
     * how many players are connected.
     */
    {
        /*
         * The port passed in over the command line has precedence over the port
         * specified in the config file. Note that the port obtained from the
         * settings structure is always initialized, regardless of whether the
         * config file existed or not.
         */
        struct server_instance* instance;
        const char*             port = *a->port ? a->port : settings.port;
        uint16_t                key = atoi(port);
        CLITHER_DEBUG_ASSERT(key != 0);

        if (server_instance_btree_emplace_new(&instances, key, &instance) !=
            BTREE_NEW)
        {
            goto start_default_instance_failed;
        }
        instance->settings = &settings;
        instance->ip = a->ip;
        strcpy(instance->port, port);

        log_dbg("Starting default server instance\n");
        instance->thread = thread_start(server_instance_run, instance);
        if (instance->thread == NULL)
        {
            log_err(
                "Failed to start the default server instance! Can't "
                "continue\n");
            goto start_default_instance_failed;
        }
    }

    /* For now we don't create more instances once the server fills up */
    {
        int16_t                 idx;
        uint16_t                port;
        struct server_instance* instance;
        btree_for_each (instances, idx, port, instance)
        {
            (void)port;
            thread_join(instance->thread);
        }
        log_dbg("Joined all server instances\n");
    }

    server_settings_save(&settings, a->config_file);

    server_instance_btree_deinit(instances);
    mem_deinit_threadlocal();
    log_set_colors("", "");
    log_set_prefix("");

    return (void*)0;

start_default_instance_failed:
load_settings_failed:
    server_instance_btree_deinit(instances);
    mem_deinit_threadlocal();
    log_set_colors("", "");
    log_set_prefix("");
    return (void*)-1;
}
