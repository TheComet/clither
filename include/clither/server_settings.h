#pragma once

#include "clither/config.h"
#include "cstructures/hashmap.h"

C_BEGIN

struct server_settings
{
	int max_players;
	int max_username_len;
	int sim_tick_rate;
	int net_tick_rate;
	char port[6];
	
	struct cs_hashmap banned_ips;
};

void
server_settings_load_or_set_defaults(struct server_settings* s, const char* filename);

void
server_settings_save(const struct server_settings* s, const char* filename);

C_END
