#include <opus.h>
#include <stdio.h>
#include <stdlib.h>

#define BUFFER_SIZE 2048

typedef struct {
    OpusEncoder *encoder;
    float *buffer;
    int buffer_size;
    int buffer_head;
    int buffer_tail;
    int buffer_num_ready;
} t_udp_audio_encoder;

t_udp_audio_encoder *udp_audio_encoder_init(int sample_rate) {
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

    return ctx;
}

void udp_audio_encoder_encode(t_udp_audio_encoder *ctx, float *samples, int num_samples, void* target, void(*encode_callback)(void*, size_t, const char*)) {
    for (int i = 0; i < num_samples; i++) {
        ctx->buffer[ctx->buffer_head] = samples[i];
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

void udp_audio_encoder_destroy(t_udp_audio_encoder *ctx) {
    opus_encoder_destroy(ctx->encoder);
    free(ctx->buffer);
    free(ctx);
}

typedef struct {
    OpusDecoder *decoder;
} t_udp_audio_decoder;

t_udp_audio_decoder *udp_audio_decoder_init(int sample_rate) {
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

int udp_audio_decoder_decode(t_udp_audio_decoder *ctx, const unsigned char *encoded_data, int encoded_size, float *output_samples, int num_samples) {
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

void udp_audio_decoder_destroy(t_udp_audio_decoder *ctx) {
    opus_decoder_destroy(ctx->decoder);
    free(ctx);
}
