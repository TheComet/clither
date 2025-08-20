#include "example1/settings_ini.h"
#include <stdint.h>
#include <string.h>

SECTION("client")
struct settings_client
{
    char* username DEFAULT("Snek :D");
    char           address[128] DEFAULT("127.0.0.1");
    uint16_t port  DEFAULT(8080) CONSTRAIN(1, 65535);
};

SECTION("server")
struct settings_server
{
    char*    address;
    uint16_t port;
};

struct settings
{
    struct settings_client client;
    struct settings_server server;
};

static const char* overlay =
    "[server]\n"
    "address = \"0.0.0.0\"\n"

    "[client]\n"
    "username = \"Bob\"\n";

int main(int argc, char* argv[])
{
    struct settings settings;
    (void)argc, (void)argv;

    settings_client_init(&settings.client);
    settings_server_init(&settings.server);

    settings_client_fwrite(&settings.client, stdout);
    settings_server_fwrite(&settings.server, stdout);

    settings_client_parse(&settings.client, "<stdin>", overlay, strlen(overlay));
    settings_server_parse(&settings.server, "<stdin>", overlay, strlen(overlay));

    settings_client_fwrite(&settings.client, stdout);
    settings_server_fwrite(&settings.server, stdout);

    settings_client_deinit(&settings.client);
    settings_server_deinit(&settings.server);

    return 0;
}
