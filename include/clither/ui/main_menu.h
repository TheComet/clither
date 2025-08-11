#pragma once

struct audio_interface;
struct audio;
struct bot;
struct bot_interface;
struct fs_watch;
struct gfx;
struct gfx_interface;
struct resource_pack;
struct settings;

int main_menu_run(
    const struct audio_interface* iaudio,
    struct audio*                 audio,
    const struct gfx_interface**  igfx,
    struct gfx**                  gfx,
    struct resource_pack**        pack,
    struct fs_watch**             pack_watch,
    const struct bot_interface*   ibot,
    struct bot*                   bot,
    struct settings*              settings);
