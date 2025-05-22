// tim schoen and porres 2024

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <pthread.h>
#include <m_pd.h>

#define FRAMES 4096

static t_class *sfload_class;

typedef struct _sfload{
    t_object        x_obj;
    t_outlet       *x_info_outlet;
    AVCodecContext *x_stream_ctx;
    int            x_stream_idx;
    AVPacket       *x_pkt;
    AVFrame        *x_frm;
    AVFormatContext *x_ic;
    AVChannelLayout x_layout;
    unsigned int    x_nchans;
    int             x_ch;
    t_sample       *x_all_out[64];
    t_canvas       *x_canvas;
    t_symbol       *x_arr_name;
    pthread_t       x_process_thread;
    int             x_thread_created;
    int             x_threaded;
    long            x_nsamps;   // number of samples in the file
    long            x_arr_size; // number of samples in the array
    long            x_onset;
    _Atomic int     x_result_ready;
    t_clock        *x_result_clock;
    char            x_path[MAXPDSTRING];
    t_atom          x_sfinfo[4];
}t_sfload;

void* sfload_read_audio_threaded(void *arg){ // read audio into array
    t_sfload *x = (t_sfload*)arg;
    x->x_ic = avformat_alloc_context();
    x->x_ic->probesize = 128;
    x->x_ic->max_probe_packets = 1;
    if(avformat_open_input(&x->x_ic, x->x_path, NULL, NULL) != 0){
        pd_error(x, "[sfload]: Could not open file '%s'", x->x_path);
        return(NULL);
    }
    if(avformat_find_stream_info(x->x_ic, NULL) < 0){
        pd_error(x, "[sfload]: Could not find stream information");
        return(NULL);
    }
    int audio_stream_index = -1;
    for(unsigned int i = 0; i < x->x_ic->nb_streams; i++){
        if(x->x_ic->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
            audio_stream_index = i;
            break;
        }
    }
    if(audio_stream_index == -1){
        pd_error(x, "[sfload]: Could not find any audio stream in the file");
        return(NULL);
    }
    x->x_stream_idx = audio_stream_index;
    AVCodecParameters *codec_parameters = x->x_ic->streams[audio_stream_index]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codec_parameters->codec_id);
    if(!codec){
        pd_error(x, "[sfload]: Codec not found");
        return(NULL);
    }
    x->x_stream_ctx = avcodec_alloc_context3(codec);
    if(!x->x_stream_ctx){
        pd_error(x, "[sfload]: Could not allocate audio codec context");
        return(NULL);
    }
    if(avcodec_parameters_to_context(x->x_stream_ctx, codec_parameters) < 0){
        pd_error(x, "[sfload]: Could not copy codec parameters to context");
        return(NULL);
    }
    if(avcodec_open2(x->x_stream_ctx, codec, NULL) < 0){
        pd_error(x, "[sfload]: Could not open codec");
        return(NULL);
    }
    AVChannelLayout layout;
    if(x->x_stream_ctx->ch_layout.u.mask)
        av_channel_layout_from_mask(&layout, x->x_stream_ctx->ch_layout.u.mask);
    else
        av_channel_layout_default(&layout, x->x_stream_ctx->ch_layout.nb_channels);
    unsigned int nch = layout.nb_channels;

    SwrContext *swr = NULL;
    swr_alloc_set_opts2(&swr, &layout, AV_SAMPLE_FMT_FLTP, x->x_stream_ctx->sample_rate,
        &layout, x->x_stream_ctx->sample_fmt, x->x_stream_ctx->sample_rate, 0, NULL);
    if(!swr || swr_init(swr) < 0){
        pd_error(x, "[sfload]: Could not initialize the resampling context");
        return(NULL);
    }
    t_sample **x_out = (t_sample **)av_mallocz(nch * sizeof(t_sample *));
    for(unsigned int ch = 0; ch < nch; ch++)
        x_out[ch] = (t_sample *)av_mallocz(FRAMES * sizeof(t_sample));
    long output_index = 0;
    while(av_read_frame(x->x_ic, x->x_pkt) >= 0){
        if(x->x_pkt->stream_index == x->x_stream_idx){
            if(avcodec_send_packet(x->x_stream_ctx, x->x_pkt) < 0
            || avcodec_receive_frame(x->x_stream_ctx, x->x_frm) < 0)
                continue;
            int samples_converted = swr_convert(swr, (uint8_t **)x_out, FRAMES,
                (const uint8_t **)x->x_frm->extended_data, x->x_frm->nb_samples);
            if(samples_converted < 0){
                pd_error(x, "[sfload]: Error converting samples");
                continue;
            }
            for(unsigned int ch = 0; ch < nch; ch++){
                x->x_all_out[ch] = realloc(x->x_all_out[ch],
                    (output_index + samples_converted) * sizeof(t_sample));
                memcpy(x->x_all_out[ch] + output_index,
                    x_out[ch], samples_converted * sizeof(t_sample));
            }
           output_index += samples_converted;
        }
        av_packet_unref(x->x_pkt);
    }
    SETFLOAT(x->x_sfinfo + 0, output_index);
    SETFLOAT(x->x_sfinfo + 1, x->x_stream_ctx->sample_rate);
    SETFLOAT(x->x_sfinfo + 2, nch);
    SETFLOAT(x->x_sfinfo + 3, av_get_bytes_per_sample(x->x_stream_ctx->sample_fmt) * 8);
    x->x_nchans = nch;
    x->x_result_ready = output_index;
    x->x_nsamps = output_index;
    for(unsigned int ch = 0; ch < nch; ch++)
        av_free(x_out[ch]);
    av_free(x_out);
    av_channel_layout_uninit(&x->x_layout);
    avcodec_free_context(&x->x_stream_ctx);
    avformat_close_input(&x->x_ic);
    swr_free(&swr);
    return(NULL);
}

