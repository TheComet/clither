#include "clither/log.h"
#include "clither/msg_queue.h"
#include "clither/net.h"
#include "clither/server.h"
#include "clither/server_settings.h"

#include "cstructures/vector.h"

#include <string.h>  /* memcpy */

struct client_table_entry
{
    struct cs_vector pending_unreliable;  /* struct msg* */
    struct cs_vector pending_reliable;    /* struct msg* */
    int timeout_counter;
};

#define CLIENT_TABLE_FOR_EACH(table, k, v) \
    HASHMAP_FOR_EACH(table, void, struct client_table_entry, k, v)
#define CLIENT_TABLE_END_EACH \
    HASHMAP_END_EACH

#define MALICIOUS_CLIENT_FOR_EACH(table, k, v) \
    HASHMAP_FOR_EACH(table, void, int, k, v)
#define MALICIOUS_CLIENT_END_EACH \
    HASHMAP_END_EACH

/* ------------------------------------------------------------------------- */
static void
mark_client_as_malicious_and_drop(struct server* server)
{

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
        sizeof(struct client_table_entry));
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

    CLIENT_TABLE_FOR_EACH(&server->client_table, addr, client)
        msg_queue_deinit(&client->pending_reliable);
        msg_queue_deinit(&client->pending_unreliable);
    CLIENT_TABLE_END_EACH
    hashmap_deinit(&server->client_table);
}

/* ------------------------------------------------------------------------- */
void
server_send_pending_data(struct server* server, const struct server_settings* settings)
{
    char buf[MAX_UDP_PACKET_SIZE];

    CLIENT_TABLE_FOR_EACH(&server->client_table, addr, client)
        uint8_t type;
        int len = 0;

        /* Append unreliable messages first */
        MSG_FOR_EACH(&client->pending_unreliable, msg)
            if (len + msg->payload_len + 2 > MAX_UDP_PACKET_SIZE)
                continue;

            type = (uint8_t)msg->type;
            memcpy(buf + 0, &type, 1);
            memcpy(buf + 1, &msg->payload_len, 1);
            memcpy(buf + 2, msg->payload, msg->payload_len);

            len += msg->payload_len + 2;
            msg_free(msg);
            MSG_ERASE_IN_FOR_LOOP(&client->pending_unreliable, msg);
        MSG_END_EACH

        /* Append reliable messages */
        MSG_FOR_EACH(&client->pending_reliable, msg)
            if (len + msg->payload_len + 2 > MAX_UDP_PACKET_SIZE)
                continue;

            if (msg->resend_rate_counter-- > 0)
                continue;
            msg->resend_rate_counter = msg->resend_rate;

            type = (uint8_t)msg->type;
            memcpy(buf + 0, &type, 1);
            memcpy(buf + 1, &msg->payload_len, 1);
            memcpy(buf + 2, msg->payload, msg->payload_len);

            len += msg->payload_len + 2;
        MSG_END_EACH

        if (len > 0)
        {
            /* NOTE: The hashmap's key size contains the length of the stored address */
            net_sendto(server->udp_sock, buf, len, addr, server->client_table.key_size);
            client->timeout_counter++;
        }

        if (client->timeout_counter > settings->client_timeout * settings->net_tick_rate)
        {
            char ipstr[MAX_ADDRSTRLEN];
            net_addr_to_str(ipstr, MAX_ADDRSTRLEN, addr);
            log_warn("Client %s timed out\n", ipstr);
            msg_queue_deinit(&client->pending_reliable);
            msg_queue_deinit(&client->pending_unreliable);
            hashmap_erase(&server->client_table, addr);
        }
    CLIENT_TABLE_END_EACH
}

/* ------------------------------------------------------------------------- */
int
server_recv(struct server* server, const struct server_settings* settings, uint16_t frame_number)
{
    char buf[MAX_UDP_PACKET_SIZE];
    char client_addr[MAX_ADDRLEN];
    int bytes_received;
    int i;

    /* Update timeout counters of every client that we've communicated with */
    CLIENT_TABLE_FOR_EACH(&server->client_table, addr, client)
        client->timeout_counter++;
    CLIENT_TABLE_END_EACH

    /* Update malicious client timeouts */
    MALICIOUS_CLIENT_FOR_EACH(&server->malicious_clients, addr, timeout)
        if (--(*timeout) <= 0)
        {
            char ipstr[MAX_ADDRSTRLEN];
            net_addr_to_str(ipstr, MAX_ADDRSTRLEN, addr);
            log_dbg("Client %s removed from malicious list\n", ipstr);
            hashmap_erase(&server->malicious_clients, addr);
        }
    MALICIOUS_CLIENT_END_EACH

    /* We may need to read more than one UDP packet */
    while (1)
    {
        struct client_table_entry* client;
        bytes_received = net_recvfrom(server->udp_sock, buf, MAX_UDP_PACKET_SIZE,
            client_addr, server->client_table.key_size);

        /* Nothing received or error */
        if (bytes_received <= 0)
            return bytes_received;

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
         * buf[1] == payload length
         * buf[2] == beginning of message payload
         */
        for (i = 0; i < bytes_received - 1;)
        {
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
                mark_client_as_malicious_and_drop(server);
                break;
            }

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
            switch (msg_parse_paylaod(&pp, type, payload_len, &buf[i + 2]))
            {
                default: {
                    mark_client_as_malicious_and_drop(server);
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

                    if (client == NULL)
                    {
                        /* TODO: Create snake in world */

                        log_dbg("Protocol: MSG_JOIN <- \"%s\"\n", pp.join_request.username);

                        client = hashmap_emplace(&server->client_table, &client_addr);
                        msg_queue_init(&client->pending_reliable);
                        msg_queue_init(&client->pending_unreliable);
                        client->timeout_counter = 0;
                    }

                    /* (Re-)send join accept response */
                    {
                        struct qpos2 spawn_pos = { 32, 32 };
                        struct msg* response = msg_join_accept(
                            settings->sim_tick_rate,
                            settings->net_tick_rate,
                            pp.join_request.frame,
                            frame_number,
                            &spawn_pos);
                        vector_push(&client->pending_unreliable, &response);
                    }
                } break;

                case MSG_JOIN_ACCEPT:
                case MSG_JOIN_DENY_BAD_PROTOCOL:
                case MSG_JOIN_DENY_BAD_USERNAME:
                case MSG_JOIN_DENY_SERVER_FULL:
                    break;

                case MSG_JOIN_ACK: {
                } break;

                case MSG_LEAVE: {

                } break;

                case MSG_SNAKE_METADATA: {

                } break;

                case MSG_SNAKE_METADATA_ACK: {

                } break;

                case MSG_SNAKE_DATA: {

                } break;

                case MSG_FOOD_CREATE: {

                } break;

                case MSG_FOOD_CREATE_ACK: {

                } break;

                case MSG_FOOD_DESTROY: {

                } break;

                case MSG_FOOD_DESTROY_ACK: {

                } break;
            }

            i += payload_len + 2;
        }
    }

    return 0;
}
