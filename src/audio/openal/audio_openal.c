#include "clither/audio/audio.h"
#include "clither/util/log.h"
#include "clither/util/mem.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <math.h>
#include <stddef.h>
#include <string.h>

struct audio
{
    ALCdevice*  device;
    ALCcontext* context;
    ALuint      source;
    ALuint      buffer;
};

#define CASE_RETURN(err)                                                       \
    case (err): return #err
const char* al_err_str(ALenum err)
{
    switch (err)
    {
        CASE_RETURN(AL_NO_ERROR);
        CASE_RETURN(AL_INVALID_NAME);
        CASE_RETURN(AL_INVALID_ENUM);
        CASE_RETURN(AL_INVALID_VALUE);
        CASE_RETURN(AL_INVALID_OPERATION);
        CASE_RETURN(AL_OUT_OF_MEMORY);
    }
    return "unknown";
}
#undef CASE_RETURN

#define __al_check_error(file, line)                                           \
    do                                                                         \
    {                                                                          \
        ALenum err = alGetError();                                             \
        if (err != AL_NO_ERROR)                                                \
            log_err("AL Error %s at %s:%d\n", al_err_str(err), file, line);    \
    } while (0)

#define al_check_error() __al_check_error(__FILE__, __LINE__)

/* ------------------------------------------------------------------------- */
static int audio_openal_init(void)
{
    return 0;
}

/* ------------------------------------------------------------------------- */
static void audio_openal_deinit(void)
{
}

/* ------------------------------------------------------------------------- */
static struct audio* audio_openal_create(void)
{
    struct audio* a = mem_alloc(sizeof *a);
    if (a == NULL)
        goto alloc_audio_failed;

    alGetError();

    const ALCchar* devices = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
    const ALCchar *device = devices, *next = devices + 1;
    size_t         len = 0;

    log_dbg("Available audio devices: ");
    while (device && *device != '\0' && next && *next != '\0')
    {
        if (len)
            log_raw(", ");
        log_raw("%s", device);
        len = strlen(device);
        device += (len + 1);
        next += (len + 2);
    }
    log_raw("\n");

    const char* defname = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
    log_info("Using default audio device: %s\n", defname);

    a->device = alcOpenDevice(defname);
    if (a->device == NULL)
    {
        al_check_error();
        goto open_device_failed;
    }

    a->context = alcCreateContext(a->device, NULL);
    if (a->context == NULL)
    {
        al_check_error();
        goto create_context_failed;
    }
    alcMakeContextCurrent(a->context);
    al_check_error();

    alGenSources(1, &a->source);
    al_check_error();
    alGenBuffers(1, &a->buffer);
    al_check_error();

    float    freq = 150.f;
    int      seconds = 1;
    unsigned sample_rate = 44100;
    size_t   buf_size = seconds * sample_rate;

    short* samples = mem_alloc(buf_size * sizeof(*samples));
    int    i;
    for (i = 0; i < buf_size; ++i)
        samples[i] = 32760 * sinf((2.f * M_PI * freq) / sample_rate * i);
    alBufferData(
        a->buffer,
        AL_FORMAT_MONO16,
        samples,
        buf_size * sizeof(*samples),
        sample_rate);
    al_check_error();

    alSourcei(a->source, AL_BUFFER, a->buffer);
    al_check_error();
    alSourcePlay(a->source);
    al_check_error();

    return a;

create_context_failed:
    alcCloseDevice(a->device);
open_device_failed:
    mem_free(a);
alloc_audio_failed:
    return NULL;
}

/* ------------------------------------------------------------------------- */
static void audio_openal_destroy(struct audio* a)
{
    alcCloseDevice(a->device);
    mem_free(a);
}

/* ------------------------------------------------------------------------- */
static void audio_openal_update(struct audio* a)
{
}

/* ------------------------------------------------------------------------- */
const struct audio_interface audio_openal = {
    "OpenAL",
    audio_openal_init,
    audio_openal_deinit,
    audio_openal_create,
    audio_openal_destroy,
    audio_openal_update};
