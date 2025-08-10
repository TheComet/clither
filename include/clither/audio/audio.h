#pragma once

#include "clither/config.h"

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
    void (*update)(struct audio* audio);
};

#if defined(CLITHER_AUDIO)

extern const struct audio_interface* audio_backends[];

#endif
