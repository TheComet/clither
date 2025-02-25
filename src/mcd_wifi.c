#include "clither/args.h"
#include "clither/cli_colors.h"
#include "clither/log.h"
#include "clither/mcd_wifi.h"
#include "clither/mem.h"
#include "clither/net.h"
#include "clither/signals.h"
#include "clither/tick.h"
#include <stdlib.h>
#include <string.h>

VEC_DECLARE(msg_buf, uint8_t*, 16)
VEC_DEFINE(msg_buf, uint8_t*, 16)

struct ctx
{
    struct sockfd_vec* server_fds;
    struct net_addr    client_addr;
    int                client_fd;
    unsigned           client_active : 1;
};

static int send_pending_client_msgs(uint8_t** pmsg, void* user)
{
    uint8_t*    msg = *pmsg;
    struct ctx* ctx = user;
    if (msg[2] == 0)
    {
        int len = (msg[0] << 8) | msg[1];
        net_send(*vec_last(ctx->server_fds), msg + 3, len);
        return mem_free(msg), VEC_ERASE;
    }

    return msg[2]--, VEC_RETAIN;
}

static int send_pending_server_msgs(uint8_t** pmsg, void* user)
{
    uint8_t*    msg = *pmsg;
    struct ctx* ctx = user;
    if (msg[2] == 0)
    {
        int len = (msg[0] << 8) | msg[1];
        if (ctx->client_active)
            net_sendto(ctx->client_fd, msg + 3, len, &ctx->client_addr);
        return mem_free(msg), VEC_ERASE;
    }

    return msg[2]--, VEC_RETAIN;
}

/* ------------------------------------------------------------------------- */
void* run_mcd_wifi(const void* args)
{
    struct ctx         ctx;
    char               buf[NET_MAX_UDP_PACKET_SIZE];
    int                bytes_received;
    uint8_t**          pmsg;
    uint8_t*           msg;
    int*               pfd;
    struct msg_buf*    client_buf;
    struct msg_buf*    server_buf;
    struct tick        tick;
    const struct args* a = args;

    /* Change log prefix and color for server log messages */
    log_set_prefix("McD WiFi: ");
    log_set_colors(COL_B_MAGENTA, COL_RESET);

    mem_init_threadlocal();

    /* Client will connect to this socket */
    ctx.client_fd = net_bind("", a->mcd_port);
    if (ctx.client_fd < 0)
        goto bind_client_fd_failed;

    /* We connect as a proxy to the server */
    sockfd_vec_init(&ctx.server_fds);
    if (net_connect(
            &ctx.server_fds, "localhost", *a->port ? a->port : NET_DEFAULT_PORT)
        < 0)
        goto connect_server_failed;

    msg_buf_init(&client_buf);
    msg_buf_init(&server_buf);

#define TICK_RATE 60
    tick_cfg(&tick, TICK_RATE);

    ctx.client_active = 0;
    log_info(
        "McDonald's WiFi hosted with %dms ping, %d%% packet loss\n",
        a->mcd_latency,
        a->mcd_loss);
    while (signals_exit_requested() == 0)
    {
        /* Read packets from client */
        while (1)
        {
            bytes_received = net_recvfrom(
                ctx.client_fd, buf, sizeof(buf), &ctx.client_addr);
            if (bytes_received < 0)
                goto exit_mcd;
            if (bytes_received == 0)
                break;

            /* Lose packet randomly */
            if (rand() < (int64_t)a->mcd_loss * RAND_MAX / 100)
                continue;

            /* Add packet to queue */
            msg = mem_alloc(bytes_received + 3);
            msg[0] = ((uint16_t)bytes_received) >> 8;
            msg[1] = ((uint16_t)bytes_received) & 0xFF;
            msg[2] = TICK_RATE * a->mcd_latency / 1000;
            memcpy(msg + 3, buf, bytes_received);
            msg_buf_push(&client_buf, msg);
            ctx.client_active = 1;

            /* Duplicate packet randmoly */
            if (rand() < (int64_t)a->mcd_dup * RAND_MAX / 100)
            {
                uint8_t* dup_msg = mem_alloc(bytes_received + 3);
                memcpy(dup_msg, msg, bytes_received + 3);
                msg_buf_push(&client_buf, dup_msg);
            }

            /* Reorder packet randomly */
            if (rand() < (int64_t)a->mcd_reorder * RAND_MAX / 100)
                msg[2] += (int64_t)rand() * TICK_RATE / RAND_MAX;
        }

        /* Read packets from server */
        while (1)
        {
        retry_recv:
            bytes_received
                = net_recv(*vec_last(ctx.server_fds), buf, sizeof(buf));
            if (bytes_received < 0)
            {
                /* This file descriptor is invalid, close it and try with the
                 * next one */
                if (sockfd_vec_count(ctx.server_fds) == 1)
                    goto exit_mcd;
                net_close(*sockfd_vec_pop(ctx.server_fds));
                goto retry_recv;
            }
            if (bytes_received == 0)
                break;

            /* Lose packet randomly */
            if (rand() < (int64_t)a->mcd_loss * RAND_MAX / 100)
                continue;

            msg = mem_alloc(bytes_received + 3);
            msg[0] = ((uint16_t)bytes_received) >> 8;
            msg[1] = ((uint16_t)bytes_received) & 0xFF;
            msg[2] = TICK_RATE * a->mcd_latency / 1000;
            memcpy(msg + 3, buf, bytes_received);
            msg_buf_push(&server_buf, msg);

            /* Duplicate packet randmoly */
            if (rand() < (int64_t)a->mcd_dup * RAND_MAX / 100)
            {
                uint8_t* dup_msg = mem_alloc(bytes_received + 3);
                memcpy(dup_msg, msg, bytes_received + 3);
                msg_buf_push(&server_buf, dup_msg);
            }

            /* Reorder packet randomly */
            if (rand() < (int64_t)a->mcd_reorder * RAND_MAX / 100)
                msg[2] += (int64_t)rand() * TICK_RATE / RAND_MAX;
        }

        msg_buf_retain(client_buf, send_pending_client_msgs, &ctx);
        msg_buf_retain(server_buf, send_pending_server_msgs, &ctx);

        if (tick_wait(&tick) > TICK_RATE * 3) /* 3 seconds */
            tick_skip(&tick);
    }
exit_mcd:;
    log_info("Stopping McDonald's WiFi\n");

    vec_for_each(client_buf, pmsg)
    {
        mem_free(*pmsg);
    }
    vec_for_each(server_buf, pmsg)
    {
        mem_free(*pmsg);
    }
    msg_buf_deinit(server_buf);
    msg_buf_deinit(client_buf);

    vec_for_each(ctx.server_fds, pfd)
    {
        net_close(*pfd);
    }
    sockfd_vec_deinit(ctx.server_fds);
    net_close(ctx.client_fd);

    mem_deinit_threadlocal();
    log_set_colors("", "");
    log_set_prefix("");

    return (void*)0;

connect_server_failed:
    sockfd_vec_deinit(ctx.server_fds);
    net_close(ctx.client_fd);
bind_client_fd_failed:
    mem_deinit_threadlocal();
    log_set_colors("", "");
    log_set_prefix("");
    return (void*)-1;
}
