#include "clither/audio/audio.h"
#include <stddef.h>

#if defined(CLITHER_AUDIO_OPENAL)
extern const struct audio_interface audio_openal;
#endif

const struct audio_interface* audio_backends[] = {
#if defined(CLITHER_AUDIO_OPENAL)
    &audio_openal,
#endif
    NULL};
