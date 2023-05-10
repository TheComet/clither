#include "clither/log.h"
#include "clither/server_settings.h"
#include "cstructures/string.h"

#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

/* ------------------------------------------------------------------------- */
static void
set_defaults(struct server_settings* s)
{
	s->max_players = 600;
	s->max_username_len = 32;
	s->sim_tick_rate = 60;
	s->net_tick_rate = 20;
	strcpy(s->port, "5555");
}

/* ------------------------------------------------------------------------- */
void
server_settings_load_or_set_defaults(struct server_settings* server, const char* filename)
{
	FILE* fp;
	struct cs_string line;
	char server_section_found;

	set_defaults(server);

	if (!*filename)
		return;

	fp = fopen(filename, "r");
	if (fp == NULL)
	{
		log_warn("Failed to open config file \"%s\": %s\n", filename, strerror(errno));
		log_warn("Using default server settings\n");

		return;
	}

	log_dbg("Reading config from file \"%s\"\n", filename);
#define SCAN_UNTIL_CHAR(s, c) \
		while (*s != ' ' && *s) \
			++s; \
		if (!*s) \
			continue

#define SCAN_WHILE_CHAR(s, c) \
		while (*s == ' ') \
			++s; \
		if (!*s) \
			continue

#define UNQUOTE_STRING(s) \
		if (*s == '"') \
		{ \
			char* p = s+1; \
			++s; \
			while (*p != '"' && *p) \
				++p; \
			if (!*p) \
				continue; \
			*p = '\0'; \
		}

	string_init(&line);
	server_section_found = 0;
	while (string_getline(&line, fp) > 0)
	{
		char* s = string_cstr(&line);
		char* key;
		char* value;

		if (string_length(&line) == 0)
			continue;
		if (server_section_found == 0)
		{
			if (string_length(&line) < strlen("[server]"))
				continue;
			if (memcmp(s, "[server]", strlen("[server]")) == 0)
				server_section_found = 1;
			continue;
		}
		if (s[0] == '[')
			break;

		/* Keys cannot have spaces */
		key = s;
		SCAN_UNTIL_CHAR(s, ' ');
		*s = '\0';
		++s;

		SCAN_UNTIL_CHAR(s, '=');
		++s;
		SCAN_WHILE_CHAR(s, ' ');
		value = s;

		if (*s == '"')
		{
			++s;
			SCAN_UNTIL_CHAR(s, '"');
			++s;
			*s = '\0';
		}
		else
		{
			while (*s != ' ' && *s != ';' && *s != '#' && *s)
				++s;
			*s = '\0';
		}

		if (strcmp(key, "max_players") == 0)
		{
			UNQUOTE_STRING(value);
			int max_players = atoi(value);
			if (max_players > 0)
				server->max_players = max_players;
			else
				log_err("Invalid value \"%s\" for \"max_players\" in config file\n", value);
		}
		else if (strcmp(key, "max_username_len") == 0)
		{
			UNQUOTE_STRING(value);
			int maxlen = atoi(value);
			if (maxlen > 0)
				server->max_username_len = maxlen;
			else
				log_err("Invalid value \"%s\" for \"max_username_len\" in config file\n", value);
		}
		else if (strcmp(key, "sim_tick_rate") == 0)
		{
			UNQUOTE_STRING(value);
			int rate = atoi(value);
			if (rate > 0)
				server->sim_tick_rate = rate;
			else
				log_err("Invalid value \"%s\" for \"sim_tick_rate\" in config file\n", value);
		}
		else if (strcmp(key, "net_tick_rate") == 0)
		{
			UNQUOTE_STRING(value);
			int rate = atoi(value);
			if (rate > 0)
				server->net_tick_rate = rate;
			else
				log_err("Invalid value \"%s\" for \"net_tick_rate\" in config file\n", value);
		}
		else if (strcmp(key, "port") == 0)
		{
			UNQUOTE_STRING(value);
			if (atoi(value) == 0 || atoi(value) > 65535 || strlen(value) > sizeof(server->port))
			{
				log_err("Invalid port number \"%s\" for \"port\" in config file\n", value);
				continue;
			}
			strcpy(server->port, value);
		}
	}

	string_deinit(&line);
	fclose(fp);
}

/* ------------------------------------------------------------------------- */
void
server_settings_save(const struct server_settings* s, const char* filename)
{
	FILE* fp;

	if (!*filename)
		return;
	
	fp = fopen(filename, "w");
	if (fp == NULL)
	{
		log_err("Failed to save config file \"%s\": %s\n", strerror(errno));
		return;
	}

	log_dbg("Saving config to \"%s\"\n", filename);
	fprintf(fp, "[server]\n");
	fprintf(fp, "max_players = %d     ; Total number of clients allowed to join the server\n", s->max_players);
	fprintf(fp, "max_username_len = %d ; Limits the user name length\n", s->max_username_len);
	fprintf(fp, "sim_tick_rate = %d    ; Simulation speed in Hz\n", s->sim_tick_rate);
	fprintf(fp, "net_tick_rate = %d    ; Network update speed in Hz. Should be smaller or equal to simulation speed\n", s->net_tick_rate);
	fprintf(fp, "port = %s           ; Port to bind server to\n", s->port);
	fclose(fp);
}
 