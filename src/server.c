#include "clither/bezier.h"
#include "clither/log.h"
#include "clither/msg_queue.h"
#include "clither/net.h"
#include "clither/server.h"
#include "clither/server_settings.h"
#include "clither/snake.h"
#include "clither/world.h"
#include "clither/wrap.h"

#include "cstructures/hashmap.h"
#include "cstructures/rb.h"
#include "cstructures/vector.h"

#include <string.h>  /* memcpy */

#define CBF_WINDOW_SIZE 20

struct server_client
{
    struct cs_vector pending_msgs;         /* struct msg* */
    struct cs_hashmap bezier_handles_ack;  /* struct bezier_handle */
    int timeout_counter;
    int cbf_window[CBF_WINDOW_SIZE];       /* "Command Buffer Fullness" window */
    cs_btree_key snake_id;
    uint16_t last_command_msg_frame;
};

#define SERVER_CLIENT_FOR_EACH(table, k, v) \
    HASHMAP_FOR_EACH(table, void, struct server_client, k, v)
#define SERVER_CLIENT_END_EACH \
    HASHMAP_END_EACH

#define MALICIOUS_CLIENT_FOR_EACH(table, k, v) \
    HASHMAP_FOR_EACH(table, void, int, k, v)
#define MALICIOUS_CLIENT_END_EACH \
    HASHMAP_END_EACH

/* ------------------------------------------------------------------------- */
static void
client_remove(struct server* server, struct world* world, void* addr, struct server_client* client)
{
    world_remove_snake(world, client->snake_id);
    msg_queue_deinit(&client->pending_msgs);
    hashmap_erase(&server->client_table, addr);
}

/* ------------------------------------------------------------------------- */
static void
mark_client_as_malicious_and_drop(
    struct server* server,
    struct world* world,
    void* addr,
    struct server_client* client,
    int timeout)
{
    hashmap_insert(&server->malicious_clients, addr, &timeout);
    client_remove(server, world, addr, client);
}

/* ------------------------------------------------------------------------- */
int
server_init(
    struct server* server,
    const char* bind_address,
    const char* port)
{
    int addrlen;

    server->udp_sock = net_bind(bind_address, port, &addrlen);
    if (server->udp_sock < 0)
        return -1;

    /*
     * Whenever we receive a UDP packet, we look up the source address to get
     * the client structure associated with that packet. Depending on whether
     * we are using IPv4 or IPv6 the size of the key will be different.
     */
    hashmap_init(
        &server->client_table,
        (uint32_t)addrlen,
        sizeof(struct server_client));
    hashmap_init(
        &server->malicious_clients,
        (uint32_t)addrlen,
        sizeof(int));
    hashmap_init(
        &server->banned_clients,
        (uint32_t)addrlen,
        0);

    return 0;
}

/* ------------------------------------------------------------------------- */
void
server_deinit(struct server* server)
{
    net_close(server->udp_sock);

    hashmap_deinit(&server->banned_clients);
    hashmap_deinit(&server->malicious_clients);

    SERVER_CLIENT_FOR_EACH(&server->client_table, addr, client)
        msg_queue_deinit(&client->pending_msgs);
    SERVER_CLIENT_END_EACH
    hashmap_deinit(&server->client_table);
}

/* ------------------------------------------------------------------------- */
static void
cbf_add(struct server_client* client, int value)
{
    memmove(
        &client->cbf_window[1],
        &client->cbf_window[0],
        sizeof(int) * (CBF_WINDOW_SIZE - 1));
    client->cbf_window[0] = value;
}

/* ------------------------------------------------------------------------- */
static int
cbf_lower_bound(struct server_client* client)
{
    int i;
    int lower = INT32_MAX;
    for (i = 0; i != CBF_WINDOW_SIZE; ++i)
        if (lower > client->cbf_window[i])
            lower = client->cbf_window[i];
    return lower;
}

