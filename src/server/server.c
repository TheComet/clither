#include "clither/game/args.h"
#include "clither/game/bezier_knot_rb.h"
#include "clither/game/bezier_point_vec.h"
#include "clither/game/msg_vec.h"
#include "clither/game/qwaabb_rb.h"
#include "clither/game/settings.h"
#include "clither/game/snake.h"
#include "clither/game/snake_bmap.h"
#include "clither/game/world.h"
#include "clither/game/wrap.h"
#include "clither/platform/net.h"
#include "clither/platform/thread.h"
#include "clither/server/bezier_knot_acks_bmap.h"
#include "clither/server/food_in_proximity_hset.h"
#include "clither/server/net_addr_hmap.h"
#include "clither/server/server.h"
#include "clither/server/server_client.h"
#include "clither/server/server_client_hmap.h"
#include "clither/server/server_instance.h"
#include "clither/server/server_instance_bmap.h"
#include "clither/server/snakes_in_proximity_bmap.h"
#include "clither/util/cli_colors.h"
#include "clither/util/log.h"
#include "clither/util/morton.h"
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
    hmap_for_each (server->clients, idx, other_addr, other_client)
    {
        struct bezier_knot_acks_bmap** knot_acks;
        (void)idx, (void)other_addr;

        knot_acks = snakes_in_proximity_bmap_find(
            other_client->snakes_in_proximity, client->snake_id);
        if (knot_acks == NULL)
            continue;
        bezier_knot_acks_bmap_deinit(*knot_acks);
        snakes_in_proximity_bmap_erase(
            other_client->snakes_in_proximity, client->snake_id);

        if (other_client != client)
            server_queue(other_client, msg_snake_destroy(client->snake_id));
    }

    world_remove_snake(world, client->snake_id);
    server_client_deinit(client);
    server_client_hmap_erase(server->clients, addr);
}

/* ------------------------------------------------------------------------- */
static void mark_client_as_malicious_and_drop(
    struct server*         server,
    const struct net_addr* addr,
    struct server_client*  client,
    struct world*          world,
    int                    timeout)
{
    net_addr_hmap_insert_update(&server->malicious_clients, addr, timeout);
    server_client_remove(server, world, addr, client);
}

/* ------------------------------------------------------------------------- */
int server_init(
    struct server* server, const char* bind_address, const char* port)
{
    server->udp_server = net_udp_server.create(bind_address, port);
    if (server->udp_server == NULL)
        goto bind_udp_sock_failed;

#if defined(CLITHER_SERVER_WEBSOCKETS)
    server->ws_server = net_ws_server.create(bind_address, port);
    if (server->ws_server == NULL)
        goto bind_tcp_sock_failed;
#endif

    server_client_hmap_init(&server->clients);
    net_addr_hmap_init(&server->malicious_clients);
    net_addr_hmap_init(&server->banned_clients);

    return 0;

#if defined(CLITHER_SERVER_WEBSOCKETS)
bind_tcp_sock_failed:
#endif
    net_udp_server.destroy(server->udp_server);
bind_udp_sock_failed:
    return -1;
}

