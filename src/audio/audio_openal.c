#include "clither/audio/audio.h"
#include "clither/audio/audio_decoder.h"
#include "clither/game/resource_pack.h"
#include "clither/util/log.h"
#include "clither/util/mem.h"
#include "clither/util/tracker.h"
#include "speex/speex.h"
#include <AL/al.h>
#include <AL/alc.h>
#include <stddef.h>
#include <string.h>

#define INVALID_HANDLE ((ALuint) - 1)

enum
{
    SPEEX_FRAME_SIZE = 160,
    VOICE_SAMPLING_RATE = 8000
};

struct audio
{
    ALCdevice*  out_dev;
    ALCdevice*  in_dev;
    ALCcontext* context;

    SpeexBits voice_enc_bits;
    SpeexBits voice_dec_bits;
    void*     voice_enc_state;
    void*     voice_dec_state;

    const struct audio_decoder_interface* imusic_decoder;
    struct audio_decoder*                 music_decoder;
    const char*                           music_filenames[MUSIC_COUNT];

    ALuint voice_source;
    ALuint voice_rb[16];
    int    voice_rb_read, voice_rb_write;

    ALuint music_source;
    ALuint music_rb[4];
    int    music_rb_read, music_rb_write;

    ALuint sfx_buffers[SFX_COUNT];
    ALuint sfx_sources[SFX_COUNT];
};

static void audio_openal_stop_music(struct audio* a);
static void audio_openal_stop_voice(struct audio* audio);

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
    int            len, i;

    a = mem_alloc(sizeof *a);
    if (a == NULL)
        goto alloc_audio_failed;

    speex_bits_init(&a->voice_enc_bits);
    speex_bits_init(&a->voice_dec_bits);
    a->voice_enc_state =
        speex_encoder_init(speex_lib_get_mode(SPEEX_MODEID_NB));
    if (a->voice_enc_state == NULL)
    {
        log_err("Failed to initialize Speex encoder\n");
        goto init_voice_encoder_failed;
    }
    track_mem(a->voice_enc_state, 0, "Speex Encoder State");
    a->voice_dec_state =
        speex_decoder_init(speex_lib_get_mode(SPEEX_MODEID_NB));
    if (a->voice_dec_state == NULL)
    {
        log_err("Failed to initialize Speex decoder\n");
        goto init_voice_decoder_failed;
    }
    track_mem(a->voice_dec_state, 0, "Speex Decoder State");

    alGetError();

    devices = alcGetString(NULL, ALC_CAPTURE_DEVICE_SPECIFIER);
    device = devices;
    next = devices + 1;
    len = 0;
    log_dbg("Available input audio devices: ");
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

    devices = alcGetString(NULL, ALC_DEVICE_SPECIFIER);
    device = devices;
    next = devices + 1;
    len = 0;
    log_dbg("Available output audio devices: ");
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

    defname = alcGetString(NULL, ALC_CAPTURE_DEFAULT_DEVICE_SPECIFIER);
    log_info("Using default input device: %s\n", defname);
    a->in_dev = alcCaptureOpenDevice(
        NULL, VOICE_SAMPLING_RATE, AL_FORMAT_MONO16, SPEEX_FRAME_SIZE);
    if (a->in_dev == NULL)
    {
        al_check_error();
        goto open_capture_device_failed;
    }
    track_mem(a->in_dev, 0, "OpenAL Capture Device");

    defname = alcGetString(NULL, ALC_DEFAULT_DEVICE_SPECIFIER);
    log_info("Using default output device: %s\n", defname);
    a->out_dev = alcOpenDevice(defname);
    if (a->out_dev == NULL)
    {
        al_check_error();
        goto open_device_failed;
    }
    track_mem(a->out_dev, 0, "OpenAL Output Device");

    a->context = alcCreateContext(a->out_dev, NULL);
    if (a->context == NULL)
    {
        al_check_error();
        goto create_context_failed;
    }
    track_mem(a->context, 0, "OpenAL Context");
    alcMakeContextCurrent(a->context);
    al_check_error();

    alGenSources(1, &a->voice_source);
    audio_track_source(a->voice_source, "OpenAL Voice Source");
    al_check_error();

    alGenBuffers(CLITHER_ARRAY_SIZE(a->voice_rb), a->voice_rb);
    audio_track_buf(a->voice_rb[0], "OpenAL Voice Buffers");
    al_check_error();
    a->voice_rb_read = 0;
    a->voice_rb_write = 0;

    alGenSources(1, &a->music_source);
    audio_track_source(a->music_source, "OpenAL Music Source");
    al_check_error();

    a->imusic_decoder = NULL;
    a->music_decoder = NULL;
    for (i = 0; i != MUSIC_COUNT; ++i)
        a->music_filenames[i] = "";

    alGenBuffers(CLITHER_ARRAY_SIZE(a->music_rb), a->music_rb);
    audio_track_buf(a->music_rb[0], "OpenAL Music Buffers");
    al_check_error();
    a->music_rb_read = 0;
    a->music_rb_write = 0;

    alGenSources(CLITHER_ARRAY_SIZE(a->sfx_sources), a->sfx_sources);
    audio_track_source(a->sfx_sources[0], "OpenAL Sound Effect Sources");
    alGenBuffers(CLITHER_ARRAY_SIZE(a->sfx_buffers), a->sfx_buffers);
    audio_track_buf(a->sfx_buffers[0], "OpenAL Sound Effect Buffers");

    return a;

