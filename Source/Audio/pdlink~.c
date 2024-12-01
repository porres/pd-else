/**
 * include the interface to Pd
 */

#ifndef ENABLE_OPUS
#define ENABLE_OPUS 1
#endif

#include "m_pd.h"
#include "magic.h"
#include "else_alloca.h"
#include "link.h"
#if ENABLE_OPUS
#include "opus_compression.h"
#endif
#include <samplerate.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <time.h>

static t_class *pdlink_tilde_class;

typedef struct _pdlink_audio_stream {
    uint32_t stream_id;
    t_int stream_channel;
    t_int stream_active;
    t_float* signal_buffer;
    t_int buf_write_pos;
    t_int buf_read_pos;
    t_int buf_num_ready;
#if ENABLE_OPUS
    t_udp_audio_decoder *audio_decoder;
#endif
    SRC_STATE *samplerate_converter;
} t_pdlink_audio_stream;

typedef struct _pdlink_tilde {
    t_object  x_obj;
    t_glist *x_glist;
    t_int x_local;
    t_int x_debug;
    t_int x_delay;
    t_int x_buf_size;
    t_int x_in_nchs;
    t_int x_out_nchs;
    t_int x_overrun;
    t_symbol *x_name;
    t_link_handle x_link;
    t_clock *x_ping_clock;
    t_clock *x_dsp_reset_clock;
    t_inlet *x_set_inlet;
    t_outlet *x_outlet;
    t_int x_compress;
#if ENABLE_OPUS
    t_udp_audio_encoder *x_audio_encoders;
#endif
    t_pdlink_audio_stream *x_audio_streams;
    t_int x_num_streams;
} t_pdlink_tilde;

static void pdlink_audio_stream_init(t_pdlink_tilde *x, t_pdlink_audio_stream *stream, t_int stream_id)
{
#if ENABLE_OPUS
    stream->audio_decoder = udp_audio_decoder_init();
#endif
    stream->buf_write_pos = x->x_delay;
    stream->buf_read_pos = 0;
    stream->buf_num_ready = x->x_delay;
    stream->stream_id = stream_id;
    stream->stream_active = 0;
    stream->signal_buffer = calloc(x->x_buf_size, sizeof(t_float));

    int error;
    stream->samplerate_converter = src_new(SRC_LINEAR, 1, &error);
    if (stream->samplerate_converter == NULL || error) {
        pd_error(x, "[pdlink~]: failed to initialise libsamplerate");
        stream->samplerate_converter = NULL;
    }
}

static void pdlink_audio_stream_free(t_pdlink_audio_stream *stream)
{
    free(stream->signal_buffer);
#if ENABLE_OPUS
    udp_audio_decoder_destroy(stream->audio_decoder);
#endif
    src_delete(stream->samplerate_converter);
}

static void pdlink_tilde_audio_stream_convert_samplerate(t_pdlink_audio_stream *stream, const t_float *input, long input_frames, const t_float *output, long *output_frames, float sr_original, float sr_target)
{
    SRC_DATA src_data;
    src_data.data_in = input;
    src_data.input_frames = input_frames;
    src_data.data_out = output;
    src_data.output_frames = *output_frames;
    src_data.src_ratio = sr_target / sr_original;
    src_data.end_of_input = 0;

    src_process(stream->samplerate_converter, &src_data);

    *output_frames = src_data.output_frames_gen;
}

static t_pdlink_audio_stream* pdlink_tilde_get_audio_stream(t_pdlink_tilde *x, const uint32_t stream_id) {
    // Look for existing stream
    for(int i = 0; i < x->x_num_streams; i++)
    {
         if(x->x_audio_streams[i].stream_id == stream_id)
         {
             return &x->x_audio_streams[i];
         }
    }
    // Look for streams that can be reused
    for(int i = 0; i < x->x_num_streams; i++)
    {
        if(x->x_audio_streams[i].stream_active == 0)
        {
            t_pdlink_audio_stream *stream = &x->x_audio_streams[i];
#if ENABLE_OPUS
            udp_audio_decoder_destroy(stream->audio_decoder);
#endif
            pdlink_audio_stream_init(x, stream, stream_id);
            return stream;
        }
    }

    // Allocate a new stream
    if(x->x_num_streams)
    {
        x->x_audio_streams = realloc(x->x_audio_streams, (++x->x_num_streams) * sizeof(t_pdlink_audio_stream));
    }
    else {
        x->x_audio_streams = malloc((++x->x_num_streams) * sizeof(t_pdlink_audio_stream));
    }

    t_pdlink_audio_stream *stream = &x->x_audio_streams[x->x_num_streams-1];
    pdlink_audio_stream_init(x, stream, stream_id);
    return stream;
}

