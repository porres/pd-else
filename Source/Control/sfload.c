// tim schoen and porres 2024

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <pthread.h>
#include <m_pd.h>

#define FRAMES 4096

static t_class *sfload_class;

typedef struct _sfload {
    t_object        x_obj;
    t_outlet       *x_info_outlet;
    AVCodecContext *x_stream_ctx;
    int            x_stream_idx;
    AVPacket       *x_pkt;
    AVFrame        *x_frm;
    SwrContext     *x_swr;
    AVFormatContext *x_ic;
    AVChannelLayout x_layout;
    unsigned int    x_channel;
    t_sample       *x_all_out;
    t_canvas       *x_canvas;
    t_symbol       *x_arr_name;
    pthread_t       x_process_thread;
    _Atomic int     x_result_ready;
    t_clock        *x_result_clock;
    char            x_path[MAXPDSTRING];
    t_atom          x_sfinfo[6];
}t_sfload;

void* sfload_read_audio(void *arg){ // read audio into array
    t_sfload *x = (t_sfload*)arg;
    x->x_ic = avformat_alloc_context();
    x->x_ic->probesize = 128;
    x->x_ic->max_probe_packets = 1;
    if(avformat_open_input(&x->x_ic, x->x_path, NULL, NULL) != 0){
        pd_error(x, "[sfload]: Could not open file '%s'\n", x->x_path);
        return (NULL);
    }
    if(avformat_find_stream_info(x->x_ic, NULL) < 0){
        pd_error(x, "[sfload]: Could not find stream information\n");
        return (NULL);
    }
    int audio_stream_index = -1;
    for(unsigned int i = 0; i < x->x_ic->nb_streams; i++){
        if(x->x_ic->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
            audio_stream_index = i;
            break;
        }
    }
    if(audio_stream_index == -1){
        pd_error(x, "[sfload]: Could not find any audio stream in the file\n");
        return (NULL);
    }
    x->x_stream_idx = audio_stream_index;
    AVCodecParameters *codec_parameters = x->x_ic->streams[audio_stream_index]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codec_parameters->codec_id);
    if(!codec){
        pd_error(x, "[sfload]: Codec not found\n");
        return (NULL);
    }
    x->x_stream_ctx = avcodec_alloc_context3(codec);
    if(!x->x_stream_ctx){
        pd_error(x, "[sfload]: Could not allocate audio codec context\n");
        return (NULL);
    }
    if(avcodec_parameters_to_context(x->x_stream_ctx, codec_parameters) < 0){
        pd_error(x, "[sfload]: Could not copy codec parameters to context\n");
        return (NULL);
    }
    if(avcodec_open2(x->x_stream_ctx, codec, NULL) < 0){
        pd_error(x, "[sfload]: Could not open codec\n");
        return (NULL);
    }
    AVChannelLayout layout;
    if(x->x_stream_ctx->ch_layout.u.mask)
        av_channel_layout_from_mask(&layout, x->x_stream_ctx->ch_layout.u.mask);
    else
        av_channel_layout_default(&layout, x->x_stream_ctx->ch_layout.nb_channels);
    unsigned int nch = layout.nb_channels;
    swr_alloc_set_opts2(&x->x_swr, &layout, AV_SAMPLE_FMT_FLT, x->x_stream_ctx->sample_rate,
        &layout, x->x_stream_ctx->sample_fmt, x->x_stream_ctx->sample_rate, 0, NULL);
    if(!x->x_swr || swr_init(x->x_swr) < 0){
        pd_error(x, "[sfload]: Could not initialize the resampling context\n");
        return (NULL);
    }

    AVDictionaryEntry *loop_start_entry = av_dict_get(x->x_ic->metadata, "loop_start", NULL, 0);
    AVDictionaryEntry *loop_end_entry = av_dict_get(x->x_ic->metadata, "loop_end", NULL, 0);

    t_sample* x_out = (t_sample*)av_mallocz(nch * FRAMES * sizeof(t_sample));
    int output_index = 0;
    while(av_read_frame(x->x_ic, x->x_pkt) >= 0) {
        if(x->x_pkt->stream_index == x->x_stream_idx) {
            if(avcodec_send_packet(x->x_stream_ctx, x->x_pkt) < 0
            || avcodec_receive_frame(x->x_stream_ctx, x->x_frm) < 0)
                continue;
            int samples_converted = swr_convert(x->x_swr, (uint8_t **)&x_out, FRAMES,
                (const uint8_t **)x->x_frm->extended_data, x->x_frm->nb_samples);
            if(samples_converted < 0){
                pd_error(x, "[sfload]: Error converting samples\n");
                continue;
            }
            x->x_all_out = realloc(x->x_all_out, (output_index + (samples_converted / nch)) * sizeof(t_sample));
            int n = 0;
            while(n < samples_converted){
                for(unsigned int ch = 0; ch < nch; ch++){
                    if(ch == x->x_channel)
                        x->x_all_out[output_index] = x_out[n];
                    n++;
                }
                output_index++;
            }
        }
        av_packet_unref(x->x_pkt);
    }

    SETFLOAT(x->x_sfinfo, x->x_stream_ctx->sample_rate);
    SETFLOAT(x->x_sfinfo + 1, nch);
    SETFLOAT(x->x_sfinfo + 2, av_get_bytes_per_sample(x->x_stream_ctx->sample_fmt) * 8);
    SETFLOAT(x->x_sfinfo + 3, output_index);
    SETFLOAT(x->x_sfinfo + 4, loop_start_entry ? atoi(loop_start_entry->value) : 0);
    SETFLOAT(x->x_sfinfo + 5, loop_end_entry ? atoi(loop_end_entry->value) : 0);

    x->x_result_ready = output_index;
    av_free(x_out);
    return (NULL);
}

