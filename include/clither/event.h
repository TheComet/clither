#pragma once

struct event
{
    char x;
};

struct event_queue
{
    char x;
};

void
event_put(struct event e);

int
event_take(struct event* e);