static t_int pdlink_tilde_get_num_stream_channels(t_pdlink_tilde *x)
{
    t_int nchs = 1;
    for(int i = 0; i < x->x_num_streams; i++)
    {
        if(x->x_audio_streams[i].stream_active)
        {
            int ch = x->x_audio_streams[i].stream_channel + 1;
            if(ch > nchs) nchs = ch;
        }
    }
    return nchs;
}

static void pdlink_send_signal_message(t_link_handle link, const uint16_t channel, const float samplerate, const uint16_t compressed, const size_t bufsize, const char* buffer)
{
    size_t size = bufsize + 16;
    char* message_buf = ALLOCA(char, size);
    memset(message_buf, 0, size);

    uint16_t port = (uint16_t)link_get_own_port(link);

    // Format signal header
    memcpy(message_buf, &port, sizeof(uint16_t));
    memcpy(message_buf + 2, &channel, sizeof(uint16_t));
    memcpy(message_buf + 4, &compressed, sizeof(uint16_t));
    memcpy(message_buf + 6, &samplerate, sizeof(float));

    // Copy signal data
    memcpy(message_buf + 16, buffer, bufsize);

    // Send message
    link_send(link, size, message_buf);
    FREEA(message_buf, char, size);
}


void pdlink_tilde_receive(void *ptr, const size_t len, const char* message) {
    t_pdlink_tilde *x = (t_pdlink_tilde *)ptr;
    const t_float* samples = (const t_float*)(message + 16);

    uint32_t stream_id;
    uint16_t stream_channel;
    uint16_t stream_compressed;

    float stream_samplerate;
    memcpy(&stream_id, message, sizeof(uint32_t));
    memcpy(&stream_channel, message + 2, sizeof(uint16_t));
    memcpy(&stream_compressed, message + 4, sizeof(uint16_t));
    memcpy(&stream_samplerate, message + 6, sizeof(float));

    float current_samplerate = sys_getsr();
    t_pdlink_audio_stream *stream = pdlink_tilde_get_audio_stream(x, stream_id);
    stream->stream_active = 1;
    stream->stream_channel = stream_channel;

    if(stream_compressed)
    {
#if ENABLE_OPUS
        // Decode
        t_float output_buffer[120];
        int num_decoded = udp_audio_decoder_decode(stream->audio_decoder, (unsigned char*)message+16, len - 16, output_buffer, 120);

        // Convert samplerate. Compresed streams are always 48khz
        int max_buffer_size = ceil((double)num_decoded * 48000.0 / current_samplerate);
        t_float* converted_samples = ALLOCA(t_float, max_buffer_size);
        long output_frames = max_buffer_size;
        pdlink_tilde_audio_stream_convert_samplerate(stream, output_buffer, num_decoded, converted_samples, &output_frames, 48000.0f, current_samplerate);

        if(x->x_debug && stream->buf_num_ready + output_frames > x->x_buf_size)
        {
            if(!x->x_overrun) post("[pdlink~]: buffer overrun for port %i", (uint16_t)stream->stream_id);
            stream->buf_write_pos = (stream->buf_read_pos + x->x_delay) % x->x_buf_size;
            stream->buf_num_ready = x->x_delay;
            memset(stream->signal_buffer, 0, x->x_buf_size * sizeof(t_float));
            x->x_overrun++;
            return;
        }
        for(long i = 0; i < output_frames; i++)
        {
            stream->signal_buffer[stream->buf_write_pos] = converted_samples[i];
            stream->buf_write_pos = (stream->buf_write_pos + 1) % x->x_buf_size;
            stream->buf_num_ready++;
        }
        FREEA(converted_samples, t_float, max_buffer_size);
#else
        pd_error(NULL, "[pdlink~] receiving compressed stream, but was built without opus");
#endif
    }
    else {
        int num_float = (len - 16) / sizeof(t_float);
        if(current_samplerate != stream_samplerate) // Perform sample rate conversion if necessary
        {
            int max_buffer_size = ceil((double)num_float * current_samplerate / stream_samplerate);
            long output_frames = max_buffer_size;
            t_float* converted_samples = ALLOCA(t_float, max_buffer_size);
            pdlink_tilde_audio_stream_convert_samplerate(stream, samples, num_float, converted_samples, &output_frames, stream_samplerate, current_samplerate);

            if(x->x_debug && stream->buf_num_ready + output_frames > x->x_buf_size)
            {
                if(!x->x_overrun) post("[pdlink~]: buffer overrun for port %i", (uint16_t)stream->stream_id);
                stream->buf_write_pos = (stream->buf_read_pos + x->x_delay) % x->x_buf_size;
                stream->buf_num_ready = x->x_delay;
                x->x_overrun++;
                return;
            }
            for(long i = 0; i < output_frames; i++)
            {
                stream->signal_buffer[stream->buf_write_pos] = converted_samples[i];
                stream->buf_write_pos = (stream->buf_write_pos + 1) % x->x_buf_size;
                stream->buf_num_ready++;
            }
            FREEA(converted_samples, t_float, max_buffer_size);
        }
        else {
            if(x->x_debug &&  stream->buf_num_ready + num_float > x->x_buf_size)
            {
                if(!x->x_overrun) post("[pdlink~]: buffer overrun for port %i", (uint16_t)stream->stream_id);
                stream->buf_write_pos = (stream->buf_read_pos + x->x_delay) % x->x_buf_size;
                stream->buf_num_ready = x->x_delay;
                x->x_overrun++;
                return;
            }
            for(int i = 0; i < num_float; i++)
            {
                stream->signal_buffer[stream->buf_write_pos] = samples[i];
                stream->buf_write_pos = (stream->buf_write_pos + 1) % x->x_buf_size;
                stream->buf_num_ready++;
            }
        }
    }
    if(x->x_overrun > 0) x->x_overrun--;
}

