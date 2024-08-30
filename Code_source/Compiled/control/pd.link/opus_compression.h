#include <opus.h>
#include <stdio.h>
#include <stdlib.h>
#include <samplerate.h>
#include <math.h>

#define BUFFER_SIZE 2048

typedef struct {
    OpusEncoder *encoder;
    float *buffer;
    int buffer_size;
    int buffer_head;
    int buffer_tail;
    int buffer_num_ready;
    SRC_STATE* samplerate_converter;
} t_udp_audio_encoder;

static t_udp_audio_encoder *udp_audio_encoder_init() {
    t_udp_audio_encoder *ctx = malloc(sizeof(t_udp_audio_encoder));
    if (!ctx) {
        return NULL;
    }

    int error;
    ctx->encoder = opus_encoder_create(48000, 1, OPUS_APPLICATION_AUDIO, &error);
    opus_encoder_ctl(ctx->encoder, OPUS_SET_BITRATE(256000));
    opus_encoder_ctl(ctx->encoder, OPUS_SET_BANDWIDTH(OPUS_BANDWIDTH_FULLBAND));

    if (!ctx->encoder) {
        free(ctx);
        return NULL;
    }

    ctx->buffer = calloc(BUFFER_SIZE, sizeof(float));
    if (!ctx->buffer) {
        opus_encoder_destroy(ctx->encoder);
        free(ctx);
        return NULL;
    }

    ctx->buffer_size = BUFFER_SIZE;
    ctx->buffer_head = 0;
    ctx->buffer_tail = 0;
    ctx->buffer_num_ready = 0;
    ctx->samplerate_converter = src_new(SRC_LINEAR, 1, &error);
    if (ctx->samplerate_converter == NULL || error) {
        ctx->samplerate_converter = NULL;
    }

    return ctx;
}

static void udp_audio_encoder_convert_samplerate(t_udp_audio_encoder *ctx, const t_float *input, long input_frames, const t_float *output, long *output_frames, float samplerate)
{
    SRC_DATA src_data;
    src_data.data_in = input;
    src_data.input_frames = input_frames;
    src_data.data_out = output;
    src_data.output_frames = *output_frames;
    src_data.src_ratio = 48000.0 / samplerate;
    src_data.end_of_input = 0;
    
    src_process(ctx->samplerate_converter, &src_data);
    
    *output_frames = src_data.output_frames_gen;
}

static void udp_audio_encoder_encode(t_udp_audio_encoder *ctx, float *samples, int num_samples, float samplerate, void* target, void(*encode_callback)(void*, size_t, const char*)) {
    
    // Opus codec expects a sample rate of 48khz
    int max_buffer_size = ceil(48000.0 / samplerate) * num_samples;
    t_float* converted_samples = ALLOCA(t_float, max_buffer_size);
    long output_frames = max_buffer_size;
    udp_audio_encoder_convert_samplerate(ctx, samples, num_samples, converted_samples, &output_frames, samplerate);
    
    for (int i = 0; i < output_frames; i++) {
        ctx->buffer[ctx->buffer_head] = converted_samples[i];
        ctx->buffer_head = (ctx->buffer_head+1) % ctx->buffer_size;
        ctx->buffer_num_ready++;
    }

    // Encode audio when we have enough samples in the buffer
    while (ctx->buffer_num_ready >= 120) {
        float ready_samples[120];
        for (int i = 0; i < 120; i++) {
            ready_samples[i] = ctx->buffer[ctx->buffer_tail];
            ctx->buffer_tail = (ctx->buffer_tail+1) % ctx->buffer_size;
        }

        unsigned char encoded_data[4000];
        int encoded_size = opus_encode_float(ctx->encoder, ready_samples, 120, encoded_data, 4000);
        if (encoded_size < 0) {
            // TODO: handle error
            break;
        }

        encode_callback(target, encoded_size, (const char*)encoded_data);
        ctx->buffer_num_ready -= 120;
    }
}

static void udp_audio_encoder_destroy(t_udp_audio_encoder *ctx) {
    opus_encoder_destroy(ctx->encoder);
    free(ctx->buffer);
}

typedef struct {
    OpusDecoder *decoder;
} t_udp_audio_decoder;

static t_udp_audio_decoder *udp_audio_decoder_init() {
    t_udp_audio_decoder *ctx = malloc(sizeof(t_udp_audio_decoder));
    if (!ctx) {
        return NULL;
    }

    int error;
    ctx->decoder = opus_decoder_create(48000, 1, &error);
    if (!ctx->decoder || error) {
        free(ctx);
        return NULL;
    }

    return ctx;
}

static int udp_audio_decoder_decode(t_udp_audio_decoder *ctx, const unsigned char *encoded_data, int encoded_size, float *output_samples, int num_samples) {
    // Decode the data directly into the output_samples buffer
    int decoded_samples = opus_decode_float(ctx->decoder, encoded_data, encoded_size, output_samples, num_samples, 0);
    if (decoded_samples < 0) {
        // Error handling
        return 0;
    }

    // Adjust the number of samples if the decoded samples are less than requested
    if (decoded_samples < num_samples) {
        for (int i = decoded_samples; i < num_samples; i++) {
            output_samples[i] = 0.0f; // Fill the remaining space with silence if necessary
        }
    }

    return decoded_samples;
}

static void udp_audio_decoder_destroy(t_udp_audio_decoder *ctx) {
    opus_decoder_destroy(ctx->decoder);
    free(ctx);
}
