#pragma once

struct settings;
struct thread;

struct server_instance
{
    const struct settings_server* settings;
    struct thread*                thread;
    const char*                   addr;
    const char*                   port;
};

void* server_instance_run(const void* args);