static int current_channel;
static void pdlink_send_compressed(void *link, const size_t size, const char* data){
    pdlink_send_signal_message((t_link_handle)link, current_channel, sys_getsr(), 1, size, data);
}

// Receive callback for messages
static t_int *pdlink_tilde_perform(t_int *w){
    t_pdlink_tilde *x = (t_pdlink_tilde *)(w[1]);
    int nblock = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *out1 = (t_float *)(w[4]);

    // Don't send if there's no inlet connection
    int connected = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    if(connected) {
        for(int ch = 0; ch < x->x_in_nchs; ch++) {
            if(x->x_compress)
            {
#if ENABLE_OPUS
                // Decode audio and send over udp
                current_channel = ch;
                udp_audio_encoder_encode(&x->x_audio_encoders[ch], in1 + (ch * nblock), nblock, sys_getsr(), x->x_link, pdlink_send_compressed);
#endif
            }
            else {
                pdlink_send_signal_message(x->x_link, ch, sys_getsr(), 0, nblock * sizeof(t_float), (const char*)(in1 + ch * nblock));
            }
        }
    }

    link_receive(x->x_link, x, pdlink_tilde_receive);

    // Num channel changed, update DSP chain
    if(pdlink_tilde_get_num_stream_channels(x) != x->x_out_nchs)
    {
        clock_delay(x->x_dsp_reset_clock, 0);
    }

    memset(out1, 0, nblock*x->x_out_nchs*sizeof(t_float));
    for(int i = 0; i < x->x_num_streams; i++) {
        t_pdlink_audio_stream *stream = &x->x_audio_streams[i];
        if(stream->stream_active && stream->buf_num_ready < nblock)
        {
            if(x->x_debug)
            {
                post("[pdlink~]: buffer underrun for port %i", (uint16_t)stream->stream_id);
            }
            stream->stream_active = 0;
            continue;
        }
        if(!stream->stream_active || stream->stream_channel >= x->x_out_nchs) continue;
        for(int n = 0; n < nblock; n++)
        {
            out1[n + stream->stream_channel*nblock] += stream->signal_buffer[stream->buf_read_pos];
            stream->buf_read_pos = (stream->buf_read_pos + 1) % x->x_buf_size;
            stream->buf_num_ready--;
        }
    }

    return w+5;
}

