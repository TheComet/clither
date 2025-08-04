#pragma once

struct semaphore;
struct settings;
struct thread;

struct server_instance
{
    struct thread*         thread;
    struct semaphore*      ready;
    struct semaphore*      stop;
    const struct settings* settings;
    const char*            addr;
    const char*            port;
};

void server_instance_init(struct server_instance* instance);
int  server_instance_start(
     struct server_instance* instance,
     const struct settings*  settings,
     const char*             addr,
     const char*             port);
void server_instance_wait_for_ready(struct server_instance* instance);
int  server_instance_is_running(const struct server_instance* instance);
void server_instance_stop(struct server_instance* instance);