void sfload_check_done(t_sfload* x){ // result clock
    if(x->x_result_ready){
        t_garray* garray = (t_garray*)pd_findbyclass(x->x_arr_name, garray_class);
        garray_resize_long(garray, x->x_result_ready);
        t_word* vec = ((t_word*)garray_vec(garray));
        for(int i = 0; i < x->x_result_ready; i++)
            vec[i].w_float = x->x_all_out[i];
        garray_redraw(garray);
        x->x_result_ready = 0;
        free(x->x_all_out);
        x->x_all_out = NULL;
        outlet_list(x->x_info_outlet, &s_, 6, x->x_sfinfo);
    }
    else
        clock_delay(x->x_result_clock, 20);
}

static void sfload_find_file(t_sfload *x, t_symbol* file, char* dir_out){
    static char fname[MAXPDSTRING];
    char *bufptr;
    int fd = canvas_open(x->x_canvas, file->s_name, "", fname, &bufptr, MAXPDSTRING, 1);
    if(fd < 0){
        post("[sfload] file '%s' not found", file->s_name);
        return;
    }
    else {
        if(bufptr > fname) bufptr[-1] = '/';
        strcpy(dir_out, fname);
    }
}

void sfload_load(t_sfload* x, t_symbol* s, int ac, t_atom* av){
    if(x->x_arr_name == NULL){
        pd_error(x, "[sfload]: No array set\n");
        return;
    }
    if(!pd_findbyclass(x->x_arr_name, garray_class)){
        pd_error(x, "[sfload]: Array not found\n");
        return;
    }
    if(!ac){
        pd_error(x, "[sfload]: no filename given\n");
        return;
    }
    t_symbol* path = NULL;
    if(ac == 1 && av[0].a_type == A_SYMBOL)
        path = atom_getsymbol(av);
    if(ac >= 2 && av[1].a_type == A_FLOAT)
        x->x_channel = atom_getfloat(av + 1);
    else
        x->x_channel = 0;
    if(!path){
        pd_error(x, "[sfload]: Invalid arguments for 'load' message\n");
        return;
    }
    char dir[MAXPDSTRING], file[MAXPDSTRING];
    sfload_find_file(x, path, dir, file);
    snprintf(x->x_path, MAXPDSTRING, "%s/%s", dir, file);
    if(pthread_create(&x->x_process_thread, NULL, sfload_read_audio, x) != 0){
        pd_error(x, "[sfload]: Error creating thread\n");
        return;
    }
    clock_delay(x->x_result_clock, 20);
}

void sfload_set(t_sfload* x, t_symbol* s){
    x->x_arr_name = s;
}

static void sfload_free(t_sfload *x){
    clock_free(x->x_result_clock);
    pthread_join(x->x_process_thread, NULL);
    av_channel_layout_uninit(&x->x_layout);
    avcodec_free_context(&x->x_stream_ctx);
    avformat_close_input(&x->x_ic);
    av_packet_free(&x->x_pkt);
    av_frame_free(&x->x_frm);
    swr_free(&x->x_swr);
}

static void *sfload_new(t_symbol *s, int ac, t_atom *av){
    t_sfload *x = (t_sfload *)pd_new(sfload_class);
    x->x_arr_name = NULL;
    if(ac >= 1 && av[0].a_type == A_SYMBOL)
        x->x_arr_name = atom_getsymbol(av);
    x->x_pkt = av_packet_alloc();
    x->x_frm = av_frame_alloc();
    x->x_ic = NULL;
    x->x_all_out = NULL;
    x->x_canvas = canvas_getcurrent();
    x->x_result_ready = 0;
    x->x_result_clock = clock_new(x, (t_method)sfload_check_done);
    x->x_info_outlet = outlet_new(x, &s_list);
    return(x);
}

void sfload_setup(void) {
    sfload_class = class_new(gensym("sfload"), (t_newmethod)sfload_new,
        (t_method)sfload_free, sizeof(t_sfload), 0, A_GIMME, 0);
    class_addmethod(sfload_class, (t_method)sfload_load, gensym("load"), A_GIMME, 0);
    class_addmethod(sfload_class, (t_method)sfload_set, gensym("set"), A_SYMBOL, 0);
}
