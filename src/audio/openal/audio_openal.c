#include "clither/audio/audio.h"
#include "clither/audio/audio_decoder.h"
#include "clither/util/log.h"
#include "clither/util/mem.h"
#include "clither/util/tracker.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <stddef.h>
#include <string.h>

struct audio
{
    ALCdevice*  device;
    ALCcontext* context;

    const struct audio_decoder_interface* adi;
    struct audio_decoder*                 ad;

    ALuint source;

    ALuint   music_rb[4];
    int      music_rb_read, music_rb_write;
    unsigned music_playing : 1;
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
#if defined(CLITHER_DEBUG_MEMORY)
struct tracker_audio
{
    struct tracker* buf;
    struct tracker* source;
};
static struct tracker_audio g_tracker_audio;

static int tracker_audio_init(void)
{
    g_tracker_audio.buf = tracker_create("AL Buffer");
    if (g_tracker_audio.buf == NULL)
        goto tracker_buf_create_failed;
    g_tracker_audio.source = tracker_create("AL Source");
    if (g_tracker_audio.source == NULL)
        goto tracker_source_create_failed;

    return 0;

tracker_source_create_failed:
    tracker_destroy(g_tracker_audio.buf);
tracker_buf_create_failed:
    return -1;
}
static void tracker_audio_deinit(void)
{
    tracker_destroy(g_tracker_audio.source);
    tracker_destroy(g_tracker_audio.buf);
}
/* clang-format off */
void audio_track_buf(ALuint buf, const char* name)
    {tracker_track(g_tracker_audio.buf, (void*)(uintptr_t)buf, 0, name);}
void audio_track_source(ALuint source, const char* name)
    {tracker_track(g_tracker_audio.source, (void*)(uintptr_t)source, 0, name);}

void audio_untrack_buf(ALuint buf)
    {tracker_untrack(g_tracker_audio.buf, (void*)(uintptr_t)buf);}
void audio_untrack_source(ALuint source)
    {tracker_untrack(g_tracker_audio.source, (void*)(uintptr_t)source);}
/* clang-format on */
#else
/* clang-format off */
#    define audio_track_buf(buf, name)       do {} while (0)
#    define audio_track_source(source, name) do {} while (0)

#    define audio_untrack_buf(buf)           do {} while (0)
#    define audio_untrack_source(source)     do {} while (0)
/* clang-format on */
#endif

/* ------------------------------------------------------------------------- */
static int on_next_buffer(
    const struct pcm16_vec*   buffer,
    int                       sample_rate,
    enum audio_decoder_format format,
    void*                     user_data)
{
    struct audio* a = user_data;
    ALenum        al_format;

    if (format == AUDIO_DECODER_MONO)
        al_format = AL_FORMAT_MONO16;
    else
        al_format = AL_FORMAT_STEREO16;

    alBufferData(
        a->music_rb[a->music_rb_write],
        al_format,
        vec_data(buffer),
        vec_count(buffer) * sizeof(int16_t),
        sample_rate);
    al_check_error();

    return 0;
}

/* ------------------------------------------------------------------------- */
static int audio_openal_init(void)
{
#if defined(CLITHER_DEBUG_MEMORY)
    if (tracker_audio_init() < 0)
        return -1;
#endif

    return 0;
}

/* ------------------------------------------------------------------------- */
static void audio_openal_deinit(void)
{
#if defined(CLITHER_DEBUG_MEMORY)
    tracker_audio_deinit();
#endif
}

/* ------------------------------------------------------------------------- */
static struct audio* audio_openal_create(void)
{
    const ALCchar* devices;
    const ALCchar* device;
    const ALCchar* next;
    struct audio*  a;
    const char*    defname;
    int            len;

    a = mem_alloc(sizeof *a);
    if (a == NULL)
        goto alloc_audio_failed;

    alGetError();

    devices = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
    device = devices;
    next = devices + 1;
    len = 0;

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

    defname = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
    log_info("Using default audio device: %s\n", defname);

    a->device = alcOpenDevice(defname);
    if (a->device == NULL)
    {
        al_check_error();
        goto open_device_failed;
    }
    track_mem(a->device, 0, "OpenAL Device");

    a->context = alcCreateContext(a->device, NULL);
    if (a->context == NULL)
    {
        al_check_error();
        goto create_context_failed;
    }
    track_mem(a->context, 0, "OpenAL Context");
    alcMakeContextCurrent(a->context);
    al_check_error();