create_context_failed:
    untrack_mem(a->out_dev);
    alcCloseDevice(a->out_dev);
open_device_failed:
    untrack_mem(a->in_dev);
    alcCaptureCloseDevice(a->in_dev);
open_capture_device_failed:
    speex_decoder_destroy(a->voice_dec_state);
init_voice_decoder_failed:
    speex_encoder_destroy(a->voice_enc_state);
init_voice_encoder_failed:
    speex_bits_destroy(&a->voice_dec_bits);
    speex_bits_destroy(&a->voice_enc_bits);
    mem_free(a);
alloc_audio_failed:
    return NULL;
}

/* ------------------------------------------------------------------------- */
static void audio_openal_destroy(struct audio* a)
{
    audio_openal_stop_music(a);
    audio_openal_stop_voice(a);

    audio_untrack_buf(a->sfx_buffers[0]);
    alDeleteBuffers(CLITHER_ARRAY_SIZE(a->sfx_buffers), a->sfx_buffers);
    audio_untrack_source(a->sfx_sources[0]);
    alDeleteSources(CLITHER_ARRAY_SIZE(a->sfx_sources), a->sfx_sources);

    audio_untrack_buf(a->music_rb[0]);
    alDeleteBuffers(CLITHER_ARRAY_SIZE(a->music_rb), a->music_rb);
    audio_untrack_source(a->music_source);
    alDeleteSources(1, &a->music_source);

    audio_untrack_buf(a->voice_rb[0]);
    alDeleteBuffers(CLITHER_ARRAY_SIZE(a->voice_rb), a->voice_rb);
    audio_untrack_source(a->voice_source);
    alDeleteSources(1, &a->voice_source);

    untrack_mem(a->context);
    alcMakeContextCurrent(NULL);
    alcDestroyContext(a->context);

    untrack_mem(a->out_dev);
    alcCloseDevice(a->out_dev);

    untrack_mem(a->in_dev);
    alcCaptureCloseDevice(a->in_dev);

    untrack_mem(a->voice_enc_state);
    speex_decoder_destroy(a->voice_dec_state);
    untrack_mem(a->voice_dec_state);
    speex_encoder_destroy(a->voice_enc_state);
    speex_bits_destroy(&a->voice_dec_bits);
    speex_bits_destroy(&a->voice_enc_bits);

    mem_free(a);
}

/* ------------------------------------------------------------------------- */
static int load_sfx(struct audio* a, enum audio_sfx sfx, const char* filename)
{
    const struct audio_decoder_interface* iad;
    struct audio_decoder*                 ad;
    const struct pcm16_vec*               buffer;

    log_dbg("Loading sound effect \"%s\"\n", filename);

    iad = audio_decoder_lookup(filename);
    if (iad == NULL)
    {
        log_err("Failed to find audio decoder for \"%s\"\n", filename);
        return -1;
    }

    ad = iad->open(filename);
    if (ad == NULL)
    {
        log_err("Failed to open audio decoder for \"%s\"\n", filename);
        return -1;
    }

    buffer = iad->read_all(ad);
    if (vec_count(buffer) == 0)
    {
        iad->close(ad);
        return -1;
    }

    alBufferData(
        a->sfx_buffers[sfx],
        iad->get_format(ad) == AUDIO_MONO ? AL_FORMAT_MONO16
                                          : AL_FORMAT_STEREO16,
        vec_data(buffer),
        vec_count(buffer) * sizeof(int16_t),
        iad->get_sample_rate(ad));
    al_check_error();

    alSourcei(a->sfx_sources[sfx], AL_BUFFER, a->sfx_buffers[sfx]);

    iad->close(ad);
    return 0;
}

