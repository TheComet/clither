#pragma once

#include "clither/util/vec.h"

VEC_DECLARE(pcm16_vec, int16_t, 32)

enum audio_decoder_format
{
    AUDIO_DECODER_MONO,
    AUDIO_DECODER_STEREO
};

/* Opaque type. This is implemented differently for each decoder */
struct audio_decoder;

struct audio_decoder_interface
{
    const char*  name;
    const char** file_extensions;

    int (*init)(void);
    void (*deinit)(void);

    struct audio_decoder* (*open)(const char* filename);
    void (*close)(struct audio_decoder* decoder);

    void (*reset)(struct audio_decoder* decoder);
    struct pcm16_vec* (*next_buffer)(struct audio_decoder* decoder);
    struct pcm16_vec* (*read_all)(struct audio_decoder* decoder);
    enum audio_decoder_format (*get_format)(struct audio_decoder* decoder);
    int (*get_sample_rate)(struct audio_decoder* decoder);
};

const struct audio_decoder_interface*
audio_decoder_lookup(const char* filename);