static void pdlink_tilde_dsp(t_pdlink_tilde *x, t_signal **sp) {
    if(x->x_name == gensym("")) return;

    if(x->x_compress && sp[0]->s_nchans != x->x_in_nchs)
    {
#if ENABLE_OPUS
        if(x->x_audio_encoders) {
            for(int ch = 0; ch < x->x_in_nchs; ch++)
            {
                udp_audio_encoder_destroy(&x->x_audio_encoders[ch]);
            }
        }

        x->x_audio_encoders = calloc(sp[0]->s_nchans, sizeof(t_udp_audio_encoder));
        for(int ch = 0; ch < sp[0]->s_nchans; ch++)
        {
            x->x_audio_encoders[ch] = *udp_audio_encoder_init();
        }
#endif
    }
    x->x_in_nchs = sp[0]->s_nchans;
    x->x_out_nchs = pdlink_tilde_get_num_stream_channels(x);
    signal_setmultiout(&sp[1], x->x_out_nchs);
    dsp_add(pdlink_tilde_perform, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
}

static void pdlink_tilde_connection_lost(void *x, const int port)
{
    if(((t_pdlink_tilde*)x)->x_debug)
    {
         post("[pdlink~]: connection lost: %i", port);
    }
}

// Discovery and message retrieval loop
static void pdlink_tilde_discover_loop(t_pdlink_tilde *x)
{
    clock_delay(x->x_ping_clock, 400);

    link_discover(x->x_link);

    int num_peers = link_get_num_peers(x->x_link);
    for(int i = 0; i < num_peers; i++)
    {
        t_link_discovery_data data = link_get_discovered_peer_data(x->x_link, i);
        if(strcmp(data.sndrcv, x->x_name->s_name) == 0) {
            // Only connect to localhost in -local mode
            if(x->x_local && strcmp(data.ip, "127.0.0.1") != 0) continue;
            if(!x->x_local && strcmp(data.ip, "127.0.0.1") == 0) continue;

            int created = link_connect(x->x_link, data.port, data.ip);
            if(created && x->x_debug)
            {
               post("[pdlink~]: connected to:\n%s\n%s:%i\n%s", data.hostname, data.ip, data.port, data.platform);
            }
        }
        if(data.hostname) free(data.hostname);
        if(data.sndrcv) free(data.sndrcv);
        if(data.platform) free(data.platform);
        if(data.ip) free(data.ip);
    }

    // Send ping to connected servers, and remove connections that we have not received a ping from in the last 1.5 seconds
    link_ping(x->x_link, x, pdlink_tilde_connection_lost);
}

static void pdlink_tilde_free(t_pdlink_tilde *x)
{
    if(x->x_link) link_free(x->x_link);
    if(x->x_ping_clock) clock_free(x->x_ping_clock);
    if(x->x_dsp_reset_clock) clock_free(x->x_dsp_reset_clock);

    if(x->x_audio_streams) {
        for(int i = 0; i < x->x_num_streams; i++)
        {
            pdlink_audio_stream_free(&x->x_audio_streams[i]);
        }
        free(x->x_audio_streams);
    }
#if ENABLE_OPUS
    if(x->x_audio_encoders) {
        for(int i = 0; i < x->x_in_nchs; i++)
        {
            udp_audio_encoder_destroy(&x->x_audio_encoders[i]);
        }
        free(x->x_audio_encoders);
    }
#endif
}

static void pdlink_tilde_update_dsp(t_pdlink_tilde *x)
{
    canvas_update_dsp();
}

static void pdlink_tilde_set(t_pdlink_tilde *x, t_symbol *s)
{
    x->x_name = s;

    // Get pd platform identifier (only what's known at compile time, so any external will report pure-data)
    char pd_platform[MAXPDSTRING];
    char os[16];

    #if _WIN32
        snprintf(os, 16, "Windows");
    #elif __APPLE__
        #include "TargetConditionals.h"
        #if TARGET_OS_IPHONE && TARGET_IPHONE_SIMULATOR
            snprintf(os, 16, "iOS Simulator");
        #elif TARGET_OS_IPHONE
            snprintf(os, 16, "iOS");
        #else
            snprintf(os, 16, "macOS");
        #endif
    #elif __linux__
        snprintf(os, 16, "Linux");
    #else
        snprintf(os, 16, "Unknown OS");
    #endif

#if PLUGDATA
    snprintf(pd_platform, MAXPDSTRING, "plugdata %s - %s", PD_PLUGDATA_VERSION, os);
#else
    int major = 0, minor = 0, bugfix = 0;
    sys_getversion(&major, &minor, &bugfix);
    snprintf(pd_platform, MAXPDSTRING, "pure-data %i.%i-%i - %s", major, minor, bugfix, os);
#endif

    if(x->x_link) link_free(x->x_link);
    x->x_link = link_init(x->x_name->s_name, pd_platform, x->x_local, 7680413);
    if(!x->x_link)
    {
        pd_error(x, "[pdlink]: failed to bind server socket");
        x->x_link = NULL; // TODO: handle this state!
    }
}

static void *pdlink_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
    t_pdlink_tilde *x = (t_pdlink_tilde *)pd_new(pdlink_tilde_class);
    x->x_name = gensym("");
    x->x_link = NULL;
    x->x_ping_clock = NULL;
    x->x_local = 0;
    x->x_debug = 0;
    x->x_delay = 1024;
    x->x_in_nchs = 0;
    x->x_out_nchs = 1;
    x->x_overrun = 0;
    x->x_num_streams = 0;
    x->x_audio_streams = NULL;
#if ENABLE_OPUS
    x->x_audio_encoders = NULL;
#endif
    x->x_glist = canvas_getcurrent();

    for (int i = 0; i < argc; i++) {
        if (argv[i].a_type == A_SYMBOL) {
            t_symbol *sym = atom_getsymbol(&argv[i]);
            if (strcmp(sym->s_name, "-local") == 0) {
                x->x_local = 1; // Localhost only connection
            } else if (strcmp(sym->s_name, "-debug") == 0) {
                x->x_debug = 1; // Enable debug logging
            }  else if (strcmp(sym->s_name, "-compress") == 0) {
#if ENABLE_OPUS
                x->x_compress = 1; // Enable opus compression
#else
                pd_error(x, "[pdlink~] opus compression is disabled for this build");
#endif
            } else if(strcmp(sym->s_name, "-bufsize") == 0 && i < argc-1 && argv[i+1].a_type == A_FLOAT) {
                i++;
                x->x_delay = atom_getfloat(argv + i);
            }
            else if (x->x_name == gensym("")) {
                // Assign the first non-flag symbol to x_name
                x->x_name = sym;
            }
        }
    }

    int is_valid = x->x_name != gensym("");

    pdlink_tilde_set(x, x->x_name);

    if(!x->x_link)
    {
        pd_error(x, "[pdlink~]: failed to bind server socket");
        pd_free((t_pd*)x);
        return NULL;
    }
    if(is_valid) {
        x->x_dsp_reset_clock = clock_new(x, (t_method)pdlink_tilde_update_dsp);
        x->x_ping_clock = clock_new(x, (t_method)pdlink_tilde_discover_loop);
        clock_delay(x->x_ping_clock, 0);
    }

    if(x->x_delay < 64)
    {
        post("[pdlink~]: bufsize needs to be at least 64 samples");
        post("bufsize set to 64 samples");
        x->x_delay = 64;
    }

    if(x->x_debug)
    {
        post("[pdlink~]: own IP:\n%s:%i", link_get_own_ip(x->x_link), link_get_own_port(x->x_link));
    }

    x->x_set_inlet = inlet_new((t_object*)x, (t_pd*)x, &s_symbol, gensym("__set"));
    x->x_outlet = outlet_new((t_object*)x, &s_signal);
    x->x_buf_size = x->x_delay >= 4096 ? x->x_delay * 2 : 8192; // needs to have at least a decent size to prevent overruns
    return (void *)x;
}

void pdlink_tilde_setup(void) {
    pdlink_tilde_class = class_new(gensym("pdlink~"),
                             (t_newmethod)pdlink_tilde_new,
                             (t_method)pdlink_tilde_free,
                             sizeof(t_pdlink_tilde),
                             CLASS_MULTICHANNEL,
                             A_GIMME, 0);

    class_addmethod(pdlink_tilde_class, nullfn, gensym("signal"), 0);
    class_addmethod(pdlink_tilde_class, (t_method)pdlink_tilde_dsp, gensym("dsp"), 0);
    class_addmethod(pdlink_tilde_class, (t_method)pdlink_tilde_set, gensym("__set"), A_SYMBOL, 0);
}
