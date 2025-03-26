#pragma once

struct settings_server;
struct settings_world;
struct thread;

struct server_instance
{
    const struct settings_server* settings_server;
    const struct settings_world*  settings_world;
    struct thread*                thread;
    const char*                   addr;
    const char*                   port;
};

void* server_instance_run(const void* args);