/* ------------------------------------------------------------------------- */
static int audio_openal_load_resource_pack(
    struct audio* a, const struct resource_audio* res)
{
    a->music_filenames[MUSIC_MENU] = str_cstr(res->menu_music);

#define X(name, NAME)                                                          \
    if (str_len(res->name) == 0)                                               \
        log_warn("SFX \"" #name "\" missing from pack.ini\n");                 \
    else if (load_sfx(a, SFX_##NAME, str_cstr(res->name)) != 0)                \
        return -1;
    AUDIO_SFX_LIST
#undef X

    return 0;
}

/* ------------------------------------------------------------------------- */
static void audio_openal_unload_resource_pack(struct audio* a)
{
    int i;

#define X(name, NAME) alBufferData(a->sfx_buffers[SFX_##NAME], 0, NULL, 0, 0);
    AUDIO_SFX_LIST
#undef X

    audio_openal_stop_music(a);
    for (i = 0; i != MUSIC_COUNT; ++i)
        a->music_filenames[i] = "";
}

/* ------------------------------------------------------------------------- */
static void audio_openal_start_voice(struct audio* a)
{
    alcCaptureStart(a->in_dev);
    al_check_error();
}

/* ------------------------------------------------------------------------- */
static void audio_openal_stop_voice(struct audio* a)
{
    alcCaptureStop(a->in_dev);
    alGetError(); /* don't care */
}

/* ------------------------------------------------------------------------- */
static void queue_voice_frame(struct audio* a, const void* data, int size)
{
    int16_t frame[SPEEX_FRAME_SIZE];
    int     rc;

    if ((a->voice_rb_write + 1) % CLITHER_ARRAY_SIZE(a->voice_rb) ==
        a->voice_rb_read)
    {
        log_warn("OpenAL voice ring buffer is full, dropping frame\n");
        return;
    }

    if (data == NULL)
        rc = speex_decode_int(a->voice_dec_state, NULL, frame);
    else
    {
        speex_bits_read_from(&a->voice_dec_bits, data, size);
        rc = speex_decode_int(a->voice_dec_state, &a->voice_dec_bits, frame);
    }
    if (rc)
    {
        /* -1 for end of stream, -2 for corrupt stream */
        return;
    }

    alBufferData(
        a->voice_rb[a->voice_rb_write],
        AL_FORMAT_MONO16,
        frame,
        SPEEX_FRAME_SIZE * sizeof(int16_t),
        VOICE_SAMPLING_RATE);
    al_check_error();

    alSourceQueueBuffers(a->voice_source, 1, &a->voice_rb[a->voice_rb_write++]);
    al_check_error();
    a->voice_rb_write = a->voice_rb_write % CLITHER_ARRAY_SIZE(a->voice_rb);

    alGetSourcei(a->voice_source, AL_SOURCE_STATE, &rc);
    if (rc != AL_PLAYING)
    {
        alSourcePlay(a->voice_source);
        al_check_error();
    }
}

/* ------------------------------------------------------------------------- */
static void
audio_openal_queue_voice_frame(struct audio* a, const void* data, int size)
{
    queue_voice_frame(a, data, size);
}

/* ------------------------------------------------------------------------- */
static void audio_openal_queue_missing_voice_frame(struct audio* a)
{
    queue_voice_frame(a, NULL, 0);
}

/* ------------------------------------------------------------------------- */
static int
audio_openal_record_voice_frame(struct audio* a, void* data, int capacity)
{
    ALint   result;
    int16_t frame[SPEEX_FRAME_SIZE];

    alcGetIntegerv(a->in_dev, ALC_CAPTURE_SAMPLES, 1, &result);
    if (result < SPEEX_FRAME_SIZE)
        return 0;

    alcCaptureSamples(a->in_dev, frame, SPEEX_FRAME_SIZE);
    speex_bits_reset(&a->voice_enc_bits);
    speex_encode_int(a->voice_enc_state, frame, &a->voice_enc_bits);
    return speex_bits_write(&a->voice_enc_bits, data, capacity);
}

/* ------------------------------------------------------------------------- */
static void audio_openal_set_voice_volume(struct audio* a, uint8_t percent)
{
    alSourcef(a->voice_source, AL_GAIN, (float)percent / 100.0f);
}

/* ------------------------------------------------------------------------- */
static void audio_openal_loop_music(struct audio* a, enum audio_music music)
{
    audio_openal_stop_music(a);

    log_dbg("Loading music \"%s\"\n", a->music_filenames[music]);

    a->imusic_decoder = audio_decoder_lookup(a->music_filenames[music]);
    if (a->imusic_decoder == NULL)
    {
        log_err(
            "Failed to find audio decoder for \"%s\"\n",
            a->music_filenames[music]);
        return;
    }
    a->music_decoder = a->imusic_decoder->open(a->music_filenames[music]);
    if (a->music_decoder == NULL)
    {
        log_err(
            "Failed to open audio decoder for \"%s\"\n",
            a->music_filenames[music]);
    }
}

/* ------------------------------------------------------------------------- */
static void audio_openal_stop_music(struct audio* a)
{
    if (a->music_decoder != NULL)
    {
        a->imusic_decoder->close(a->music_decoder);
        a->music_decoder = NULL;
        a->imusic_decoder = NULL;
    }
}

/* ------------------------------------------------------------------------- */
static void audio_openal_set_music_volume(struct audio* a, uint8_t percent)
{
    alSourcef(a->music_source, AL_GAIN, (float)percent / 100.0f);
}

/* ------------------------------------------------------------------------- */
static void audio_openal_play_sound(struct audio* a, enum audio_sfx sfx)
{
    alSourcePlay(a->sfx_sources[sfx]);
    al_check_error();
}

/* ------------------------------------------------------------------------- */
static void audio_openal_set_sound_volume(struct audio* a, uint8_t percent)
{
    int i;
    for (i = 0; i != SFX_COUNT; ++i)
        alSourcef(a->sfx_sources[i], AL_GAIN, (float)percent / 100.0f);
}

/* ------------------------------------------------------------------------- */
static void unqueue_buffers(struct audio* a)
{
    ALint result;

    while (alGetSourcei(a->music_source, AL_BUFFERS_PROCESSED, &result),
           result > 0)
    {
        alSourceUnqueueBuffers(
            a->music_source, 1, &a->music_rb[a->music_rb_read++]);
        a->music_rb_read = a->music_rb_read % CLITHER_ARRAY_SIZE(a->music_rb);
        al_check_error();
    }

    while (alGetSourcei(a->voice_source, AL_BUFFERS_PROCESSED, &result),
           result > 0)
    {
        alSourceUnqueueBuffers(
            a->voice_source, 1, &a->voice_rb[a->voice_rb_read++]);
        a->voice_rb_read = a->voice_rb_read % CLITHER_ARRAY_SIZE(a->voice_rb);
        al_check_error();
    }
}

/* ------------------------------------------------------------------------- */
static void update_music(struct audio* a)
{
    ALint result;
    alGetSourcei(a->music_source, AL_BUFFERS_QUEUED, &result);
    if (result < CLITHER_ARRAY_SIZE(a->music_rb))
    {
        const struct pcm16_vec* buffer;
        while (1)
        {
            buffer = a->imusic_decoder->next_buffer(a->music_decoder);
            if (buffer == NULL)
                return;
            if (vec_count(buffer) > 0)
                break;

            a->imusic_decoder->rewind(a->music_decoder);
        }

        alBufferData(
            a->music_rb[a->music_rb_write],
            a->imusic_decoder->get_format(a->music_decoder) == AUDIO_MONO
                ? AL_FORMAT_MONO16
                : AL_FORMAT_STEREO16,
            vec_data(buffer),
            vec_count(buffer) * sizeof(int16_t),
            a->imusic_decoder->get_sample_rate(a->music_decoder));
        al_check_error();

        alSourceQueueBuffers(
            a->music_source, 1, &a->music_rb[a->music_rb_write++]);
        al_check_error();

        a->music_rb_write = a->music_rb_write % CLITHER_ARRAY_SIZE(a->music_rb);
    }

    alGetSourcei(a->music_source, AL_SOURCE_STATE, &result);
    if (result != AL_PLAYING)
        alSourcePlay(a->music_source);
}

/* ------------------------------------------------------------------------- */
static void audio_openal_update(struct audio* a)
{
    unqueue_buffers(a);

    if (a->music_decoder != NULL)
        update_music(a);
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
    audio_openal_start_voice,
    audio_openal_stop_voice,
    audio_openal_queue_voice_frame,
    audio_openal_queue_missing_voice_frame,
    audio_openal_record_voice_frame,
    audio_openal_set_voice_volume,
    audio_openal_loop_music,
    audio_openal_stop_music,
    audio_openal_set_music_volume,
    audio_openal_play_sound,
    audio_openal_set_sound_volume,
    audio_openal_update};