void sfload_update_arrays(t_sfload* x){
    float srkhz = sys_getsr() * 0.001;
    long start = x->x_onset * srkhz; // start point in samples
    long arraysize = x->x_arr_size;
    long filesize = x->x_nsamps - start;
    long readsize;
     if(arraysize > 0){ // arraysize is given
        arraysize *= srkhz;
        if(filesize <= arraysize)
            readsize = filesize <= arraysize ? filesize : arraysize;
    }
    else // not given, set to filesize
        readsize = arraysize = filesize;
    for(int ch = 0; ch < x->x_nchans; ch++){
        if(x->x_ch != -1 && ch != x->x_ch) // given channel doesn't match, skip
            continue;
        char channel_name[MAXPDSTRING];
        snprintf(channel_name, MAXPDSTRING, "%i-%s", ch, x->x_arr_name->s_name);
        t_garray* garray = (t_garray*)pd_findbyclass(gensym(channel_name), garray_class);
        if(garray){
            garray_resize_long(garray, arraysize);
            t_word* vec = ((t_word*)garray_vec(garray));
            for(int i = 0; i < arraysize; i++){
                if(i < readsize)
                    vec[i].w_float = x->x_all_out[ch][i+start];
                else
                    vec[i].w_float = 0;
            }
            garray_redraw(garray);
        }
        else{
            if(x->x_ch == -1 && ch > 0) // only load 1st channel
                return;
            garray = (t_garray*)pd_findbyclass(x->x_arr_name, garray_class);
            if(garray){
                garray_resize_long(garray, arraysize);
                t_word* vec = ((t_word*)garray_vec(garray));
                for(int i = 0; i < arraysize; i++){
                    if(i < readsize)
                        vec[i].w_float = x->x_all_out[ch][i+start];
                    else
                        vec[i].w_float = 0;
                }
                garray_redraw(garray);
            }
        }
    }
}

