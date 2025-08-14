#pragma once

#include "clither/config.h"
#include <stdint.h>

#define AUDIO_SFX_LIST                                                         \
    X(button_hover, BUTTON_HOVER)                                              \
    X(button_click, BUTTON_CLICK)                                              \
    X(button_back, BUTTON_BACK)                                                \
    X(slider_click, SLIDER_CLICK)                                              \
    X(slider_drag, SLIDER_DRAG)                                                \
    X(slider_release, SLIDER_RELEASE)                                          \
    X(textinput_type, TEXTINPUT_TYPE)                                          \
    X(textinput_delete, TEXTINPUT_DELETE)                                      \
    X(eat_food, EAT_FOOD)

enum audio_sfx
{
#define X(name, NAME) SFX_##NAME,
    AUDIO_SFX_LIST
#undef X
        SFX_COUNT
};

enum audio_music
{
    MUSIC_MENU,

    MUSIC_COUNT
};

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

    void (*start_voice)(struct audio* audio);
    void (*stop_voice)(struct audio* audio);
    void (*queue_voice_frame)(struct audio* audio, const void* data, int size);
    void (*queue_missing_voice_frame)(struct audio* audio);
    int (*record_voice_frame)(struct audio* audio, void* data, int capacity);
    void (*set_voice_volume)(struct audio* audio, uint8_t percent);

    void (*loop_music)(struct audio* audio, enum audio_music music);
    void (*stop_music)(struct audio* audio);
    void (*set_music_volume)(struct audio* audio, uint8_t percent);

    void (*play_sound)(struct audio* audio, enum audio_sfx sfx);
    void (*set_sound_volume)(struct audio* audio, uint8_t percent);

    void (*update)(struct audio* audio);
};

#if defined(CLITHER_AUDIO)

extern const struct audio_interface* audio_backends[];

#endif