/* ------------------------------------------------------------------------- */
void
server_send_pending_data(struct server* server)
{
    char buf[MAX_UDP_PACKET_SIZE];

    SERVER_CLIENT_FOR_EACH(&server->client_table, addr, client)
        uint8_t type;
        int len = 0;

        /* Append unreliable messages first */
        MSG_FOR_EACH(&client->pending_msgs, msg)
            if (len + msg->payload_len + 2 > MAX_UDP_PACKET_SIZE)
                continue;
            if (msg->resend_rate != 0)  /* message is reliable */
                continue;

            log_net("Packing msg type=%d, len=%d, resend=%d\n", msg->type, msg->payload_len, msg->resend_rate);

            type = (uint8_t)msg->type;
            memcpy(buf + len + 0, &type, 1);
            memcpy(buf + len + 1, &msg->payload_len, 1);
            memcpy(buf + len + 2, msg->payload, msg->payload_len);

            len += msg->payload_len + 2;
            msg_free(msg);
            MSG_ERASE_IN_FOR_LOOP(&client->pending_msgs, msg);
        MSG_END_EACH

        /* Append reliable messages */
        MSG_FOR_EACH(&client->pending_msgs, msg)
            if (len + msg->payload_len + 2 > MAX_UDP_PACKET_SIZE)
                continue;
            if (msg->resend_rate == 0)  /* message is unreliable */
                continue;

            if (--msg->resend_rate_counter > 0)
                continue;
            msg->resend_rate_counter = msg->resend_rate;

            log_net("Packing msg type=%d, len=%d, resend=%d\n", msg->type, msg->payload_len, msg->resend_rate);

            type = (uint8_t)msg->type;
            memcpy(buf + len + 0, &type, 1);
            memcpy(buf + len + 1, &msg->payload_len, 1);
            memcpy(buf + len + 2, msg->payload, msg->payload_len);

            len += msg->payload_len + 2;
        MSG_END_EACH

        if (len > 0)
        {
            /* NOTE: The hashmap's key size contains the length of the stored address */
            log_net("Sending UDP packet, size=%d\n", len);
            net_sendto(server->udp_sock, buf, len, addr, server->client_table.key_size);
            client->timeout_counter++;
        }
    SERVER_CLIENT_END_EACH
}

/* ------------------------------------------------------------------------- */
static void
server_queue(struct server_client* client, struct msg* msg)
{
    vector_push(&client->pending_msgs, &msg);
}

/* ------------------------------------------------------------------------- */
void
server_queue_snake_data(
    struct server* server,
    const struct world* world,
    uint16_t frame_number)
{
    SERVER_CLIENT_FOR_EACH(&server->client_table, addr, client)
        struct snake* snake = world_get_snake(world, client->snake_id);
        server_queue(client, msg_snake_head(&snake->head, frame_number));
    SERVER_CLIENT_END_EACH

    SERVER_CLIENT_FOR_EACH(&server->client_table, addr, client)
        SERVER_CLIENT_FOR_EACH(&server->client_table, other_addr, other_client)
            struct snake* snake;
            struct snake* other_snake;

            /* OK to compare pointers here -- they're from the same hashmap */
            if (addr == other_addr)
                continue;

            snake = world_get_snake(world, client->snake_id);
            other_snake = world_get_snake(world, other_client->snake_id);

            /* TODO better distance check using bezier AABBs */
            qw dx = qw_sub(snake->head.pos.x, other_snake->head.pos.x);
            qw dy = qw_sub(snake->head.pos.y, other_snake->head.pos.y);
            qw dist_sq = qw_add(qw_mul(dx, dx), qw_mul(dy, dy));
            if (dist_sq > make_qw(1 * 1))
                continue;

            /* TODO queue bezier handles */

        SERVER_CLIENT_END_EACH
    SERVER_CLIENT_END_EACH
}

