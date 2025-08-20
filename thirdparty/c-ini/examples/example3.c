#include "example3/player_ini.h"
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

SECTION("sprite")
struct sprite
{
    struct sprite* next IGNORE();
    char*               name;
    int frames          DEFAULT(1);
    int tile_x          DEFAULT(1), tile_y DEFAULT(1);
    char                layer[32];
    float               x, y;
    float scale         DEFAULT(1);
    unsigned            enabled : 1 DEFAULT(1) CONSTRAIN(0, 1);
    bool visible        DEFAULT(true);
};

SECTION("player")
struct player_data
{
    char*                  username;
    char** enemies         DEFAULT("God") DEFAULT("The Devil");
    char                   friends[2][32] DEFAULT("Alice") DEFAULT("Charlie");
    struct sprite* sprites IGNORE();
};

static const char* overlay =
    "[sprite]\n"
    "visible = false\n"
    "name = \"body\"\n"

    "[sprite]\n"
    "name = \"head\"\n"
    "scale = 2\n"

    "[player]\n"
    "username = \"Bob\"\n"
    "friends = \"enemy1\", \"enemy2\"";

static int on_section(struct c_ini_parser* p, void* user_ptr)
{
    struct player_data* player = user_ptr;
    struct sprite*      sprite = malloc(sizeof *sprite);
    sprite_init(sprite);
    sprite->next = player->sprites;
    player->sprites = sprite;
    return sprite_parse_section(sprite, p);
}

int main(void)
{
    struct player_data player;
    struct sprite*     sprite;
    struct sprite*     next;

    player_data_init(&player);
    sprite_parse_all("<stdin>", overlay, strlen(overlay), on_section, &player);
    player_data_parse(&player, "<stdin>", overlay, strlen(overlay));

    player_data_fwrite(&player, stdout);
    for (sprite = player.sprites; sprite; sprite = sprite->next)
        sprite_fwrite(sprite, stdout);

    for (sprite = player.sprites; sprite; sprite = next)
        sprite_deinit(sprite), next = sprite->next, free(sprite);
    player_data_deinit(&player);

    return 0;
}