void* sfload_read_audio(t_sfload *x){
    x->x_ic = avformat_alloc_context();
    x->x_ic->probesize = 128;
    x->x_ic->max_probe_packets = 1;
    if(avformat_open_input(&x->x_ic, x->x_path, NULL, NULL) != 0){
        pd_error(x, "[sfload]: Could not open file '%s'", x->x_path);
        return(NULL);
    }
    if(avformat_find_stream_info(x->x_ic, NULL) < 0){
        pd_error(x, "[sfload]: Could not find stream information");
        return(NULL);
    }
    int audio_stream_index = -1;
    for(unsigned int i = 0; i < x->x_ic->nb_streams; i++){
        if(x->x_ic->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
            audio_stream_index = i;
            break;
        }
    }
    if(audio_stream_index == -1){
        pd_error(x, "[sfload]: Could not find any audio stream in the file");
        return(NULL);
    }
    x->x_stream_idx = audio_stream_index;
    AVCodecParameters *codec_parameters = x->x_ic->streams[audio_stream_index]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codec_parameters->codec_id);
    if(!codec){
        pd_error(x, "[sfload]: Codec not found");
        return(NULL);
    }
    x->x_stream_ctx = avcodec_alloc_context3(codec);
    if(!x->x_stream_ctx){
        pd_error(x, "[sfload]: Could not allocate audio codec context");
        return(NULL);
    }
    if(avcodec_parameters_to_context(x->x_stream_ctx, codec_parameters) < 0){
        pd_error(x, "[sfload]: Could not copy codec parameters to context");
        return(NULL);
    }
    if(avcodec_open2(x->x_stream_ctx, codec, NULL) < 0){
        pd_error(x, "[sfload]: Could not open codec");
        return(NULL);
    }
    AVChannelLayout layout;
    if(x->x_stream_ctx->ch_layout.u.mask)
        av_channel_layout_from_mask(&layout, x->x_stream_ctx->ch_layout.u.mask);
    else
        av_channel_layout_default(&layout, x->x_stream_ctx->ch_layout.nb_channels);
    unsigned int nch = layout.nb_channels;
    if(x->x_ch > 0 && x->x_ch > (nch - 1)){
        pd_error(x, "[sfload]: channel (%d) is out of range (from 0 to %d)",
            x->x_ch, nch - 1);
        return(NULL);
    }
    SwrContext *swr = NULL;
    swr_alloc_set_opts2(&swr, &layout, AV_SAMPLE_FMT_FLTP, x->x_stream_ctx->sample_rate,
        &layout, x->x_stream_ctx->sample_fmt, x->x_stream_ctx->sample_rate, 0, NULL);
    if(!swr || swr_init(swr) < 0){
        pd_error(x, "[sfload]: Could not initialize the resampling context");
        return(NULL);
    }
    t_sample **x_out = (t_sample **)av_mallocz(nch * sizeof(t_sample *));
    for(unsigned int ch = 0; ch < nch; ch++)
        x_out[ch] = (t_sample *)av_mallocz(FRAMES * sizeof(t_sample));
    long output_index = 0;
    while(av_read_frame(x->x_ic, x->x_pkt) >= 0){
        if(x->x_pkt->stream_index == x->x_stream_idx){
            if(avcodec_send_packet(x->x_stream_ctx, x->x_pkt) < 0
            || avcodec_receive_frame(x->x_stream_ctx, x->x_frm) < 0)
                continue;
            int samples_converted = swr_convert(swr, (uint8_t **)x_out, FRAMES,
                (const uint8_t **)x->x_frm->extended_data, x->x_frm->nb_samples);
            if(samples_converted < 0){
                pd_error(x, "[sfload]: Error converting samples");
                continue;
            }
            for(unsigned int ch = 0; ch < nch; ch++){
                x->x_all_out[ch] = realloc(x->x_all_out[ch],
                    (output_index + samples_converted) * sizeof(t_sample));
                memcpy(x->x_all_out[ch] + output_index,
                    x_out[ch], samples_converted * sizeof(t_sample));
            }
           output_index += samples_converted;
        }
        av_packet_unref(x->x_pkt);
    }
    SETFLOAT(x->x_sfinfo + 0, output_index);
    SETFLOAT(x->x_sfinfo + 1, x->x_stream_ctx->sample_rate);
    SETFLOAT(x->x_sfinfo + 2, nch);
    SETFLOAT(x->x_sfinfo + 3, av_get_bytes_per_sample(x->x_stream_ctx->sample_fmt) * 8);
    x->x_nchans = nch;
    x->x_nsamps = output_index;
    for(unsigned int ch = 0; ch < nch; ch++)
        av_free(x_out[ch]);
    av_free(x_out);
    av_channel_layout_uninit(&x->x_layout);
    avcodec_free_context(&x->x_stream_ctx);
    avformat_close_input(&x->x_ic);
    swr_free(&swr);
    return(NULL);
}

