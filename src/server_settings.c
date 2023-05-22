#include "clither/log.h"
#include "clither/net.h"
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
    s->client_timeout = 5;
    s->malicious_timeout = 60;
    strcpy(s->port, NET_DEFAULT_PORT);
}

/* ------------------------------------------------------------------------- */
int
server_settings_load_or_set_defaults(struct server_settings* server, const char* filename)
{
    FILE* fp;
    struct cs_string line;
    char server_section_found;

    set_defaults(server);

    if (!*filename)
        return 0;

    fp = fopen(filename, "r");
    if (fp == NULL)
    {
        log_warn("Failed to open config file \"%s\": %s\n", filename, strerror(errno));
        log_warn("Using default server settings\n");

        return 0;
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
            int max_players;
            UNQUOTE_STRING(value);
            max_players = atoi(value);
            if (max_players > 0)
                server->max_players = max_players;
            else
            {
                log_err("Invalid value \"%s\" for \"max_players\" in config file\n", value);
                goto error;
            }
        }
        else if (strcmp(key, "max_username_len") == 0)
        {
            int maxlen;
            UNQUOTE_STRING(value);
            maxlen = atoi(value);
            if (maxlen > 0)
                server->max_username_len = maxlen;
            else
            {
                log_err("Invalid value \"%s\" for \"max_username_len\" in config file\n", value);
                goto error;
            }
        }
        else if (strcmp(key, "sim_tick_rate") == 0)
        {
            int rate;
            UNQUOTE_STRING(value);
            rate = atoi(value);
            if (rate > 0)
                server->sim_tick_rate = rate;
            else
            {
                log_err("Invalid value \"%s\" for \"sim_tick_rate\" in config file\n", value);
                goto error;
            }
        }
        else if (strcmp(key, "net_tick_rate") == 0)
        {
            int rate;
            UNQUOTE_STRING(value);
            rate = atoi(value);
            if (rate > 0)
                server->net_tick_rate = rate;
            else
            {
                log_err("Invalid value \"%s\" for \"net_tick_rate\" in config file\n", value);
                goto error;
            }
        }
        else if (strcmp(key, "client_timeout") == 0)
        {
            int seconds;
            UNQUOTE_STRING(value);
            seconds = atoi(value);
            if (seconds > 0)
                server->client_timeout = seconds;
            else
            {
                log_err("Invalid value \"%s\" for \"client_timeout\" in config file\n", value);
                goto error;
            }
        }
        else if (strcmp(key, "malicious_timeout") == 0)
        {
            int seconds;
            UNQUOTE_STRING(value);
            seconds = atoi(value);
            if (seconds > 0)
                server->malicious_timeout = seconds;
            else
            {
                log_err("Invalid value \"%s\" for \"malicious_timeout\" in config file\n", value);
                goto error;
            }
        }
        else if (strcmp(key, "port") == 0)
        {
            UNQUOTE_STRING(value);
            if (atoi(value) == 0 || atoi(value) > 65535 || strlen(value) > sizeof(server->port))
            {
                log_err("Invalid port number \"%s\" for \"port\" in config file\n", value);
                goto error;
            }
            strcpy(server->port, value);
        }
        else
        {
            log_err("Unknown key/value pair in config file: \"%s = %s\"\n", key, value);
            goto error;
        }
    }

    string_deinit(&line);
    fclose(fp);
    return 0;

error:
    string_deinit(&line);
    fclose(fp);
    return -1;
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
        log_err("Failed to save config file \"%s\": %s\n", filename, strerror(errno));
        return;
    }

    log_dbg("Saving config to \"%s\"\n", filename);
    fprintf(fp, "[server]\n");
    fprintf(fp, "max_players = %d     ; Total number of clients allowed to join the server\n", s->max_players);
    fprintf(fp, "max_username_len = %d ; Limits the user name length\n", s->max_username_len);
    fprintf(fp, "sim_tick_rate = %d    ; Simulation speed in Hz\n", s->sim_tick_rate);
    fprintf(fp, "net_tick_rate = %d    ; Network update speed in Hz. Should be smaller or equal to simulation speed\n", s->net_tick_rate);
    fprintf(fp, "client_timeout = %d    ; How many seconds to wait for a client before disconnecting them\n", s->client_timeout);
    fprintf(fp, "malicious_timeout = %d ; How many seconds to keep a client on the malicious list\n", s->malicious_timeout);
    fprintf(fp, "port = %s           ; Port to bind server to\n", s->port);
    fclose(fp);
}