/* ------------------------------------------------------------------------- */
void server_deinit(struct server* server)
{
    const struct net_addr* addr;
    struct server_client*  client;
    int                    slot;

#if defined(CLITHER_SERVER_WEBSOCKETS)
    net_ws_server.destroy(server->ws_server);
#endif
    net_udp_server.destroy(server->udp_server);

    net_addr_hmap_deinit(server->banned_clients);
    net_addr_hmap_deinit(server->malicious_clients);

    server_client_hmap_for_each (server->clients, slot, addr, client)
        (void)addr, server_client_deinit(client);
    server_client_hmap_deinit(server->clients);
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
static int append_unreliable_msgs_to_buf(struct msg** pmsg, void* user)
{
    uint8_t            type;
    struct net_packet* pkt = user;
    struct msg*        msg = *pmsg;

    if (pkt->len + msg->payload_len + 2 > (int)sizeof(pkt->data))
        return VEC_RETAIN;
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
int server_send_pending_data(struct server* server, struct world* world)
{
    int                    slot;
    const struct net_addr* addr;
    struct server_client*  client;
    struct net_packet      pkt;
    struct msg**           pmsg;
    uint8_t                type;

    server_client_hmap_for_each (server->clients, slot, addr, client)
    {
        /* Append unreliable messages first */
        while (1)
        {
            pkt.len = 0;
            msg_vec_retain(
                client->pending_msgs, append_unreliable_msgs_to_buf, &pkt);
            if (pkt.len == 0)
                break;

            client->inet->send(client->net, addr, &pkt);
        }

        /* Reliable messages */
        pkt.len = 0;
        vec_for_each (client->pending_msgs, pmsg)
        {
            struct msg* msg = *pmsg;
            if (msg_is_unreliable(msg))
                continue;

            if (pkt.len + msg->payload_len + 2 > (int)sizeof(pkt.data))
            {
                client->inet->send(client->net, addr, &pkt);
                pkt.len = 0;
            }

            if (--msg->resend_period_counter > 0)
                continue;

            msg->resend_period_counter = msg->resend_period;
            if (--msg->resend_retry_counter <= 0)
            {
                log_err(
                    "Client did not acknowledge reliable message: type=%d\n",
                    msg->type);
                server_client_remove(server, world, addr, client);
                break;
            }

            type = (uint8_t)msg->type;
            memcpy(pkt.data + pkt.len + 0, &type, 1);
            memcpy(pkt.data + pkt.len + 1, &msg->payload_len, 1);
            memcpy(pkt.data + pkt.len + 2, msg->payload, msg->payload_len);
            pkt.len += msg->payload_len + 2;
        }
        if (pkt.len > 0)
            client->inet->send(client->net, addr, &pkt);

        client->timeout_counter++;
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
int server_queue_food_data(struct server* server, const struct world* world)
{
    int                    slot;
    const struct net_addr* addr;
    struct server_client*  client;

    server_client_hmap_for_each (server->clients, slot, addr, client)
    {
        const struct snake* snake;
        const struct food*  food;
        struct qwpos        pos;
        struct qwaabb       bb;
        struct qwpos        lower_pos, upper_pos;
        uint64_t            lower_morton, upper_morton, morton;
        int32_t             lower_idx, upper_idx, idx;
        struct qwpos        range;
        (void)addr;

        snake = snake_bmap_find(world->snakes, client->snake_id);
        CLITHER_DEBUG_ASSERT(snake != NULL);
        range = snake_calculate_visible_range(snake);

        lower_pos = make_qwposqw(
            qw_sub(snake->head.pos.x, range.x),
            qw_sub(snake->head.pos.y, range.y));
        upper_pos = make_qwposqw(
            qw_add(snake->head.pos.x, range.x),
            qw_add(snake->head.pos.y, range.y));
        lower_morton = morton_encode_qwpos(lower_pos);
        upper_morton = morton_encode_qwpos(upper_pos);
        lower_idx =
            food_bmap_lower_bound(world->food_grid.morton, lower_morton);
        upper_idx =
            food_bmap_lower_bound(world->food_grid.morton, upper_morton);
        bb = make_qwaabbqw(lower_pos.x, lower_pos.y, upper_pos.x, upper_pos.y);

        /* Add all food pieces within range to the "ack" list */
        bmap_for_each_range (
            world->food_grid.morton, idx, morton, food, lower_idx, upper_idx)
        {
            (void)food;
            pos = morton_decode_qwpos(morton);
            if (!qwaabb_test_qwpos(bb, pos))
                continue;
            switch (food_in_proximity_hset_insert(
                &client->food_in_proximity, morton))
            {
                case HMAP_OOM: return -1;
                case HMAP_NEW:
                    server_queue(
                        client,
                        msg_food_create(
                            morton_decode_qwpos(morton), food->dir));
                case HMAP_EXISTS: break;
            }
        }

        /* Remove food pieces that go out of range, or were removed from the
         * world */
        hset_for_each (client->food_in_proximity, idx, morton)
        {
            pos = morton_decode_qwpos(morton);
            if (!qwaabb_test_qwpos(bb, pos) ||
                food_bmap_find(world->food_grid.morton, morton) == NULL)
            {
                if (food_in_proximity_hset_erase(
                        client->food_in_proximity, morton))
                    server_queue(client, msg_food_destroy(pos));
            }
        }
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
int server_update_snakes_in_range(
    struct server* server, const struct world* world)
{
    int                    slot;
    int                    other_slot;
    const struct net_addr* addr;
    const struct net_addr* other_addr;
    struct server_client*  client;
    struct server_client*  other_client;

    /* TODO: O(n^2) */
    server_client_hmap_for_each (server->clients, slot, addr, client)
    {
        struct qwpos  proximity_range;
        struct snake* snake;

        snake = snake_bmap_find(world->snakes, client->snake_id);
        CLITHER_DEBUG_ASSERT(snake != NULL);
        proximity_range = snake_calculate_visible_range(snake);

        server_client_hmap_for_each (
            server->clients, other_slot, other_addr, other_client)
        {
            struct snake* other_snake;
            struct qwaabb other_aabb;

            /* OK to compare pointers here -- they're from the same hashmap */
            if (addr == other_addr)
                continue;

            other_snake =
                snake_bmap_find(world->snakes, other_client->snake_id);
            CLITHER_DEBUG_ASSERT(other_snake != NULL);

            other_aabb = other_snake->data.bb;
            other_aabb.x1 = qw_sub(other_aabb.x1, proximity_range.x);
            other_aabb.y1 = qw_sub(other_aabb.y1, proximity_range.y);
            other_aabb.x2 = qw_add(other_aabb.x2, proximity_range.x);
            other_aabb.y2 = qw_add(other_aabb.y2, proximity_range.y);

            /* Want to make sure to remove snakes that died, or somehow ended up
             * in the list even though they are held. */
            if (!snake_is_held(other_snake) && !snake_is_dead(other_snake) &&
                qwaabb_test_qwpos(other_aabb, snake->head.pos))
            {
                int32_t                        knot_idx;
                const struct bezier_knot*      knot;
                struct bezier_knot_acks_bmap** knot_acks;
                enum bmap_status               status =
                    snakes_in_proximity_bmap_emplace_or_get(
                        &client->snakes_in_proximity,
                        other_client->snake_id,
                        &knot_acks);
                switch (status)
                {
                    case BMAP_OOM: return -1;
                    case BMAP_NEW:
                        bezier_knot_acks_bmap_init(knot_acks);

                        /* Add all snake bezier knots to the list to send. */
                        rb_for_each (
                            other_snake->data.bezier_knots, knot_idx, knot)
                        {
                            if (bezier_knot_acks_bmap_insert_new(
                                    knot_acks, knot_idx, 0) == BMAP_OOM)
                                return -1;
                        }
                    case BMAP_EXISTS: break;
                }
            }
            else
            {
                struct bezier_knot_acks_bmap** knot_acks =
                    snakes_in_proximity_bmap_find(
                        client->snakes_in_proximity, other_client->snake_id);
                if (knot_acks == NULL)
                    continue;

                server_queue(client, msg_snake_destroy(other_client->snake_id));
                bezier_knot_acks_bmap_deinit(*knot_acks);
                snakes_in_proximity_bmap_erase(
                    client->snakes_in_proximity, other_client->snake_id);
            }
        }
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
static struct server_client*
find_client_for_snake_id(const struct server* server, uint16_t snake_id)
{
    int16_t                slot;
    const struct net_addr* addr;
    struct server_client*  client;

    server_client_hmap_for_each (server->clients, slot, addr, client)
        if (client->snake_id == snake_id)
            return (void)slot, (void)addr, client;

    return NULL;
}
static int
snake_head_collided(struct qwpos victim_head_pos, const struct snake* attacker)
{
    int16_t        bb_idx;
    struct qwaabb* bb;

    if (qwaabb_test_qwpos(attacker->data.bb, victim_head_pos))
        return 1;

    rb_for_each (attacker->data.bezier_aabbs, bb_idx, bb)
        if (qwaabb_test_qwpos(*bb, victim_head_pos))
            return 1;

    return 0;
}
int server_kill_snake_checks(struct server* server, struct world* world)
{
    int                    slot;
    const struct net_addr* addr;
    struct server_client*  victim_client;

    server_client_hmap_for_each (server->clients, slot, addr, victim_client)
    {
        int16_t                        attacker_idx;
        uint16_t                       attacker_snake_id;
        struct bezier_knot_acks_bmap** attacker_knot_acks;
        struct snake*                  victim_snake;

        (void)addr;

        victim_snake = snake_bmap_find(world->snakes, victim_client->snake_id);
        CLITHER_DEBUG_ASSERT(victim_snake != NULL);

        bmap_for_each (
            victim_client->snakes_in_proximity,
            attacker_idx,
            attacker_snake_id,
            attacker_knot_acks)
        {
            const struct snake* attacker_snake =
                snake_bmap_find(world->snakes, attacker_snake_id);
            CLITHER_DEBUG_ASSERT(attacker_snake != NULL);

            if (snake_is_held(victim_snake) || snake_is_dead(victim_snake))
                continue;

            if (snake_head_collided(victim_snake->head.pos, attacker_snake))
                goto kill_snake;
        }
        continue;

    kill_snake:
        world_spawn_food_corpse(
            world, &victim_snake->data, &victim_snake->param);

        snake_set_dead(victim_snake);
        server_queue(victim_client, msg_snake_death());

        bmap_for_each (
            victim_client->snakes_in_proximity,
            attacker_idx,
            attacker_snake_id,
            attacker_knot_acks)
        {
            struct server_client* attacker_client =
                find_client_for_snake_id(server, attacker_snake_id);
            server_queue(
                attacker_client, msg_snake_destroy(victim_client->snake_id));
            (void)attacker_knot_acks;
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
    server_client_hmap_for_each (server->clients, slot, addr, client)
    {
        struct snake* snake = snake_bmap_find(world->snakes, client->snake_id);
        CLITHER_DEBUG_ASSERT(snake != NULL), (void)addr;
        if (snake_is_held(snake) || snake_is_dead(snake))
            continue;
        server_queue(
            client,
            msg_snake_head(
                frame_number,
                snake->head.pos,
                snake->head.angle,
                snake->head.speed,
                snake->param.food_eaten));
    }

    /* Queue bezier knots of all snakes in proximity */
    server_client_hmap_for_each (server->clients, slot, addr, client)
    {
        int16_t                        prox_idx;
        uint16_t                       snake_id;
        struct bezier_knot_acks_bmap** knot_acks;
        bmap_for_each (
            client->snakes_in_proximity, prox_idx, snake_id, knot_acks)
        {
            int16_t             knot_idx;
            struct bezier_knot* knot;
            struct snake* snake = snake_bmap_find(world->snakes, snake_id);
            CLITHER_DEBUG_ASSERT(snake != NULL);
            rb_for_each (snake->data.bezier_knots, knot_idx, knot)
            {
                char* ackd;
                switch (bezier_knot_acks_bmap_emplace_or_get(
                    knot_acks, knot_idx, &ackd))
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
                *knot_acks, snake->data.bezier_knots);

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

            server_queue(
                client, msg_snake_param(snake_id, snake->param.food_eaten));
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
    struct server*                     server,
    const struct settings_server*      settings_server,
    const struct net_server_interface* inet,
    struct net_server*                 net,
    struct world*                      world,
    const struct settings_world*       settings_world,
    struct server_client*              client,
    const struct net_addr*             addr,
    enum msg_type                      msg_type,
    const uint8_t*                     msg_data,
    uint8_t                            msg_len,
    uint16_t                           frame_number)
{
    /*
     * NOTE: Beyond this point, "client" won't be NULL *unless* the
     * message is MSG_JOIN_REQUEST. This makes the switch/case handling
     * a little easier for all other cases
     */

    union parsed_payload pp;
    switch (msg_parse_payload(&pp, msg_type, msg_data, msg_len))
    {
        /* XXX: This being here is really ugly because we have to pass down
         * "inet" and "net", and we have the edge case that client is NULL. This
         * logic should be moved further up. */
        case MSG_JOIN_REQUEST: {
            if (hmap_count(server->clients) + 1 > settings_server->max_players)
            {
                struct net_packet response;
                struct msg* msg = msg_join_deny_server_full("Server full");
                response.len = msg->payload_len + 2;
                response.data[0] = msg->type;
                response.data[1] = msg->payload_len;
                memcpy(response.data + 2, msg->payload, msg->payload_len);
                inet->send(net, addr, &response);
                msg_free(msg);
                return PROCESS_MESSAGE_OK;
            }

            if (pp.join_request.username_len >
                settings_server->max_username_len)
            {
                struct net_packet response;
                struct msg*       msg =
                    msg_join_deny_bad_username("Username too long");
                response.len = msg->payload_len + 2;
                response.data[0] = msg->type;
                response.data[1] = msg->payload_len;
                memcpy(response.data + 2, msg->payload, msg->payload_len);
                inet->send(net, addr, &response);
                msg_free(msg);
                return PROCESS_MESSAGE_OK;
            }

            /* Create new client */
            if (client == NULL)
            {
                struct snake* snake;
                uint16_t      snake_id;
                log_net("MSG_JOIN_REQUEST \"%s\"\n", pp.join_request.username);

                client = server_client_hmap_emplace_new(&server->clients, addr);
                if (client == NULL)
                    return PROCESS_MESSAGE_OOM;

                snake_id = world_spawn_snake(world, pp.join_request.username);
                if (snake_id == 0)
                {
                    server_client_hmap_erase(server->clients, addr);
                    return PROCESS_MESSAGE_OOM;
                }

                /* Hold the snake in place until we receive the first
                 * command */
                snake = snake_bmap_find(world->snakes, snake_id);
                CLITHER_DEBUG_ASSERT(snake != NULL);
                snake_set_hold(snake);

                server_client_init(
                    client,
                    inet,
                    net,
                    snake_id,
                    frame_number,
                    settings_server->sim_tick_rate,
                    settings_server->net_tick_rate);
            }

            /* (Re-)send join accept response */
            {
                struct snake* snake;
                struct msg*   response;
                snake = snake_bmap_find(world->snakes, client->snake_id);
                CLITHER_DEBUG_ASSERT(snake != NULL);
                response = msg_join_accept(
                    pp.join_request.frame,
                    frame_number,
                    settings_server->sim_tick_rate,
                    settings_server->net_tick_rate,
                    settings_world->inner_radius,
                    settings_world->ring_start,
                    settings_world->ring_end,
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
            return PROCESS_MESSAGE_CLIENT_DROPPED;
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

            granularity =
                settings_server->sim_tick_rate / settings_server->net_tick_rate;

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
            log_warn(
                "Server received unexpected message type MSG_SNAKE_USERNAME\n");
            break;
        }

        case MSG_SNAKE_USERNAME_ACK: {
            msg_vec_remove_snake_username(
                client->pending_msgs, pp.snake_username_ack.snake_id);
            return PROCESS_MESSAGE_OK;
        }

        case MSG_SNAKE_DESTROY: {
            log_warn(
                "Server received unexpected message type MSG_SNAKE_DESTROY\n");
            break;
        }

        case MSG_SNAKE_DESTROY_ACK: {
            msg_vec_remove_snake_destroy(
                client->pending_msgs, pp.snake_destroy_ack.snake_id);
            return PROCESS_MESSAGE_OK;
        }

        case MSG_SNAKE_DEATH: {
            log_warn(
                "Server received unexpected message type MSG_SNAKE_DEATH\n");
            break;
        }

        case MSG_SNAKE_DEATH_ACK: {
            msg_vec_remove_type(client->pending_msgs, MSG_SNAKE_DEATH);
            return PROCESS_MESSAGE_OK;
        }

        case MSG_BEZIER:
        case MSG_KNOT: {
            log_warn(
                "Server received unexpected message type %s\n",
                msg_type == MSG_BEZIER ? "MSG_BEZIER" : "MSG_KNOT");
            break;
        }

        case MSG_KNOT_ACK: {
            struct bezier_knot_acks_bmap** knot_acks;
            struct snake*                  other_snake;
            char*                          ackd;

            other_snake = snake_bmap_find(world->snakes, pp.knot_ack.snake_id);
            if (other_snake == NULL)
            {
                log_warn("Received knot ack for unknown snake\n");
                return PROCESS_MESSAGE_OK;
            }

            knot_acks = snakes_in_proximity_bmap_find(
                client->snakes_in_proximity, pp.knot_ack.snake_id);
            if (knot_acks == NULL)
                return PROCESS_MESSAGE_OK;

            ackd = bezier_knot_acks_bmap_find(*knot_acks, pp.knot_ack.knot_idx);
            if (ackd == NULL)
                return PROCESS_MESSAGE_OK;
            *ackd = 1;

            return PROCESS_MESSAGE_OK;
        }

        case MSG_FOOD_CREATE: {
            log_warn(
                "Server received unexpected message type MSG_FOOD_CREATE\n");
            break;
        }

        case MSG_FOOD_CREATE_ACK: {
            uint64_t morton = morton_encode_qwpos(pp.food_create_ack.pos);
            msg_vec_remove_food_create(
                client->pending_msgs, pp.food_create_ack.pos);
            /* Ensure the food exists in the ack'd list, just in case
             * CREATE/DESTROY messages arrived out of order */
            switch (food_in_proximity_hset_insert(
                &client->food_in_proximity, morton))
            {
                case HSET_OOM: return PROCESS_MESSAGE_OOM;
                case HSET_EXISTS:
                case HSET_NEW: break;
            }
            return PROCESS_MESSAGE_OK;
        }

        case MSG_FOOD_DESTROY: {
            log_warn(
                "Server received unexpected message type MSG_FOOD_DESTROY\n");
            break;
        }

        case MSG_FOOD_DESTROY_ACK: {
            msg_vec_remove_food_destroy(
                client->pending_msgs, pp.food_destroy_ack.pos);
            return PROCESS_MESSAGE_OK;
        }
    }

    mark_client_as_malicious_and_drop(
        server, addr, client, world, settings_server->malicious_timeout);
    return PROCESS_MESSAGE_CLIENT_DROPPED;
}

/* ------------------------------------------------------------------------- */
static int unpack_packet(
    struct server*                     server,
    const struct settings_server*      settings_server,
    const struct net_server_interface* inet,
    struct net_server*                 net,
    struct world*                      world,
    const struct settings_world*       settings_world,
    struct server_client*              client,
    const struct net_addr*             client_addr,
    const struct net_packet*           packet,
    uint16_t                           frame_number)
{
    /*
     * Packet can contain multiple message objects.
     * buf[0] == message type
     * buf[1] == message payload length
     * buf[2] == beginning of message payload
     */
    int i;
    for (i = 0; i < packet->len - 1;)
    {
        enum msg_type  type = packet->data[i + 0];
        const uint8_t  msg_len = packet->data[i + 1];
        const uint8_t* msg = &packet->data[i + 2];

        log_net("Unpacking msg type=%d, len=%d\n", type, msg_len);

        if (i + 2 + msg_len > packet->len)
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
                settings_server->malicious_timeout);
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
            settings_server,
            inet,
            net,
            world,
            settings_world,
            client,
            client_addr,
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
#include <stdio.h>
int server_recv(
    struct server*                server,
    const struct settings_server* settings_server,
    struct world*                 world,
    const struct settings_world*  settings_world,
    uint16_t                      frame_number)
{
    struct net_packet      packet;
    const struct net_addr* server_addr;
    struct net_addr        client_addr;
    struct server_client*  client;
    int                    slot;
    int*                   timeout;

    log_net("server_recv() frame=%d\n", frame_number);

    /* Update timeout counters of every client that we've communicated with */
    server_client_hmap_for_each (server->clients, slot, server_addr, client)
    {
        client->timeout_counter++;

        if (client->timeout_counter >
            settings_server->client_timeout * settings_server->net_tick_rate)
        {
            struct net_addr_str ipstr;
            net_addr_to_str(&ipstr, server_addr);
            log_warn("Client %s timed out\n", ipstr.cstr);
            server_client_remove(server, world, server_addr, client);
        }
    }

    /* Update malicious client timeouts */
    net_addr_hmap_for_each (
        server->malicious_clients, slot, server_addr, timeout)
    {
        struct net_addr_str ipstr;

        if (--(*timeout) > 0)
            continue;

        net_addr_to_str(&ipstr, server_addr);
        log_info("Client %s removed from malicious list\n", ipstr.cstr);
        net_addr_hmap_erase(server->malicious_clients, server_addr);
    }

    /* We may need to read more than one packet */
    while (1)
    {
        switch (
            net_udp_server.receive(server->udp_server, &client_addr, &packet))
        {
            case NET_RECEIVE_ERROR: return -1;
            case NET_RECEIVE_NO_DATA: goto recv_udp_done;
            case NET_RECEIVE_DATA: break;
        }

        /*
         * If we received a packet from a banned client, ignore packet
         */
        if (net_addr_hmap_find(server->banned_clients, &client_addr))
            continue;

        /*
         * If we received a packet from a potentially malicious client,
         * increase their timeout
         */
        timeout = net_addr_hmap_find(server->malicious_clients, &client_addr);
        if (timeout != NULL)
        {
            *timeout += settings_server->malicious_timeout *
                        settings_server->net_tick_rate;
            continue;
        }

        /*
         * If we received a packet from a registered client, reset their timeout
         * counter
         */
        client = server_client_hmap_find(server->clients, &client_addr);
        if (client != NULL)
            client->timeout_counter = 0;

        if (unpack_packet(
                server,
                settings_server,
                &net_udp_server,
                server->udp_server,
                world,
                settings_world,
                client,
                &client_addr,
                &packet,
                frame_number) != 0)
        {
            return -1;
        }
    }
recv_udp_done:;

    return 0;
}

/* ------------------------------------------------------------------------- */
void* server_run(const void* p)
{
    struct server_instance_bmap* instances;
    const struct settings*       settings = p;

    mem_init_threadlocal();
    log_init();

    /* Change log prefix and color for server log messages */
    log_set_prefix(settings->server.log_prefix);
    log_set_colors(COL_B_CYAN, COL_RESET);

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

    return (void*)0;

start_default_instance_failed:
    server_instance_bmap_deinit(instances);
    mem_deinit_threadlocal();
    return (void*)-1;
}
