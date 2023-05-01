#pragma once

struct event
{

};

struct event_queue
{

};

void
event_put(struct event e);

int
event_take(struct event* e);