void sfload_check_done(t_sfload* x){ // result clock
    if(x->x_result_ready){
        sfload_update_arrays(x);
        x->x_result_ready = 0;
        outlet_list(x->x_info_outlet, &s_, 4, x->x_sfinfo);
    }
    else
        clock_delay(x->x_result_clock, 20);
}

static int sfload_is_network_protocol(const char *filename){
    const char *protocols[] = { "http://", "https://", "tcp://", "ftp://", "sftp://", "rtsp://", "rtmp://", "udp://", "data://", "gopher://", "ws://", "wss://" };
    size_t num_protocols = sizeof(protocols) / sizeof(protocols[0]);
    for(size_t i = 0; i < num_protocols; i++){
        if(!strncmp(filename, protocols[i], strlen(protocols[i])))
            return(1); // Match found, it's a network protocol
    }
    return(0); // Not a network protocol
}

static int sfload_find_url(t_sfload *x, t_symbol* file, char* dir_out){
    if(sfload_is_network_protocol(file->s_name)){
        strcpy(dir_out, file->s_name);
        return(1);
    }
    else
        return(0);
}

static int sfload_find_file(t_sfload *x, t_symbol* file, char* dir_out){
    static char fname[MAXPDSTRING];
    char *bufptr;
    int fd = canvas_open(x->x_canvas, file->s_name, "", fname, &bufptr, MAXPDSTRING, 1);
    if(fd < 0){
        pd_error(x, "[sfload] file '%s' not found", file->s_name);
        return(0);
    }
    else{
        if(bufptr > fname)
            bufptr[-1] = '/';
        strcpy(dir_out, fname);
    }
    return(1);
}

void sfload_download(t_sfload* x, t_symbol* s, int ac, t_atom* av){
    if(x->x_arr_name == NULL){
        pd_error(x, "[sfload]: No array set");
        return;
    }
    if(!ac){
        pd_error(x, "[sfload]: no filename given to download");
        return;
    }
    t_symbol *path = NULL;
    if(av[0].a_type == A_SYMBOL)
        path = atom_getsymbol(av);
    else{
        pd_error(x, "[sfload]: 1st argument of 'load' must be a symbol");
        return;
    }
    x->x_ch = -1, x->x_arr_size = -1, x->x_onset = 0;
    if(ac >= 2 && av[1].a_type == A_FLOAT){
        x->x_ch = atom_getint(av+1);
        if(x->x_ch < 0)
            x->x_ch = -1;
    }
    if(ac >= 3 && av[2].a_type == A_FLOAT)
        x->x_arr_size = (long)atom_getint(av+2);
    if(ac >= 4 && av[3].a_type == A_FLOAT)
        x->x_onset = atom_getint(av+3);
    if(x->x_onset < 0)
        x->x_onset = 0;
    int ch = x->x_ch == -1 ? 0 : x->x_ch;
    char channel_zero_name[MAXPDSTRING];
    snprintf(channel_zero_name, MAXPDSTRING, "%i-%s", ch, x->x_arr_name->s_name);
    if(!pd_findbyclass(x->x_arr_name, garray_class) && !pd_findbyclass(gensym(channel_zero_name), garray_class)){
        pd_error(x, "[sfload]: Array \"%s\" or \"%i-%s\" not found",
            x->x_arr_name->s_name, ch, x->x_arr_name->s_name);
        return;
    }
    if(!sfload_find_url(x, path, x->x_path))
        return;
    if(x->x_threaded){
        if(pthread_create(&x->x_process_thread, NULL, sfload_read_audio_threaded, x) != 0){
            pd_error(x, "[sfload]: Error creating thread");
            return;
        }
        x->x_thread_created = 1;
        clock_delay(x->x_result_clock, 20);
    }
    else{
        sfload_read_audio(x);
        sfload_update_arrays(x);
        outlet_list(x->x_info_outlet, &s_, 4, x->x_sfinfo);
    }
}