/* ------------------------------------------------------------------------- */
int
server_recv(
    struct server* server,
    const struct server_settings* settings,
    struct world* world,
    uint16_t frame_number)
{
    uint8_t buf[MAX_UDP_PACKET_SIZE];
    uint8_t client_addr[MAX_ADDRLEN];
    int bytes_received;
    int i;

    log_net("server_recv() frame=%d\n", frame_number);

    /* Update timeout counters of every client that we've communicated with */
    SERVER_CLIENT_FOR_EACH(&server->client_table, addr, client)
        client->timeout_counter++;

        if (client->timeout_counter > settings->client_timeout * settings->net_tick_rate)
        {
            char ipstr[MAX_ADDRSTRLEN];
            net_addr_to_str(ipstr, MAX_ADDRSTRLEN, addr);
            log_warn("Client %s timed out\n", ipstr);
            client_remove(server, world, addr, client);
        }
    SERVER_CLIENT_END_EACH

    /* Update malicious client timeouts */
    MALICIOUS_CLIENT_FOR_EACH(&server->malicious_clients, addr, timeout)
        if (--(*timeout) <= 0)
        {
            char ipstr[MAX_ADDRSTRLEN];
            net_addr_to_str(ipstr, MAX_ADDRSTRLEN, addr);
            log_info("Client %s removed from malicious list\n", ipstr);
            hashmap_erase(&server->malicious_clients, addr);
        }
    MALICIOUS_CLIENT_END_EACH

    /* We may need to read more than one UDP packet */
    while (1)
    {
        struct server_client* client;
        bytes_received = net_recvfrom(server->udp_sock, buf, MAX_UDP_PACKET_SIZE,
            client_addr, server->client_table.key_size);

        /* Nothing received or error */
        if (bytes_received <= 0)
            return bytes_received;
        log_net("Received UDP packet, size=%d\n", bytes_received);

        /*
         * If we received a packet from a banned client, ignore packet
         */
        if (hashmap_find(&server->banned_clients, client_addr) != 0)
            continue;

        /*
         * If we received a packet from a potentially malicious client,
         * increase their timeout
         */
        {
            int* timeout = hashmap_find(&server->malicious_clients, client_addr);
            if (timeout != NULL)
            {
                *timeout += settings->malicious_timeout * settings->net_tick_rate;
                continue;
            }
        }

        /*
         * If we received a packet from a registered client, reset their timeout
         * counter
         */
        client = hashmap_find(&server->client_table, client_addr);
        if (client != NULL)
            client->timeout_counter = 0;

        /*
         * Packet can contain multiple message objects.
         * buf[0] == message type
         * buf[1] == message payload length
         * buf[2] == beginning of message payload
         */
        for (i = 0; i < bytes_received - 1;)
        {
            const uint8_t* payload;
            union parsed_payload pp;
            enum msg_type type = buf[i + 0];
            uint8_t payload_len = buf[i + 1];
            if (i + 2 + payload_len > bytes_received)
            {
                char ipstr[MAX_ADDRSTRLEN];
                net_addr_to_str(ipstr, MAX_ADDRSTRLEN, client_addr);
                log_warn("Invalid payload length \"%d\" received from client %s\n",
                    (int)payload_len, ipstr);
                log_warn("Dropping rest of packet\n");
                mark_client_as_malicious_and_drop(server, world, client_addr, client, settings->malicious_timeout);
                break;
            }

            log_net("Unpacking msg type=%d, len=%d\n", type, payload_len);

            /*
             * Disallow receiving packets from clients that are not registered
             * with the exception of the "join game request" message.
             */
            if (client == NULL && type != MSG_JOIN_REQUEST)
            {
                char ipstr[MAX_ADDRSTRLEN];
                net_addr_to_str(ipstr, MAX_ADDRSTRLEN, client_addr);
                log_warn("Received packet from unknown client %s, ignoring\n", ipstr);
                break;
            }

            /*
             * NOTE: Beyond this point, "client" won't be NULL *unless* the message is
             * MSG_JOIN_REQUEST. This makes the switch/case handling a little easier for
             * all other cases
             */

            /* Process message */
            payload = &buf[i + 2];
            switch (msg_parse_payload(&pp, type, payload, payload_len))
            {
                default: {
                    mark_client_as_malicious_and_drop(server, world, client_addr, client, settings->malicious_timeout);
                } break;

                case MSG_JOIN_REQUEST: {
                    if (hashmap_count(&server->client_table) > (uint32_t)settings->max_players)
                    {
                        char response[2] = { MSG_JOIN_DENY_SERVER_FULL, 0 };
                        net_sendto(server->udp_sock, response, 2, client_addr, server->client_table.key_size);
                        break;
                    }

                    if (pp.join_request.username_len > settings->max_username_len)
                    {
                        char response[2] = { MSG_JOIN_DENY_BAD_USERNAME, 0 };
                        net_sendto(server->udp_sock, response, 2, client_addr, server->client_table.key_size);
                        break;
                    }

                    /* Create new client. This code is not refactored into a
                     * separate function because this is the only location where
                     * clients are created. Clients are destroyed by the
                     * function remove_client() */
                    if (client == NULL)
                    {
                        struct snake* snake;
                        int n;
                        log_net("MSG_JOIN_REQUEST \"%s\"\n", pp.join_request.username);

                        client = hashmap_emplace(&server->client_table, &client_addr);
                        msg_queue_init(&client->pending_msgs);
                        //hashmap_init(&client->bezier_handles_ack, sizeof(struct bezier_handle), 0);
                        client->timeout_counter = 0;
                        client->snake_id = world_spawn_snake(world, pp.join_request.username);
                        client->last_command_msg_frame = frame_number;

                        /* Hold the snake in place until we receive the first command */
                        snake = world_get_snake(world, client->snake_id);
                        snake_set_hold(snake);

                        /*
                         * Init "Command Buffer Fullness" queue with minimum granularity.
                         * This assumes the client has the most stable connection initially.
                         */
                        for (n = 0; n != CBF_WINDOW_SIZE; ++n)
                            client->cbf_window[n] = settings->sim_tick_rate / settings->net_tick_rate;
                    }

                    /* (Re-)send join accept response */
                    {
                        struct snake* snake = world_get_snake(world, client->snake_id);
                        struct msg* response = msg_join_accept(
                            settings->sim_tick_rate,
                            settings->net_tick_rate,
                            pp.join_request.frame,
                            frame_number,
                            client->snake_id,
                            &snake->head.pos);
                        vector_push(&client->pending_msgs, &response);
                    }
                } break;

                case MSG_JOIN_ACCEPT:
                case MSG_JOIN_DENY_BAD_PROTOCOL:
                case MSG_JOIN_DENY_BAD_USERNAME:
                case MSG_JOIN_DENY_SERVER_FULL:
                    break;

                case MSG_LEAVE: {

                } break;

                case MSG_COMMANDS: {
                    uint16_t first_frame, last_frame;
                    int lower;
                    struct snake* snake = world_get_snake(world, client->snake_id);
                    int granularity = settings->sim_tick_rate / settings->net_tick_rate;

                    /*
                     * Measure how many frames are in the client's command buffer.
                     *
                     * A negative value indicates the client is lagging behind, and
                     * the server will have to make a prediction, which will lead to
                     * the client having to re-simulate. A positive value indicates
                     * there are commands in the buffer. Depending on how stable the
                     * connection is, the client will be instructed to shrink the
                     * buffer.
                     */
                    int client_commands_queued =
                        u16_sub_wrap(command_rb_frame_end(&snake->command_rb), frame_number);

                    /* Returns the first and last frame numbers that were unpacked from the message */
                    if (msg_commands_unpack_into(&snake->command_rb, payload, payload_len, frame_number, &first_frame, &last_frame) < 0)
                        break;  /* something is wrong with the message */

                    /*
                     * This handles packets being reordered by dropping any
                     * commands older than the last command received
                     */
                    if (u16_le_wrap(last_frame, client->last_command_msg_frame))
                        break;
                    client->last_command_msg_frame = last_frame;

                    /*
                     * Compare the very last frame received with the current frame
                     * number. By tracking the lower and upper boundaries of this
                     * difference over time, it can give a good estimate of how
                     * "healthy" the client's connection is and whether the command
                     * buffer size needs to be increased or decreased.
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
                        int8_t diff = client_commands_queued < -10 ? -10 : client_commands_queued;
                        server_queue(client, msg_feedback(diff, frame_number));
                    }
                    else if (lower - granularity*2 > 0)
                    {
                        int8_t diff = lower - granularity*2;
                        diff = diff > 10 ? 10 : diff;
                        server_queue(client, msg_feedback(diff, frame_number));
                    }
                } break;

                case MSG_SNAKE_CREATE: {

                } break;

                case MSG_SNAKE_CREATE_ACK: {

                } break;
            }

            i += payload_len + 2;
        }
    }

    return 0;
}

/* ------------------------------------------------------------------------- */
void*
server_run(const void* args)
{
#define INSTANCE_FOR_EACH(btree, port, instance) \
    BTREE_FOR_EACH(btree, struct server_instance, port, instance)
#define INSTANCE_END_EACH \
    BTREE_END_EACH

    struct cs_btree instances;
    struct server_settings settings;
    const struct args* a = args;

    /* Change log prefix and color for server log messages */
    log_set_prefix("Server: ");
    log_set_colors(COL_B_CYAN, COL_RESET);

    cs_threadlocal_init();

    btree_init(&instances, sizeof(struct server_instance));

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
        const char* port = *a->port ? a->port : settings.port;
        cs_btree_key key = atoi(port);
        assert(key != 0);

        instance = btree_emplace_new(&instances, key);
        assert(instance != NULL);
        instance->settings = &settings;
        instance->ip = a->ip;
        strcpy(instance->port, port);

        log_dbg("Starting default server instance\n");
        if (thread_start(&instance->thread, run_server_instance, instance) < 0)
        {
            log_err("Failed to start the default server instance! Can't continue\n");
            goto start_default_instance_failed;
        }
    }

    /* For now we don't create more instances once the server fills up */
    INSTANCE_FOR_EACH(&instances, port, instance)
        thread_join(instance->thread, 0);
    INSTANCE_END_EACH
    log_dbg("Joined all server instances\n");

    server_settings_save(&settings, a->config_file);

    btree_deinit(&instances);
    cs_threadlocal_deinit();
    log_set_colors("", "");
    log_set_prefix("");

    return (void*)0;

start_default_instance_failed:
load_settings_failed:
    btree_deinit(&instances);
    cs_threadlocal_deinit();
    log_set_colors("", "");
    log_set_prefix("");
    return (void*)-1;
}
