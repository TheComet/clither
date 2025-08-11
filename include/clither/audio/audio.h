#pragma once

#include "clither/config.h"
#include <stdint.h>

struct resource_audio;

/*! Opaque type. This is implemented differently depending on the audio backend
 */
struct audio;

struct audio_interface
{
    const char* name;

    int (*init)(void);
    void (*deinit)(void);

    struct audio* (*create)(void);
    void (*destroy)(struct audio* audio);

    int (*load_resource_pack)(
        struct audio* audio, const struct resource_audio* res);
    void (*unload_resource_pack)(struct audio* audio);

    void (*loop_music)(struct audio* audio, const char* name);
    void (*stop_music)(struct audio* audio);
    void (*set_music_volume)(struct audio* audio, uint8_t percent);

    void (*play_sound)(struct audio* audio, const char* name);
    void (*set_sound_volume)(struct audio* audio, uint8_t percent);

    void (*update)(struct audio* audio);
};

#if defined(CLITHER_AUDIO)

extern const struct audio_interface* audio_backends[];

#endif