void sfload_load(t_sfload* x, t_symbol* s, int ac, t_atom* av){
    if(x->x_arr_name == NULL){
        pd_error(x, "[sfload]: No array set");
        return;
    }
    if(!ac){
        pd_error(x, "[sfload]: no filename given");
        return;
    }
    t_symbol *path = NULL;
    if(av[0].a_type == A_SYMBOL)
        path = atom_getsymbol(av);
    else{
        pd_error(x, "[sfload]: 1st argument of 'load' must be a symbol");
        return;
    }
    x->x_ch = -1, x->x_arr_size = -1, x->x_onset = 0;
    if(ac >= 2 && av[1].a_type == A_FLOAT){
        x->x_ch = atom_getint(av+1);
        if(x->x_ch < 0)
            x->x_ch = -1;
    }
    if(ac >= 3 && av[2].a_type == A_FLOAT)
        x->x_arr_size = (long)atom_getint(av+2);
    if(ac >= 4 && av[3].a_type == A_FLOAT)
        x->x_onset = atom_getint(av+3);
    if(x->x_onset < 0)
        x->x_onset = 0;
    int ch = x->x_ch == -1 ? 0 : x->x_ch;
    char channel_zero_name[MAXPDSTRING];
    snprintf(channel_zero_name, MAXPDSTRING, "%i-%s", ch, x->x_arr_name->s_name);
    if(!pd_findbyclass(x->x_arr_name, garray_class) && !pd_findbyclass(gensym(channel_zero_name), garray_class)){
        pd_error(x, "[sfload]: Array \"%s\" or \"%i-%s\" not found",
            x->x_arr_name->s_name, ch, x->x_arr_name->s_name);
        return;
    }
    if(!sfload_find_file(x, path, x->x_path))
        return;
    if(x->x_threaded){
        if(pthread_create(&x->x_process_thread, NULL, sfload_read_audio_threaded, x) != 0){
            pd_error(x, "[sfload]: Error creating thread");
            return;
        }
        x->x_thread_created = 1;
        clock_delay(x->x_result_clock, 20);
    }
    else{
        sfload_read_audio(x);
        sfload_update_arrays(x);
        outlet_list(x->x_info_outlet, &s_, 4, x->x_sfinfo);
    }
}

void sfload_threaded(t_sfload* x, t_floatarg f){
    x->x_threaded = (f != 0);
}

void sfload_set(t_sfload* x, t_symbol* s){
    x->x_arr_name = s;
}

static void sfload_free(t_sfload *x){
    clock_free(x->x_result_clock);
    if(x->x_thread_created) pthread_join(x->x_process_thread, NULL);
    av_packet_free(&x->x_pkt);
    av_frame_free(&x->x_frm);
    for(int ch = 0; ch < 64; ch++)
        free(x->x_all_out[ch]);
}

static void *sfload_new(t_symbol *s, int ac, t_atom *av){
    t_sfload *x = (t_sfload *)pd_new(sfload_class);
    x->x_arr_name = NULL;
    x->x_threaded = 0;
    if(ac && atom_getsymbol(av) == gensym("-t")){
        x->x_threaded = 1;
        av++, ac--;
    }
    if(ac && av->a_type == A_SYMBOL)
        x->x_arr_name = atom_getsymbol(av);
    x->x_pkt = av_packet_alloc();
    x->x_frm = av_frame_alloc();
    x->x_stream_ctx = NULL;
    x->x_ic = NULL;
    for(int ch = 0; ch < 64; ch++)
        x->x_all_out[ch] = malloc(sizeof(t_sample));
    x->x_canvas = canvas_getcurrent();
    x->x_result_ready = 0;
    x->x_thread_created = 0;
    x->x_result_clock = clock_new(x, (t_method)sfload_check_done);
    x->x_info_outlet = outlet_new(&x->x_obj, &s_list);
    return(x);
}

void sfload_setup(void){
    sfload_class = class_new(gensym("sfload"), (t_newmethod)sfload_new,
        (t_method)sfload_free, sizeof(t_sfload), 0, A_GIMME, 0);
    class_addmethod(sfload_class, (t_method)sfload_load, gensym("load"), A_GIMME, 0);
    class_addmethod(sfload_class, (t_method)sfload_download, gensym("download"), A_GIMME, 0);
    class_addmethod(sfload_class, (t_method)sfload_set, gensym("set"), A_SYMBOL, 0);
    class_addmethod(sfload_class, (t_method)sfload_threaded, gensym("threaded"), A_FLOAT, 0);
}
