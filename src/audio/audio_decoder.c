#include "clither/audio/audio_decoder.h"
#include "clither/util/str.h"

VEC_DEFINE(pcm16_vec, int16_t, 32)

extern const struct audio_decoder_interface  audio_decoder_flac;
static const struct audio_decoder_interface* interfaces[] = {
    &audio_decoder_flac, NULL};

/* ------------------------------------------------------------------------- */
const struct audio_decoder_interface* audio_decoder_lookup(const char* filename)
{
    int         i;
    const char* required_ext = cstr_ext(filename);
    for (i = 0; interfaces[i] != NULL; ++i)
    {
        const char** ext;
        for (ext = interfaces[i]->file_extensions; *ext != NULL; ++ext)
            if (strcmp(required_ext, *ext) == 0)
                return interfaces[i];
    }

    return NULL;
}
