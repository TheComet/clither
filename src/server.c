#include "clither/args.h"
#include "clither/bezier_knot_acks_bmap.h"
#include "clither/bezier_knot_rb.h"
#include "clither/cli_colors.h"
#include "clither/log.h"
#include "clither/msg_vec.h"
#include "clither/net.h"
#include "clither/net_addr_hm.h"
#include "clither/proximity_state_bmap.h"
#include "clither/server.h"
#include "clither/server_client.h"
#include "clither/server_client_hm.h"
#include "clither/server_instance.h"
#include "clither/server_instance_bmap.h"
#include "clither/settings.h"
#include "clither/snake.h"
#include "clither/snake_bmap.h"
#include "clither/thread.h"
#include "clither/world.h"
#include "clither/wrap.h"
#include <stdlib.h> /* atoi */
#include <string.h> /* memcpy */

/* ------------------------------------------------------------------------- */
static int server_queue(struct server_client* client, struct msg* msg)
{
    return msg_vec_push(&client->pending_msgs, msg);
}

/* ------------------------------------------------------------------------- */
static void server_client_remove(
    struct server*         server,
    struct world*          world,
    const struct net_addr* addr,
    struct server_client*  client)
{
    int16_t               idx;
    struct net_addr       other_addr;
    struct server_client* other_client;

    /* Other clients might still have this client in their proximity list. If
     * so, we need to remove this client and also send MSG_SNAKE_DESTROY so all
     * other clients destroy the snake. */
    hm_for_each (server->clients, idx, other_addr, other_client)
    {
        struct proximity_state* prox;
        (void)idx, (void)other_addr;

        prox = proximity_state_bmap_find(
            other_client->snakes_in_proximity, client->snake_id);
        if (prox == NULL)
            continue;
        proximity_state_deinit(prox);
        proximity_state_bmap_erase(
            other_client->snakes_in_proximity, client->snake_id);

        if (other_client != client)
            server_queue(other_client, msg_snake_destroy(client->snake_id));
    }

    world_remove_snake(world, client->snake_id);
    server_client_deinit(client);
    server_client_hm_erase(server->clients, addr);
}

/* ------------------------------------------------------------------------- */
static void mark_client_as_malicious_and_drop(
    struct server*         server,
    const struct net_addr* addr,
    struct server_client*  client,
    struct world*          world,
    int                    timeout)
{
    net_addr_hm_insert_update(&server->malicious_clients, addr, timeout);
    server_client_remove(server, world, addr, client);
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
        (void)addr, server_client_deinit(client);
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
static int cbf_min(struct server_client* client)
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

    log_net("Packing msg type=%d, len=%d", msg->type, msg->payload_len);

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

    if (--msg->resend_period_counter > 0)
        return VEC_RETAIN;
    msg->resend_period_counter = msg->resend_period;
    if (--msg->resend_retry_counter == 0)
        return log_err(
            "Client did not acknowledge reliable message: type=%d\n",
            msg->type);

    log_net(
        "Packing msg type=%d, len=%d, resend=%d, retry=%d\n",
        msg->type,
        msg->payload_len,
        msg->resend_period,
        msg->resend_retry_counter);

    type = (uint8_t)msg->type;
    memcpy(ctx->buf + ctx->len + 0, &type, 1);
    memcpy(ctx->buf + ctx->len + 1, &msg->payload_len, 1);
    memcpy(ctx->buf + ctx->len + 2, msg->payload, msg->payload_len);

    ctx->len += msg->payload_len + 2;
    return VEC_RETAIN;
}

/* ------------------------------------------------------------------------- */
int server_send_pending_data(struct server* server, struct world* world)
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
        if (msg_vec_retain(
                client->pending_msgs, append_reliable_msgs_to_buf, &ctx) == -1)
        {
            server_client_remove(server, world, addr, client);
            continue;
        }

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
int server_update_snakes_in_range(
    struct server* server, const struct world* world, qw proximity_range)
{
    int                    slot;
    int                    other_slot;
    const struct net_addr* addr;
    const struct net_addr* other_addr;
    struct server_client*  client;
    struct server_client*  other_client;

