#pragma once

struct bot;
struct bot_interface;
struct fs_watch;
struct gfx;
struct gfx_interface;
struct resource_pack;
struct settings;

int main_menu_run(
    const struct gfx_interface** igfx,
    struct gfx**                 gfx,
    struct resource_pack**       pack,
    struct fs_watch**            pack_watch,
    const struct bot_interface*  ibot,
    struct bot*                  bot,
    const struct settings*       settings);
