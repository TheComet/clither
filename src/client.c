#include "clither/client.h"
#include "clither/log.h"
#include "clither/msg_queue.h"
#include "clither/net.h"

#include <string.h>
#include <assert.h>

/* ------------------------------------------------------------------------- */
void
client_init(struct client* client)
{
    client->udp_sock = -1;
    client->timeout_counter = 0;
    client->sim_tick_rate = 60;
    client->net_tick_rate = 20;
    client->state = CLIENT_DISCONNECTED;

    return 0;
}

/* ------------------------------------------------------------------------- */
void
client_deinit(struct client* client)
{
    net_close(client->udp_sock);

    msg_queue_deinit(&client->pending_reliable);
    msg_queue_deinit(&client->pending_unreliable);
}

/* ------------------------------------------------------------------------- */
int
client_connect(struct client* client, const char* server_address, const char* port)
{
    if (!*server_address)
    {
        log_err("No server IP address was specified! Can't init client socket\n");
        log_err("You can use --ip <address> to specify an address to connect to\n");
        return -1;
    }

    assert(client->state == CLIENT_DISCONNECTED);
    assert(client->udp_sock == -1);

    client->udp_sock = net_connect(server_address, port);
    if (client->udp_sock < 0)
        return -1;

    msg_queue_init(&client->pending_unreliable);
    msg_queue_init(&client->pending_reliable);

    client->state = CLIENT_JOINING;

    return 0;
}

/* ------------------------------------------------------------------------- */
void
client_disconnect(struct client* client)
{
    assert(client->state != CLIENT_DISCONNECTED);
    assert(client->udp_sock != -1);

    net_close(client->udp_sock);
    client->udp_sock = -1;
    client->state = CLIENT_DISCONNECTED;

    msg_queue_deinit(&client->pending_reliable);
    msg_queue_deinit(&client->pending_unreliable);
}

/* ------------------------------------------------------------------------- */
int
client_send_pending_data(struct client* client)
{
    uint8_t type;
    char buf[MAX_UDP_PACKET_SIZE];
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
        log_dbg("Sending UDP packet, size=%d\n", len);
        net_send(client->udp_sock, buf, len);
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
int
client_recv(struct client* client)
{
    char buf[MAX_UDP_PACKET_SIZE];
    int i;
    int bytes_received = net_recv(client->udp_sock, buf, MAX_UDP_PACKET_SIZE);

    /* Packet can contain multiple message objects. Unpack */
    for (i = 0; i < bytes_received - 1;)
    {
        union parsed_payload pp;
        enum net_msg_type type = buf[i+0];
        uint8_t payload_len = buf[i+1];
        if (i+2 + payload_len > bytes_received)
        {
            log_warn("Invalid payload length \"%d\" received from server\n",
                (int)payload_len);
            log_warn("Dropping rest of packet\n");
            return 0;
        }

        /* Process message */
        switch (msg_parse_paylaod(&pp, type, payload_len, &buf[i+2]))
        {
            default: {
                log_warn("Received unknown message type from server. Malicious?\n");
            } break;

            case MSG_JOIN_REQUEST:
                break;

            case MSG_JOIN_ACCEPT: {
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
                int rtt = client->frame_number - pp.join_accept.client_frame;
                if (rtt < 0 || rtt > 20 * 5)  /* 5 seconds */
                {
                    log_err("Server sent back a client frame number that is unlikely or impossible.\n");
                    log_err("This may be a bug, or the server is possibly malicious.\n");
                    client->state = CLIENT_DISCONNECTED;
                    return -1;
                }

                /* We will be simulating half rtt in the future, relative to the server */
                client->frame_number = pp.join_accept.server_frame + rtt / 2;

                client->state = CLIENT_JOINED;
                log_dbg("MSG_JOIN_ACCEPT: %d, %d\n", pp.join_accept.spawn.x, pp.join_accept.spawn.y);
            } break;

            case MSG_JOIN_DENY_BAD_PROTOCOL:
            case MSG_JOIN_DENY_BAD_USERNAME:
            case MSG_JOIN_DENY_SERVER_FULL: {
                log_err("Failed to join server: %s\n", pp.join_deny.error);
            } break;

            case MSG_JOIN_ACK:
                break;
        }

        i += payload_len + 2;
    }

    client->timeout_counter = 0;
    return 0;
}
