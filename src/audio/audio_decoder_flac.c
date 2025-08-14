#include "FLAC/stream_decoder.h"
#include "clither/audio/audio_decoder.h"
#include "clither/platform/mfile.h"
#include "clither/util/log.h"
#include "clither/util/tracker.h"
#include <inttypes.h>
#include <string.h>

struct audio_decoder
{
    FLAC__StreamDecoder*      decoder;
    struct pcm16_vec*         pcm16;
    struct mfile              mf;
    int                       offset;
    int                       sample_rate;
    enum audio_format format;
    unsigned                  clear_pcm16 : 1;
};

/* ------------------------------------------------------------------------- */
static FLAC__StreamDecoderReadStatus read_callback(
    const FLAC__StreamDecoder* decoder,
    FLAC__byte                 buffer[],
    size_t*                    bytes,
    void*                      client_data)
{
    struct audio_decoder* d = client_data;
    const uint8_t*        data = d->mf.address;
    int                   bytes_left = d->mf.size - d->offset;
    (void)decoder;

    if ((int)*bytes > bytes_left)
        *bytes = bytes_left;

    if (bytes_left == 0)
        return FLAC__STREAM_DECODER_READ_STATUS_END_OF_STREAM;

    memcpy(buffer, data + d->offset, *bytes);
    d->offset += *bytes;
    return FLAC__STREAM_DECODER_READ_STATUS_CONTINUE;
}
static FLAC__StreamDecoderSeekStatus seek_callback(
    const FLAC__StreamDecoder* decoder,
    FLAC__uint64               absolute_byte_offset,
    void*                      client_data)
{
    struct audio_decoder* d = client_data;
    (void)decoder;

    if ((int)absolute_byte_offset > d->mf.size)
    {
        log_err(
            "FLAC Decoder Seek Error: Offset %" PRIu64
            " exceeds file size %d\n",
            absolute_byte_offset,
            d->mf.size);
        return FLAC__STREAM_DECODER_SEEK_STATUS_ERROR;
    }
    d->offset = (int)absolute_byte_offset;
    return FLAC__STREAM_DECODER_SEEK_STATUS_OK;
}
static FLAC__StreamDecoderTellStatus tell_callback(
    const FLAC__StreamDecoder* decoder,
    FLAC__uint64*              absolute_byte_offset,
    void*                      client_data)
{
    struct audio_decoder* d = client_data;
    (void)decoder;
    *absolute_byte_offset = d->offset;
    return FLAC__STREAM_DECODER_TELL_STATUS_OK;
}
static FLAC__StreamDecoderLengthStatus length_callback(
    const FLAC__StreamDecoder* decoder,
    FLAC__uint64*              stream_length,
    void*                      client_data)
{
    struct audio_decoder* d = client_data;
    (void)decoder;
    *stream_length = d->mf.size;
    return FLAC__STREAM_DECODER_LENGTH_STATUS_OK;
}
static FLAC__bool
eof_callback(const FLAC__StreamDecoder* decoder, void* client_data)
{
    struct audio_decoder* d = client_data;
    (void)decoder;
    return d->offset >= d->mf.size;
}
static FLAC__StreamDecoderWriteStatus write_callback(
    const FLAC__StreamDecoder* decoder,
    const FLAC__Frame*         frame,
    const FLAC__int32* const   buffer[],
    void*                      client_data)
{
    int                   s;
    int                   bps = frame->header.bits_per_sample;
    struct audio_decoder* d = client_data;
    (void)decoder;

    if (d->clear_pcm16)
        pcm16_vec_clear(d->pcm16);

    if (frame->header.channels == 1)
    {
        for (s = 0; s < (int)frame->header.blocksize; ++s)
        {
            int16_t sample = bps > 16 ? (buffer[0][s] / (1 << (bps - 16)))
                                      : (buffer[0][s] * (1 << (16 - bps)));
            if (pcm16_vec_push(&d->pcm16, sample) != 0)
            {
                log_err("FLAC Decoder Write Error: Out of memory\n");
                return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
            }
        }
    }
    else
    {
        for (s = 0; s < (int)frame->header.blocksize; ++s)
        {
            int16_t left = bps > 16 ? (buffer[0][s] / (1 << (bps - 16)))
                                    : (buffer[0][s] * (1 << (16 - bps)));
            int16_t right = bps > 16 ? (buffer[1][s] / (1 << (bps - 16)))
                                     : (buffer[1][s] * (1 << (16 - bps)));
            if (pcm16_vec_push(&d->pcm16, left) != 0 ||
                pcm16_vec_push(&d->pcm16, right) != 0)
            {
                log_err("FLAC Decoder Write Error: Out of memory\n");
                return FLAC__STREAM_DECODER_WRITE_STATUS_ABORT;
            }
        }
    }

    return FLAC__STREAM_DECODER_WRITE_STATUS_CONTINUE;
}
static void metadata_callback(
    const FLAC__StreamDecoder*  decoder,
    const FLAC__StreamMetadata* metadata,
    void*                       client_data)
{
    struct audio_decoder* d = client_data;
    (void)decoder;

    if (metadata->type == FLAC__METADATA_TYPE_STREAMINFO)
    {
        int channels = metadata->data.stream_info.channels;
        int sample_rate = metadata->data.stream_info.sample_rate;

        d->sample_rate = sample_rate;
        d->format = (channels == 1) ? AUDIO_MONO : AUDIO_STEREO;
    }
}
static void error_callback(
    const FLAC__StreamDecoder*     decoder,
    FLAC__StreamDecoderErrorStatus status,
    void*                          client_data)
{
    (void)decoder, (void)client_data;

    log_err(
        "FLAC Decoder Error: %s\n",
        FLAC__StreamDecoderErrorStatusString[status]);
}