    /* TODO: O(n^2) */
    server_client_hm_for_each (server->clients, slot, addr, client)
    {
        server_client_hm_for_each (
            server->clients, other_slot, other_addr, other_client)
        {
            struct snake* snake;
            struct snake* other_snake;
            struct qwaabb other_aabb;

            /* OK to compare pointers here -- they're from the same hashmap */
            if (addr == other_addr)
                continue;

            snake = snake_bmap_find(world->snakes, client->snake_id);
            other_snake =
                snake_bmap_find(world->snakes, other_client->snake_id);
            other_aabb = other_snake->data.aabb;
            other_aabb.x1 = qw_sub(other_aabb.x1, proximity_range);
            other_aabb.y1 = qw_sub(other_aabb.y1, proximity_range);
            other_aabb.x2 = qw_add(other_aabb.x2, proximity_range);
            other_aabb.y2 = qw_add(other_aabb.y2, proximity_range);
            if (qwaabb_test_qwpos(other_aabb, snake->head.pos))
            {
                int32_t                   knot_idx;
                const struct bezier_knot* knot;
                struct proximity_state*   prox;
                enum bmap_status status = proximity_state_bmap_emplace_or_get(
                    &client->snakes_in_proximity,
                    other_client->snake_id,
                    &prox);
                switch (status)
                {
                    case BMAP_OOM: return -1;
                    case BMAP_EXISTS: continue;
                    case BMAP_NEW: break;
                }

                proximity_state_init(prox);

                /* Add all snake bezier knots to the list to send. */
                rb_for_each (other_snake->data.bezier_knots, knot_idx, knot)
                {
                    if (bezier_knot_acks_bmap_insert_new(
                            &prox->bezier_knot_acks, knot_idx, 0) == BMAP_OOM)
                        return -1;
                }
            }
            else
            {
                struct proximity_state* prox = proximity_state_bmap_find(
                    client->snakes_in_proximity, other_client->snake_id);
                if (prox == NULL)
                    continue;

                server_queue(client, msg_snake_destroy(other_client->snake_id));
                proximity_state_deinit(prox);
                proximity_state_bmap_erase(
                    client->snakes_in_proximity, other_client->snake_id);
            }
        }
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
int server_queue_snake_data(
    struct server* server, const struct world* world, uint16_t frame_number)
{
    int                    slot;
    const struct net_addr* addr;
    struct server_client*  client;

    /* Send back real position of client snake's head */
    server_client_hm_for_each (server->clients, slot, addr, client)
    {
        struct snake* snake = snake_bmap_find(world->snakes, client->snake_id);
        CLITHER_DEBUG_ASSERT(snake != NULL), (void)addr;
        if (snake_is_held(snake))
            continue;
        server_queue(
            client,
            msg_snake_head(
                frame_number,
                snake->head.pos,
                snake->head.angle,
                snake->head.speed));
    }

    /* Queue bezier knots of all snakes in proximity */
    server_client_hm_for_each (server->clients, slot, addr, client)
    {
        int16_t                 prox_idx;
        uint16_t                snake_id;
        struct proximity_state* prox;
        bmap_for_each (client->snakes_in_proximity, prox_idx, snake_id, prox)
        {
            int16_t             knot_idx;
            struct bezier_knot* knot;
            struct snake* snake = snake_bmap_find(world->snakes, snake_id);
            CLITHER_DEBUG_ASSERT(snake != NULL);
            rb_for_each (snake->data.bezier_knots, knot_idx, knot)
            {
                char* ackd;
                switch (bezier_knot_acks_bmap_emplace_or_get(
                    &prox->bezier_knot_acks, knot_idx, &ackd))
                {
                    case BMAP_OOM: return -1;
                    case BMAP_NEW: *ackd = 0;
                    case BMAP_EXISTS: break;
                }
                if (*ackd)
                    continue;

                if (server_queue(
                        client,
                        msg_knot(
                            snake_id,
                            knot_idx,
                            knot->pos,
                            knot->angle,
                            knot->len_backwards,
                            knot->len_forwards)) != 0)
                {
                    return -1;
                }
            }

            /* As the snake moves, stale knots need to be removed from the
             * acknowledgement list explicitly. Otherwise, if the client doesn't
             * ack them, then they'd remain in the list forever. */
            bezier_knot_acks_bmap_remove_stale_knots(
                prox->bezier_knot_acks, snake->data.bezier_knots);

            /* The "len_forwards" property of the second knot (the one that
             * follows the head) and the "len_backwards" property of the head
             * knot are constantly updated as the head moves. We have to send
             * this to the client as well. */
            if (rb_count(snake->data.bezier_knots) > 1)
            {
                const struct bezier_knot* head_knot =
                    rb_peek_write(snake->data.bezier_knots);
                const struct bezier_knot* second_knot = rb_peek(
                    snake->data.bezier_knots,
                    rb_count(snake->data.bezier_knots) - 2);

                server_queue(
                    client,
                    msg_bezier(
                        snake_id,
                        frame_number,
                        rb_read_idx(snake->data.bezier_knots),
                        rb_write_idx(snake->data.bezier_knots),
                        snake->head.pos,
                        snake->head.angle,
                        snake->head.speed,
                        head_knot->len_backwards,
                        second_knot->len_forwards));
            }
        }
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
enum process_message_result
{
    PROCESS_MESSAGE_OOM = -1,
    PROCESS_MESSAGE_OK,
    PROCESS_MESSAGE_CLIENT_DROPPED
};
static enum process_message_result process_message(
    struct server*                server,
    const struct settings_server* settings,
    struct server_client*         client,
    const struct net_addr*        addr,
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
                net_sendto(server->udp_sock, pkt.data, pkt.len, addr);
                msg_free(msg);
                return PROCESS_MESSAGE_OK;
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
                net_sendto(server->udp_sock, pkt.data, pkt.len, addr);
                msg_free(msg);
                return PROCESS_MESSAGE_OK;
            }

            /* Create new client */
            if (client == NULL)
            {
                struct snake* snake;
                uint16_t      snake_id;
                log_net("MSG_JOIN_REQUEST \"%s\"\n", pp.join_request.username);

                client = server_client_hm_emplace_new(&server->clients, addr);
                if (client == NULL)
                    return PROCESS_MESSAGE_OOM;

                snake_id = world_spawn_snake(world, pp.join_request.username);
                if (snake_id == 0)
                {
                    server_client_hm_erase(server->clients, addr);
                    return PROCESS_MESSAGE_OOM;
                }

                /* Hold the snake in place until we receive the first
                 * command */
                snake = snake_bmap_find(world->snakes, snake_id);
                CLITHER_DEBUG_ASSERT(snake != NULL);
                snake_set_hold(snake);

                server_client_init(
                    client,
                    snake_id,
                    frame_number,
                    settings->sim_tick_rate,
                    settings->net_tick_rate);
            }

            /* (Re-)send join accept response */
            {
                struct snake* snake;
                struct msg*   response;
                snake = snake_bmap_find(world->snakes, client->snake_id);
                CLITHER_DEBUG_ASSERT(snake != NULL);
                response = msg_join_accept(
                    settings->sim_tick_rate,
                    settings->net_tick_rate,
                    pp.join_request.frame,
                    frame_number,
                    client->snake_id,
                    &snake->head.pos);
                if (msg_vec_push(&client->pending_msgs, response) != 0)
                    return PROCESS_MESSAGE_OOM;
            }
            return PROCESS_MESSAGE_OK;
        }

        case MSG_JOIN_ACCEPT:
        case MSG_JOIN_DENY_BAD_PROTOCOL:
        case MSG_JOIN_DENY_BAD_USERNAME:
        case MSG_JOIN_DENY_SERVER_FULL: {
            log_warn("Server received unexpected message type %d\n", msg_type);
            break;
        }

        case MSG_LEAVE: {
            server_client_remove(server, world, addr, client);
            return PROCESS_MESSAGE_OK;
        }

        case MSG_COMMANDS: {
            uint16_t      first_frame, last_frame;
            int           lower, granularity, cmd_buf_fullness;
            struct snake* snake =
                snake_bmap_find(world->snakes, client->snake_id);
            if (snake == NULL)
            {
                log_warn("Received commands for unknown snake\n");
                break;
            }

            granularity = settings->sim_tick_rate / settings->net_tick_rate;

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
            cmd_buf_fullness =
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
                return PROCESS_MESSAGE_OK;
            client->last_command_msg_frame = last_frame;

            /*
             * Compare the very last frame received with the current
             * frame number. By tracking the lower and upper boundaries
             * of this difference over time, it can give a good estimate
             * of how "healthy" the client's connection is and whether
             * the command buffer size needs to be increased or
             * decreased.
             */
            cbf_add(client, cmd_buf_fullness);
            lower = cbf_min(client);
            if (cmd_buf_fullness < 0)
            {
                /*
                 * Means we do NOT have the command of the current frame
                 *  -> server is going to make a prediction
                 *  -> will probably lead to a client-side roll back
                 * The client needs to warp forwards in time.
                 */
                int8_t diff = cmd_buf_fullness < -10 ? -10 : cmd_buf_fullness;
                server_queue(client, msg_feedback(diff, frame_number));
            }
            else if (lower - granularity > 0)
            {
                int8_t diff = lower - granularity;
                diff = diff > 10 ? 10 : diff;
                server_queue(client, msg_feedback(diff, frame_number));
            }
            return PROCESS_MESSAGE_OK;
        }

        case MSG_SNAKE_USERNAME: {
            log_warn("Server received unexpected message type %d\n", msg_type);
            break;
        }

        case MSG_SNAKE_USERNAME_ACK: {
            msg_vec_remove_snake_username(
                client->pending_msgs, pp.snake_username_ack.snake_id);
            return PROCESS_MESSAGE_OK;
        }

        case MSG_SNAKE_DESTROY: {
            log_warn("Server received unexpected message type %d\n", msg_type);
            break;
        }

        case MSG_SNAKE_DESTROY_ACK: {
            msg_vec_remove_snake_destroy(
                client->pending_msgs, pp.snake_destroy_ack.snake_id);
            return PROCESS_MESSAGE_OK;
        }

        case MSG_BEZIER:
        case MSG_KNOT: {
            log_warn("Server received unexpected message type %d\n", msg_type);
            break;
        }

        case MSG_KNOT_ACK: {
            struct proximity_state* prox;
            struct snake*           other_snake;
            char*                   ackd;

            other_snake = snake_bmap_find(world->snakes, pp.knot_ack.snake_id);
            if (other_snake == NULL)
            {
                log_warn("Received knot ack for unknown snake\n");
                return PROCESS_MESSAGE_OK;
            }

            prox = proximity_state_bmap_find(
                client->snakes_in_proximity, pp.knot_ack.snake_id);
            if (prox == NULL)
                return PROCESS_MESSAGE_OK;

            ackd = bezier_knot_acks_bmap_find(
                prox->bezier_knot_acks, pp.knot_ack.knot_idx);
            if (ackd == NULL)
                return PROCESS_MESSAGE_OK;
            *ackd = 1;

            return PROCESS_MESSAGE_OK;
        }
    }

    mark_client_as_malicious_and_drop(
        server, addr, client, world, settings->malicious_timeout);
    return PROCESS_MESSAGE_CLIENT_DROPPED;
}

/* ------------------------------------------------------------------------- */
static int unpack_packet(
    struct server*                server,
    const struct settings_server* settings,
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

        switch (process_message(
            server,
            settings,
            client,
            client_addr,
            world,
            type,
            msg,
            msg_len,
            frame_number))
        {
            case PROCESS_MESSAGE_OOM: return -1;
            case PROCESS_MESSAGE_OK: break;
            case PROCESS_MESSAGE_CLIENT_DROPPED: return 0;
        }

        i += msg_len + 2;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
int server_recv(
    struct server*                server,
    const struct settings_server* settings,
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
            server_client_remove(server, world, server_addr, client);
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
        int udp_len;

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
        timeout = net_addr_hm_find(server->malicious_clients, &client_addr);
        if (timeout != NULL)
        {
            *timeout += settings->malicious_timeout * settings->net_tick_rate;
            continue;
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
void* server_run(const void* p)
{
    struct server_instance_bmap* instances;
    const struct settings*       settings = p;

    /* Change log prefix and color for server log messages */
    log_set_prefix(settings->server.log_prefix);
    log_set_colors(COL_B_CYAN, COL_RESET);

    mem_init_threadlocal();
    server_instance_bmap_init(&instances);

    /*
     * Create the default server instance. This is always active, regardless of
     * how many players are connected.
     */
    log_info(
        "Creating default server instance: addr=%s, port=%s\n",
        *settings->server.bind_addr ? settings->server.bind_addr : "*",
        settings->server.bind_port);
    {
        struct server_instance* instance;
        uint16_t                key = atoi(settings->server.bind_port);
        CLITHER_DEBUG_ASSERT(key != 0);

        if (server_instance_bmap_emplace_new(&instances, key, &instance) !=
            BMAP_NEW)
        {
            goto start_default_instance_failed;
        }

        instance->settings_server = &settings->server;
        instance->settings_world = &settings->world;
        instance->addr = settings->server.bind_addr;
        instance->port = settings->server.bind_port;

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
        bmap_for_each (instances, idx, port, instance)
        {
            (void)port;
            thread_join(instance->thread);
        }
        log_info("Joined all server instances\n");
    }

    server_instance_bmap_deinit(instances);
    mem_deinit_threadlocal();
    log_set_colors("", "");
    log_set_prefix("");

    return (void*)0;

start_default_instance_failed:
    server_instance_bmap_deinit(instances);
    mem_deinit_threadlocal();
    log_set_colors("", "");
    log_set_prefix("");
    return (void*)-1;
}