    alGenSources(1, &a->source);
    audio_track_source(a->source, "OpenAL Music Source");
    al_check_error();

    alGenBuffers(CLITHER_ARRAY_SIZE(a->music_rb), a->music_rb);
    audio_track_buf(a->music_rb[0], "OpenAL Music Buffers");
    al_check_error();
    a->music_rb_read = 0;
    a->music_rb_write = 0;

    a->music_playing = 0;
    a->adi = audio_decoder_lookup(
        "packs/liam-playground/music/Drums of the Deep.flac");
    a->ad = NULL;
    if (a->adi != NULL)
    {
        a->ad = a->adi->open(
            "packs/liam-playground/music/Drums of the Deep.flac",
            on_next_buffer,
            a);
        if (a->ad != NULL)
        {
            a->music_playing = 1;
        }
    }

    return a;

create_context_failed:
    untrack_mem(a->device);
    alcCloseDevice(a->device);
open_device_failed:
    mem_free(a);
alloc_audio_failed:
    return NULL;
}

/* ------------------------------------------------------------------------- */
static void audio_openal_destroy(struct audio* a)
{
    if (a->ad)
        a->adi->close(a->ad);

    audio_untrack_buf(a->music_rb[0]);
    alDeleteBuffers(CLITHER_ARRAY_SIZE(a->music_rb), a->music_rb);

    audio_untrack_source(a->source);
    alDeleteSources(1, &a->source);

    untrack_mem(a->context);
    alcDestroyContext(a->context);

    untrack_mem(a->device);
    alcCloseDevice(a->device);
    mem_free(a);
}

/* ------------------------------------------------------------------------- */
static int audio_openal_load_resource_pack(
    struct audio* audio, const struct resource_audio* res)
{
    return 0;
}

/* ------------------------------------------------------------------------- */
static void audio_openal_unload_resource_pack(struct audio* audio)
{
}

/* ------------------------------------------------------------------------- */
static void audio_openal_loop_music(struct audio* audio, const char* name)
{
}

/* ------------------------------------------------------------------------- */
static void audio_openal_stop_music(struct audio* a)
{
}

/* ------------------------------------------------------------------------- */
static void audio_openal_set_music_volume(struct audio* audio, uint8_t percent)
{
}

/* ------------------------------------------------------------------------- */
static void audio_openal_play_sound(struct audio* audio, const char* name)
{
}

/* ------------------------------------------------------------------------- */
static void audio_openal_set_sound_volume(struct audio* audio, uint8_t percent)
{
}

/* ------------------------------------------------------------------------- */
static void audio_openal_update(struct audio* a)
{
    ALint result;

    alGetSourcei(a->source, AL_BUFFERS_PROCESSED, &result);
    if (result > 0)
    {
        alSourceUnqueueBuffers(a->source, 1, &a->music_rb[a->music_rb_read++]);
        a->music_rb_read = a->music_rb_read % CLITHER_ARRAY_SIZE(a->music_rb);
        al_check_error();
    }

    if (a->music_playing)
    {
        alGetSourcei(a->source, AL_BUFFERS_QUEUED, &result);
        if (result < CLITHER_ARRAY_SIZE(a->music_rb))
        {
            if (a->adi->is_eof(a->ad))
            {
                log_dbg("FLAC Decoder EOF, resetting\n");
                a->adi->reset(a->ad);
            }

            if (a->adi->next_buffer(a->ad) != 0)
            {
                a->music_playing = 0;
                return;
            }

            alSourceQueueBuffers(
                a->source, 1, &a->music_rb[a->music_rb_write++]);
            a->music_rb_write =
                a->music_rb_write % CLITHER_ARRAY_SIZE(a->music_rb);
            al_check_error();
        }

        alGetSourcei(a->source, AL_SOURCE_STATE, &result);
        if (result != AL_PLAYING)
            alSourcePlay(a->source);
    }
}

/* ------------------------------------------------------------------------- */
const struct audio_interface audio_openal = {
    "OpenAL",
    audio_openal_init,
    audio_openal_deinit,
    audio_openal_create,
    audio_openal_destroy,
    audio_openal_load_resource_pack,
    audio_openal_unload_resource_pack,
    audio_openal_loop_music,
    audio_openal_stop_music,
    audio_openal_set_music_volume,
    audio_openal_play_sound,
    audio_openal_set_sound_volume,
    audio_openal_update};