/* ------------------------------------------------------------------------- */
static int flac_decoder_init(void)
{
    return 0;
}

/* ------------------------------------------------------------------------- */
static void flac_decoder_deinit(void)
{
}

/* ------------------------------------------------------------------------- */
static struct audio_decoder* flac_decoder_open(const char* filename)
{
    struct audio_decoder* d = mem_alloc(sizeof *d);
    if (d == NULL)
        goto alloc_audio_decoder_failed;

    pcm16_vec_init(&d->pcm16);
    d->offset = 0;

    if (mfile_map_read(&d->mf, filename, 1) != 0)
        goto open_file_failed;

    d->decoder = FLAC__stream_decoder_new();
    if (d->decoder == NULL)
    {
        log_err("Failed to allocate FLAC decoder\n");
        goto alloc_decoder_failed;
    }
    track_mem(d->decoder, sizeof *d->decoder, "FLAC Decoder");

    (void)FLAC__stream_decoder_set_md5_checking(d->decoder, 1);

    if (FLAC__stream_decoder_init_stream(
            d->decoder,
            read_callback,
            seek_callback,
            tell_callback,
            length_callback,
            eof_callback,
            write_callback,
            metadata_callback,
            error_callback,
            d) != FLAC__STREAM_DECODER_INIT_STATUS_OK)
    {
        log_err("Failed to initialize FLAC decoder\n");
        goto init_stream_failed;
    }

    d->sample_rate = 0;
    d->format = AUDIO_MONO;
    if (!FLAC__stream_decoder_process_until_end_of_metadata(d->decoder))
    {
        log_err("Failed to process FLAC metadata\n");
        goto init_stream_failed;
    }

    if (d->sample_rate == 0)
    {
        log_err("FLAC Decoder Error: No sample rate found in metadata\n");
        goto init_stream_failed;
    }

    return d;

init_stream_failed:
    untrack_mem(d->decoder);
    FLAC__stream_decoder_delete(d->decoder);
alloc_decoder_failed:
    mfile_unmap(&d->mf);
open_file_failed:
    pcm16_vec_deinit(d->pcm16);
    mem_free(d);
alloc_audio_decoder_failed:
    return NULL;
}

/* ------------------------------------------------------------------------- */
static void flac_decoder_close(struct audio_decoder* d)
{
    untrack_mem(d->decoder);
    FLAC__stream_decoder_delete(d->decoder);
    mfile_unmap(&d->mf);
    pcm16_vec_deinit(d->pcm16);
    mem_free(d);
}

/* ------------------------------------------------------------------------- */
static void flac_decoder_rewind(struct audio_decoder* d)
{
    FLAC__stream_decoder_reset(d->decoder);
    FLAC__stream_decoder_process_until_end_of_metadata(d->decoder);
}

/* ------------------------------------------------------------------------- */
static struct pcm16_vec* flac_decoder_next_buffer(struct audio_decoder* d)
{
    d->clear_pcm16 = 1;
    if (FLAC__stream_decoder_process_single(d->decoder))
        return d->pcm16;
    return NULL;
}

/* ------------------------------------------------------------------------- */
static struct pcm16_vec* flac_decoder_read_all(struct audio_decoder* d)
{
    d->clear_pcm16 = 0;
    if (FLAC__stream_decoder_process_until_end_of_stream(d->decoder))
        return d->pcm16;
    return NULL;
}

/* ------------------------------------------------------------------------- */
static enum audio_format
flac_decoder_get_format(struct audio_decoder* d)
{
    return d->format;
}

/* ------------------------------------------------------------------------- */
static int flac_decoder_get_sample_rate(struct audio_decoder* d)
{
    return d->sample_rate;
}

/* ------------------------------------------------------------------------- */
static const char*                   file_ext[] = {"flac", "fla", NULL};
const struct audio_decoder_interface audio_decoder_flac = {
    "FLAC Decoder",
    file_ext,
    flac_decoder_init,
    flac_decoder_deinit,
    flac_decoder_open,
    flac_decoder_close,
    flac_decoder_rewind,
    flac_decoder_next_buffer,
    flac_decoder_read_all,
    flac_decoder_get_format,
    flac_decoder_get_sample_rate};
