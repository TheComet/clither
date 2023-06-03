#include "clither/args.h"
#include "clither/cli_colors.h"
#include "clither/log.h"
#include "clither/mcd_wifi.h"
#include "clither/net.h"
#include "clither/signals.h"
#include "clither/tick.h"

#include "cstructures/memory.h"
#include "cstructures/vector.h"

#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------------- */
void*
run_mcd_wifi(const void* args)
{
    char buf[MAX_UDP_PACKET_SIZE];
    char client_addr[MAX_ADDRLEN];
    int bytes_received;
    int client_fd, client_addrlen, client_active;
    struct cs_vector server_fds;
    struct cs_vector client_buf, server_buf;
    struct tick tick;
    const struct args* a = args;

    /* Change log prefix and color for server log messages */
    log_set_prefix("McD WiFi: ");
    log_set_colors(COL_B_MAGENTA, COL_RESET);

    memory_init_thread();

    /* Client will connect to this socket */
    client_fd = net_bind("", a->mcd_port, &client_addrlen);
    if (client_fd < 0)
        goto bind_client_fd_failed;

    /* We connect as a proxy to the server */
    vector_init(&server_fds, sizeof(int));
    if (net_connect(&server_fds, "localhost", *a->port ? a->port : NET_DEFAULT_PORT) < 0)
        goto connect_server_failed;

    vector_init(&client_buf, sizeof(char*));
    vector_init(&server_buf, sizeof(char*));

#define TICK_RATE 60
    tick_cfg(&tick, TICK_RATE);

    client_active = 0;
    while (signals_exit_requested() == 0)
    {
        bytes_received = net_recvfrom(client_fd, buf, MAX_UDP_PACKET_SIZE, client_addr, client_addrlen);
        if (bytes_received < 0)
            break;
        if (bytes_received > 0)
        {
            if (rand() > a->mcd_loss * RAND_MAX / 100)
            {
                uint8_t* msg = MALLOC(bytes_received + 3);
                msg[0] = ((uint16_t)bytes_received) >> 8;
                msg[1] = ((uint16_t)bytes_received) & 0xFF;
                msg[2] = TICK_RATE * a->mcd_latency / 1000;
                memcpy(msg + 3, buf, bytes_received);
                vector_push(&client_buf, &msg);
                client_active = 1;

                if (rand() > a->mcd_dup * RAND_MAX / 100)
                {
                    uint8_t* dup_msg = MALLOC(bytes_received + 3);
                    memcpy(dup_msg, msg, bytes_received + 3);
                    vector_push(&client_buf, &dup_msg);
                }

                if (rand() > a->mcd_reorder * RAND_MAX / 100)
                    msg[2] -= rand() * 10 / RAND_MAX;
            }
        }

retry_recv:
        bytes_received = net_recv(*(int*)vector_back(&server_fds), buf, MAX_UDP_PACKET_SIZE);
        if (bytes_received < 0)
        {
            if (vector_count(&server_fds) == 1)
                break;
            net_close(*(int*)vector_pop(&server_fds));
            goto retry_recv;
        }
        if (bytes_received > 0)
        {
            char* msg = MALLOC(bytes_received + 3);
            msg[0] = ((uint16_t)bytes_received) >> 8;
            msg[1] = ((uint16_t)bytes_received) & 0xFF;
            msg[2] = TICK_RATE * a->mcd_latency / 1000;
            memcpy(msg+3, buf, bytes_received);
            vector_push(&server_buf, &msg);
        }

        VECTOR_FOR_EACH(&client_buf, char*, pmsg)
            char* msg = *pmsg;
            if (msg[2] == 0)
            {
                int len = (msg[0] << 8) | msg[1];
                net_send(*(int*)vector_back(&server_fds), msg+3, len);
                FREE(msg);
                VECTOR_ERASE_IN_FOR_LOOP(&client_buf, char*, pmsg);
            }
            else
                msg[2]--;
        VECTOR_END_EACH

        VECTOR_FOR_EACH(&server_buf, char*, pmsg)
            char* msg = *pmsg;
            if (msg[2] == 0)
            {
                int len = (msg[0] << 8) | msg[1];
                if (client_active)
                    net_sendto(client_fd, msg+3, len, client_addr, client_addrlen);
                FREE(msg);
                VECTOR_ERASE_IN_FOR_LOOP(&server_buf, char*, pmsg);
            }
            else
                msg[2]--;
        VECTOR_END_EACH

        if (tick_wait(&tick) > TICK_RATE * 3)  /* 3 seconds */
            tick_skip(&tick);
    }
    log_info("Stopping McDonald's WiFi\n");

    VECTOR_FOR_EACH(&client_buf, char*, pmsg)
        FREE(*pmsg);
    VECTOR_END_EACH
    VECTOR_FOR_EACH(&server_buf, char*, pmsg)
        FREE(*pmsg);
    VECTOR_END_EACH
    vector_deinit(&server_buf);
    vector_deinit(&client_buf);

    VECTOR_FOR_EACH(&server_fds, int, pfd)
        net_close(*pfd);
    VECTOR_END_EACH
    vector_deinit(&server_fds);
    net_close(client_fd);

    memory_deinit_thread();
    log_set_colors("", "");
    log_set_prefix("");

    return (void*)0;

connect_server_failed:
    vector_deinit(&server_fds);
    net_close(client_fd);
bind_client_fd_failed:
    memory_deinit_thread();
    log_set_colors("", "");
    log_set_prefix("");
    return (void*)-1;
}
