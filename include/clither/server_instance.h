#pragma once

struct server_settings;
struct thread;

struct server_instance
{
    const struct server_settings* settings;
    struct thread*                thread;
    const char*                   ip;
    char                          port[6];
};

void* server_instance_run(const void* args);
