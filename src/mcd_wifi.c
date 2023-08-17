#include "clither/args.h"
#include "clither/cli_colors.h"
#include "clither/log.h"
#include "clither/mcd_wifi.h"
#include "clither/net.h"
#include "clither/signals.h"
#include "clither/tick.h"

#include "cstructures/init.h"
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

    cs_threadlocal_init();

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
    log_info("McDonald's WiFi hosted with %dms ping, %d%% packet loss\n", a->mcd_latency, a->mcd_loss);
    while (signals_exit_requested() == 0)
    {
        /* Read packets from client */
        while (1)
        {
            bytes_received = net_recvfrom(client_fd, buf, MAX_UDP_PACKET_SIZE, client_addr, client_addrlen);
            if (bytes_received < 0)
                goto exit_mcd;
            if (bytes_received == 0)
                break;

            /* Lose packet randomly */
            if (rand() < (int64_t)a->mcd_loss * RAND_MAX / 100)
                continue;

            /* Add packet to queue */
            uint8_t* msg = MALLOC(bytes_received + 3);
            msg[0] = ((uint16_t)bytes_received) >> 8;
            msg[1] = ((uint16_t)bytes_received) & 0xFF;
            msg[2] = TICK_RATE * a->mcd_latency / 1000;
            memcpy(msg + 3, buf, bytes_received);
            vector_push(&client_buf, &msg);
            client_active = 1;

            /* Duplicate packet randmoly */
            if (rand() < (int64_t)a->mcd_dup * RAND_MAX / 100)
            {
                uint8_t* dup_msg = MALLOC(bytes_received + 3);
                memcpy(dup_msg, msg, bytes_received + 3);
                vector_push(&client_buf, &dup_msg);
            }

            /* Reorder packet randomly */
            if (rand() < (int64_t)a->mcd_reorder * RAND_MAX / 100)
                msg[2] += (int64_t)rand() * TICK_RATE / RAND_MAX;
        }

        /* Read packets from server */
        while (1)
        {
        retry_recv:
            bytes_received = net_recv(*(int*)vector_back(&server_fds), buf, MAX_UDP_PACKET_SIZE);
            if (bytes_received < 0)
            {
                /* This file descriptor is invalid, close it and try with the next one */
                if (vector_count(&server_fds) == 1)
                    goto exit_mcd;
                net_close(*(int*)vector_pop(&server_fds));
                goto retry_recv;
            }
            if (bytes_received == 0)
                break;

            /* Lose packet randomly */
            if (rand() < (int64_t)a->mcd_loss * RAND_MAX / 100)
                continue;

            char* msg = MALLOC(bytes_received + 3);
            msg[0] = ((uint16_t)bytes_received) >> 8;
            msg[1] = ((uint16_t)bytes_received) & 0xFF;
            msg[2] = TICK_RATE * a->mcd_latency / 1000;
            memcpy(msg + 3, buf, bytes_received);
            vector_push(&server_buf, &msg);

            /* Duplicate packet randmoly */
            if (rand() < (int64_t)a->mcd_dup * RAND_MAX / 100)
            {
                uint8_t* dup_msg = MALLOC(bytes_received + 3);
                memcpy(dup_msg, msg, bytes_received + 3);
                vector_push(&server_buf, &dup_msg);
            }

            /* Reorder packet randomly */
            if (rand() < (int64_t)a->mcd_reorder * RAND_MAX / 100)
                msg[2] += (int64_t)rand() * TICK_RATE / RAND_MAX;
        }

        /* Send any pending client messages to server */
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

        /* Send any pending server messages to client */
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
exit_mcd:;
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

    cs_threadlocal_deinit();
    log_set_colors("", "");
    log_set_prefix("");

    return (void*)0;

connect_server_failed:
    vector_deinit(&server_fds);
    net_close(client_fd);
bind_client_fd_failed:
    cs_threadlocal_deinit();
    log_set_colors("", "");
    log_set_prefix("");
    return (void*)-1;
}
