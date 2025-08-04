#pragma once

struct fs_watch;
struct gfx;
struct gfx_interface;
struct resource_pack;
struct settings;

int garage_menu_run(
    const struct gfx_interface** igfx,
    struct gfx**                 gfx,
    struct resource_pack**       pack,
    struct fs_watch**            pack_watch,
    const struct settings*       settings);

