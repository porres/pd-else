// tim schoen and porres 2024-2025

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <else_alloca.h>
#include <m_pd.h>
#include <libavutil/error.h>  // for av_strerror()

#define FRAMES 4096

t_canvas *glist_getcanvas(t_glist *x);

typedef struct _playlist{
    t_symbol  **arr;      // m3u list of tracks
    t_symbol   *dir;      // starting directory
    int         size;     // size of the list
    int         max;      // size of the memory allocation
}t_playlist;

typedef struct _playfile{
    t_object        x_obj;
    t_sample      **x_outs;
    unsigned char   x_play;       // play/pause toggle
    unsigned char   x_open;       // true when a file has been successfully opened
    unsigned int    x_nch;        // number of channels
    t_outlet       *x_bang_out;   // outputs bang when finished
    AVCodecContext *x_stream_ctx;
    int            x_stream_idx;
    AVPacket       *x_pkt;
    AVFrame        *x_frm;
    SwrContext     *x_swr;
    AVFormatContext *x_ic;
    AVChannelLayout x_layout;
    t_playlist      x_plist;
    t_canvas       *x_canvas;
    t_sample       *x_buffer;
    int             x_buff_idx;
    int             x_bufsize;
    t_float         x_speed;
    int             x_loop;
    int             x_mc;
    int             x_n;
    int             x_sr;
    int             x_file_nch;
    t_symbol       *x_play_next;
    t_symbol       *x_openpanel_sym;
}t_playfile;

// ---------------------- playlist ------------------------

typedef const char *err_t;

// prevents self-referencing m3u's from causing an infinite loop
static int depth;

#define M3U_MAIN(deeper, increment) \
    char line[MAXPDSTRING]; \
    while (fgets(line, MAXPDSTRING, fp) != NULL) { \
        line[strcspn(line, "\r\n")] = '\0'; \
        int isabs = (line[0] == '/'); \
        if ((isabs ? 0 : dlen) + strlen(line) >= MAXPDSTRING) { \
            continue; \
        } \
        strcpy(dir + dlen, line); \
        char *ext = strrchr(line, '.'); \
        if (ext && !strcmp(ext + 1, "m3u")) { \
            char *fname = strrchr(line, '/'); \
            int len = (fname) ? ++fname - line : 0; \
            FILE *m3u = fopen(dir, "r"); \
            if (m3u) { \
                if (depth < 0x100) { \
                    depth++; \
                    (deeper); \
                    depth--; \
                } \
                fclose(m3u); \
            } \
        } else { \
            (increment); \
        } \
    }

static int m3u_size(FILE *fp, char *dir, int dlen){
    int size = 0;
    M3U_MAIN(size += m3u_size(m3u, dir, dlen + len), size++)
    return(size);
}

static int playlist_fill(t_playlist *pl, FILE *fp, char *dir, int dlen, int i) {
    int oldlen = strlen(pl->dir->s_name);
    M3U_MAIN (i = playlist_fill(pl, m3u, dir, dlen + len, i), pl->arr[i++] = gensym(dir + oldlen))
    return(i);
}

static inline err_t playlist_m3u(t_playlist *pl, t_symbol *s){
    FILE *fp = fopen(s->s_name, "r");
    if(!fp)
        return ("Could not open m3u");
    depth = 1;
    char dir[MAXPDSTRING];
    strcpy(dir, pl->dir->s_name);
    int size = m3u_size(fp, dir, strlen(dir));
    if(size < 1)
        return ("Playlist is empty");
    if(size > pl->max){
        pl->arr = (t_symbol **)resizebytes(pl->arr,
            pl->max * sizeof(t_symbol *), size * sizeof(t_symbol *));
        pl->max = size;
    }
    pl->size = size;
    rewind(fp);
    playlist_fill(pl, fp, dir, strlen(pl->dir->s_name), 0);
    fclose(fp);
    return(0);
}

static void playfile_seek(t_playfile *x, t_float f){
    if(!x->x_open)
        return;
    avformat_seek_file(x->x_ic, -1, 0, f * 1000, x->x_ic->duration, 0);
    AVRational ratio = x->x_ic->streams[x->x_stream_idx]->time_base;
    x->x_frm->pts = f * ratio.den / (ratio.num * 1000);
    swr_init(x->x_swr);
    // avcodec_flush_buffers(x->x_stream_ctx); // doesn't always flush properly
    avcodec_free_context(&x->x_stream_ctx);
    x->x_stream_ctx = avcodec_alloc_context3(NULL);
    avcodec_parameters_to_context(x->x_stream_ctx, x->x_ic->streams[x->x_stream_idx]->codecpar);
    x->x_stream_ctx->pkt_timebase = x->x_ic->streams[x->x_stream_idx]->time_base;
    const AVCodec *codec = avcodec_find_decoder(x->x_stream_ctx->codec_id);
    avcodec_open2(x->x_stream_ctx, codec, NULL);
    x->x_bufsize = 0;
}

static inline err_t playfile_context(t_playfile *x){
    int i = x->x_stream_idx;
    avcodec_free_context(&x->x_stream_ctx);
    x->x_stream_ctx = avcodec_alloc_context3(NULL);
    if(!x->x_stream_ctx)
        return ("Failed to allocate AVCodecContext");
    if(avcodec_parameters_to_context(x->x_stream_ctx, x->x_ic->streams[i]->codecpar) < 0)
        return ("Failed to fill codec with parameters");
    x->x_stream_ctx->pkt_timebase = x->x_ic->streams[i]->time_base;
    const AVCodec *codec = avcodec_find_decoder(x->x_stream_ctx->codec_id);
    if(!codec)
        return ("Codec not found");
    if(avcodec_open2(x->x_stream_ctx, codec, NULL) < 0)
        return ("Failed to open codec");
    return(0);
}

static AVChannelLayout playfile_layout(t_playfile *x){
    AVChannelLayout layout_in;
    if(x->x_stream_ctx->ch_layout.u.mask)
        av_channel_layout_from_mask(&layout_in, x->x_stream_ctx->ch_layout.u.mask);
    else
        av_channel_layout_default(&layout_in, x->x_stream_ctx->ch_layout.nb_channels);
    return(layout_in);
}

static err_t playfile_reset(void *y){
    t_playfile *x = (t_playfile *)y;
    t_playfile *b = x;
    swr_free(&x->x_swr);
    AVChannelLayout layout_in = playfile_layout(b);
    swr_alloc_set_opts2(&x->x_swr, &x->x_layout, AV_SAMPLE_FMT_FLT,
        x->x_stream_ctx->sample_rate, &layout_in, x->x_stream_ctx->sample_fmt,
        x->x_stream_ctx->sample_rate, 0, NULL);
    if(swr_init(x->x_swr) < 0)
        return("SWResampler initialization failed");
    return(0);
}

static err_t playfile_load(t_playfile *x, int index){
    char url[MAXPDSTRING];
    const char *fname = x->x_plist.arr[index]->s_name;
    if(fname[0] == '/') // absolute path
        strcpy(url, fname);
    else{
        size_t dir_len = strlen(x->x_plist.dir->s_name);
        // Copy directory name
        strcpy(url, x->x_plist.dir->s_name);
        // If directory does NOT end with '/', append one
        if(dir_len > 0 && x->x_plist.dir->s_name[dir_len - 1] != '/')
            strcat(url, "/");
        strcat(url, fname); // Append the file name
    }
    if(!x->x_ic || !x->x_ic->url || strncmp(x->x_ic->url, url, MAXPDSTRING) != 0){
        avformat_close_input(&x->x_ic);
        x->x_ic = avformat_alloc_context();
        x->x_ic->probesize = 128;
        x->x_ic->max_probe_packets = 1;
        int ret = avformat_open_input(&x->x_ic, url, NULL, NULL);
        if(ret < 0){
            char errbuf[256];
            av_strerror(ret, errbuf, sizeof(errbuf));
            post("play.file~: Failed to open input stream '%s': %s", url, errbuf);
            return("Failed to open input stream");
        }
        if(avformat_find_stream_info(x->x_ic, NULL) < 0){
            post("play.file~: Failed to find stream information for '%s'", url);
            return "Failed to find stream information";
        }
        x->x_ic->seek2any = 1;
        int i = -1;
        for(unsigned int j = x->x_ic->nb_streams; j--;){
            if(x->x_ic->streams[j]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
                i = j;
                break;
            }
        }
        x->x_stream_idx = i;
        if(i < 0){
            post("play.file~: No audio stream found in '%s'", url);
            return("No audio stream found");
        }
    }
    err_t err_msg = playfile_context(x);
    if(err_msg)
        return err_msg;
    x->x_frm->pts = 0;
    x->x_sr = x->x_stream_ctx->sample_rate;
    int pd_sr = sys_getsr();
    if(x->x_sr != pd_sr)
        post("[play.file~] Warning: file's sample rate (%d Hz) differs from Pd's (%d Hz)", x->x_sr, pd_sr);
    int file_nch = x->x_stream_ctx->ch_layout.nb_channels;
/*    if(file_nch != x->x_file_nch && x->x_mc){
        x->x_file_nch = file_nch;
        canvas_update_dsp();
    }*/
    return(playfile_reset(x));
}

static void playfile_pause(t_playfile *x){
    x->x_play = 0;
}

static void playfile_continue(t_playfile *x){
    x->x_play = 1;
}

static void playfile_start(t_playfile *x, t_float f, t_float ms){
    int track = f;
    err_t err_msg = "";
    if(0 < track && track <= x->x_plist.size){
        if((err_msg = playfile_load(x, track - 1)))
            pd_error(x, "[play.file~] 'base start': %s.", err_msg);
        playfile_seek(x, ms);
        x->x_open = !err_msg;
    }
    else
        playfile_seek(x, 0);
    x->x_play = !err_msg;
}

/*static int playfile_is_network_protocol(const char *filename) {
    const char *protocols[] = { "http://", "https://", "tcp://", "ftp://", "sftp://", "rtsp://", "rtmp://", "udp://", "data://", "gopher://", "ws://", "wss://" };
    size_t num_protocols = sizeof(protocols) / sizeof(protocols[0]);
    for(size_t i = 0; i < num_protocols; i++){
        if(strncmp(filename, protocols[i], strlen(protocols[i])) == 0)
            return(1); // Match found, it's a network protocol
    }
    return(0); // Not a network protocol
}*/

static int playfile_find_file(t_playfile *x, t_symbol* file, char* dir_out, char** filename_out){
/*    if(playfile_is_network_protocol(file->s_name)){
        strcpy(dir_out, file->s_name);
        *filename_out = NULL;
        return(2);
    }*/
    char *bufptr;
    int fd = canvas_open(x->x_canvas, file->s_name, "", dir_out, filename_out, MAXPDSTRING, 1);
    if(fd < 0){
        pd_error(x, "[play.file~] file '%s' not found", file->s_name);
        return(0);
    }
    sys_close(fd);
    return(1);
}

static void playfile_openpanel_callback(t_playfile *x, t_symbol *s, int ac, t_atom *av){
    if(ac == 1 && av->a_type == A_SYMBOL){
        err_t err_msg = 0;
        t_symbol* path = atom_getsymbol(av);
        const char *path_str = path->s_name;
        t_playlist *pl = &x->x_plist;
        char dir[MAXPDSTRING];
        const char *fname = strrchr(path_str, '/');
        if(fname){
            int len = ++fname - path_str;
            strncpy(dir, path_str, len);
            dir[len] = '\0';
        }
        else{
            fname = path_str;
            strcpy(dir, "./");
        }
        pl->dir = gensym(dir);
        const char *ext = strrchr(path_str, '.');
        if(ext && !strcmp(ext + 1, "m3u"))
            err_msg = playlist_m3u(pl, s);
        else{
            pl->size = 1;
            pl->arr[0] = gensym(fname);
        }
        if(err_msg || (err_msg = playfile_load(x, 0)))
            pd_error(x, "[play.file~]: open: %li", (long)err_msg);
        x->x_open = !err_msg;
    }
}

/*static void playfile_stream(t_playfile *x, t_symbol *s){
    x->x_play = 0;
    err_t err_msg = 0;
    const char *url = s->s_name;
    if(strlen(url) >= MAXPDSTRING){
        err_msg = "URL is too long";
        pd_error(x, "[play.file~]: %s.", err_msg);
    }
    else if(!strncmp(url, "http:", 5) ||
    !strncmp(url, "https:", 6) ||
    !strncmp(url, "ftp:", 4)){
        t_playlist *pl = &x->x_plist;
        char dir[MAXPDSTRING];
        const char *fname = strrchr(url, '/');
        if(fname){
            int len = ++fname - url;
            strncpy(dir, url, len);
            dir[len] = '\0';
        }
        else{
            fname = url;
            strcpy(dir, "./");
        }
        pl->dir = gensym(dir);
        const char *ext = strrchr(url, '.');
        if(ext && !strcmp(ext + 1, "m3u"))
            err_msg = playlist_m3u(pl, s);
        else{
            pl->size = 1;
            pl->arr[0] = gensym(fname);
        }
        if(err_msg || (err_msg = playfile_load(x, 0)))
            pd_error(x, "[play.file~]: open: %s.", err_msg);
        x->x_open = !err_msg;
    }
}*/

static void playfile_open(t_playfile *x, t_symbol *s, int ac, t_atom *av){
    x->x_play = 0;
    err_t err_msg = 0;
    if(ac == 0){
        pdgui_vmess("pdtk_openpanel", "ssic", x->x_openpanel_sym->s_name,
            canvas_getdir(x->x_canvas)->s_name, 0, glist_getcanvas(x->x_canvas));
    }
    else if(av->a_type == A_SYMBOL){
        const char *sym = av->a_w.w_symbol->s_name;
        if(strlen(sym) >= MAXPDSTRING){
            err_msg = "File path is too long";
            pd_error(x, "[play.file~]: %s.", err_msg);
        }
        else{
            char dirname[MAXPDSTRING];
            char* filename = NULL;
            if(!playfile_find_file(x, av->a_w.w_symbol, dirname, &filename))
                return;
            t_playlist *pl = &x->x_plist;
            pl->dir = gensym(dirname);
            const char *ext = strrchr(filename, '.');
            if(ext && !strcmp(ext + 1, "m3u"))
                err_msg = playlist_m3u(pl, gensym(filename));
            else{
                pl->size = 1;
                pl->arr[0] = gensym(filename);
            }
            if(err_msg || (err_msg = playfile_load(x, 0)))
                pd_error(x, "[play.file~]: open: %s.", err_msg);
            x->x_open = !err_msg;
        }
    }
}

static void playfile_click(t_playfile *x, t_floatarg xpos,
t_floatarg ypos, t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    xpos = ypos = shift = ctrl = alt = 0;
    playfile_open(x, NULL, 0, NULL);
}

static void playfile_float(t_playfile *x, t_float f){
    playfile_start(x, f, 0);
}

static void playfile_bang(t_playfile *x){
    playfile_float(x, 1);
}

static void playfile_stop(t_playfile *x){
    playfile_float(x, 0);
}

static t_class *playfile_class;

static void playfile_loop(t_playfile *x, t_float f){
    x->x_loop = f;
}

static void playfile_set(t_playfile *x, t_symbol* s){
    if(x->x_open && x->x_play)
        x->x_play_next = s;
    else
        playfile_open(x, gensym("open"), 1, &(t_atom){.a_type = A_SYMBOL,
            .a_w = { .w_symbol = s }});
}

static t_int *playfile_perform(t_int *w){
    t_playfile *x = (t_playfile *)(w[1]);
    unsigned nch = x->x_nch;
    t_sample** outs = ALLOCA(t_sample*, nch);
    for(int j = 0; j < nch; j++)
        outs[j] = x->x_outs[j];
    int n = x->x_n, i = 0;
    if(x->x_play){
        while(i < n){
            if(x->x_buff_idx >= x->x_bufsize){
                x->x_buff_idx = x->x_bufsize = 0;
                while(av_read_frame(x->x_ic, x->x_pkt) >= 0){
                    if(x->x_pkt->stream_index == x->x_stream_idx){
                        if(avcodec_send_packet(x->x_stream_ctx, x->x_pkt) < 0
                        || avcodec_receive_frame(x->x_stream_ctx, x->x_frm) < 0){
                            av_packet_unref(x->x_pkt);
                            continue;
                        }
                        int samples_converted = swr_convert(x->x_swr, (uint8_t **)&x->x_buffer, FRAMES,
                            (const uint8_t **)x->x_frm->extended_data, x->x_frm->nb_samples);
                        x->x_bufsize = samples_converted;
                        if(samples_converted < 0){
                            fprintf(stderr, "Error converting samples\n");
                            x->x_bufsize = 0;
                            av_packet_unref(x->x_pkt);
                            continue;
                        }
                        x->x_bufsize = samples_converted * nch;
                        av_packet_unref(x->x_pkt);
                        break;
                    }
                    av_packet_unref(x->x_pkt);
                }
                if(x->x_bufsize == 0){
                    if(x->x_play){
                        x->x_play = 0;
                        outlet_bang(x->x_bang_out);
                    }
                    if(x->x_loop){
                        if(x->x_play_next){
                            playfile_open(x, gensym("open"), 1, &(t_atom){.a_type = A_SYMBOL, .a_w = { .w_symbol = x->x_play_next }});
                            x->x_play_next = NULL;
                        }
                        playfile_seek(x, 0.0f);
                        x->x_play = 1;
                    }
                    else{
                        if(x->x_play_next){
                            playfile_open(x, gensym("open"), 1, &(t_atom){.a_type = A_SYMBOL, .a_w = { .w_symbol = x->x_play_next }});
                            playfile_stop(x);
                            x->x_play_next = NULL;
                        }
                        playfile_seek(x, 0);
                        goto silence;
                    }
                }
            }
            while(i < n && x->x_buff_idx < x->x_bufsize){
                for(unsigned int ch = 0; ch < nch; ch++)
                    outs[ch][i] = x->x_buffer[x->x_buff_idx + ch];
                x->x_buff_idx += nch;
                i++;
            }
        }
    }
    else{
        while(i < n){
        silence:
            for(int ch = nch; ch--;)
                outs[ch][i] = 0.0f;
            i++;
        }
    }
    FREEA(outs, t_sample*, nch);
    return(w+2);
}

static t_int *playfile_perform_mc(t_int *w){
    t_playfile *x = (t_playfile *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    unsigned int nch = x->x_nch;
    int n = x->x_n;
    int i = 0;
    if(x->x_play){
        while(i < n){
            if(x->x_buff_idx >= x->x_bufsize){
                x->x_buff_idx = x->x_bufsize = 0;
                while(av_read_frame(x->x_ic, x->x_pkt) >= 0){
                    if(x->x_pkt->stream_index == x->x_stream_idx){
                        if(avcodec_send_packet(x->x_stream_ctx, x->x_pkt) < 0 ||
                           avcodec_receive_frame(x->x_stream_ctx, x->x_frm) < 0){
                            av_packet_unref(x->x_pkt);
                            continue;
                        }
                        int samples_converted =
                            swr_convert(x->x_swr,
                                (uint8_t **)&x->x_buffer, FRAMES,
                                (const uint8_t **)x->x_frm->extended_data,
                                x->x_frm->nb_samples);
                        if(samples_converted < 0){
                            fprintf(stderr, "Error converting samples\n");
                            x->x_bufsize = 0;
                            av_packet_unref(x->x_pkt);
                            continue;
                        }
                        x->x_bufsize = samples_converted * nch;
                        av_packet_unref(x->x_pkt);
                        break;
                    }
                    av_packet_unref(x->x_pkt);
                }
                if(x->x_bufsize == 0){
                    if(x->x_play){
                        x->x_play = 0;
                        outlet_bang(x->x_bang_out);
                    }
                    if(x->x_loop){
                        if(x->x_play_next){
                            playfile_open(x, gensym("open"), 1,
                                &(t_atom){.a_type=A_SYMBOL,
                                .a_w={.w_symbol=x->x_play_next}});
                            x->x_play_next = NULL;
                        }
                        playfile_seek(x, 0.0f);
                        x->x_play = 1;
                    }
                    else{
                        if(x->x_play_next){
                            playfile_open(x, gensym("open"), 1,
                                &(t_atom){.a_type=A_SYMBOL,
                                .a_w={.w_symbol=x->x_play_next}});
                            playfile_stop(x);
                            x->x_play_next = NULL;
                        }
                        playfile_seek(x, 0);
                        goto silence;
                    }
                }
            }
            while(i < n && x->x_buff_idx < x->x_bufsize){
                for(unsigned ch = 0; ch < nch; ch++)
                    out[ch*n + i] = x->x_buffer[x->x_buff_idx + ch];
                x->x_buff_idx += nch;
                i++;
            }
        }
    }
    else{
        while(i < n){
        silence:
            for(unsigned ch = 0; ch < nch; ch++)
                out[ch*n + i] = 0.0f;
            i++;
        }
    }
    return(w+3);
}

static void playfile_dsp(t_playfile *x, t_signal **sp){
    x->x_n =  sp[0]->s_n;
    int sr = sp[0]->s_sr;
    if(x->x_sr > 0 && x->x_sr != sr)
        post("[play.file~] Warning: file's sample rate (%d Hz) differs from Pd's (%d Hz)",
             x->x_sr, sr);
    if(x->x_mc){
/*        if(x->x_nch != x->x_file_nch){
            x->x_buffer = (t_sample *)resizebytes(x->x_buffer,
                x->x_nch * FRAMES * sizeof(t_sample), x->x_file_nch * FRAMES * sizeof(t_sample));
            x->x_nch = x->x_file_nch;
        }
        signal_setmultiout(&sp[0], x->x_file_nch);*/
        signal_setmultiout(&sp[0], x->x_nch);
        dsp_add(playfile_perform_mc, 2, x, sp[0]->s_vec);
    }
    else{
        for(int i = x->x_nch; i--;){
            signal_setmultiout(&sp[i], 1);
            x->x_outs[i] = sp[i]->s_vec;
        }
        dsp_add(playfile_perform, 1, x);
    }
}

static void playfile_free(t_playfile *x){
    av_channel_layout_uninit(&x->x_layout);
    avcodec_free_context(&x->x_stream_ctx);
    avformat_close_input(&x->x_ic);
    av_packet_free(&x->x_pkt);
    av_frame_free(&x->x_frm);
    swr_free(&x->x_swr);
    t_playlist *pl = &x->x_plist;
    freebytes(pl->arr, pl->max * sizeof(t_symbol *));
    freebytes(x->x_outs, x->x_nch * sizeof(t_sample *));
    freebytes(x->x_buffer, x->x_nch * sizeof(t_sample) * FRAMES);
    pd_unbind(&x->x_obj.ob_pd, x->x_openpanel_sym);
}

AVChannelLayout playfile_get_channel_layout_for_file(t_playfile* x, const char *dirname, const char *filename) {
    char input_path[MAXPDSTRING];
    snprintf(input_path, MAXPDSTRING, "%s/%s", dirname, filename);
    x->x_ic = avformat_alloc_context();
    x->x_ic->probesize = 128;
    x->x_ic->max_probe_packets = 1;
    if(avformat_open_input(&x->x_ic, input_path, NULL, NULL) != 0){
        fprintf(stderr, "Could not open input file '%s'\n", input_path);
        goto error;
    }
    // Retrieve stream information
    if(avformat_find_stream_info(x->x_ic, NULL) < 0){
        fprintf(stderr, "Could not find stream information\n");
        goto error;
    }
    int audio_stream_index = -1;
    for(unsigned int i = 0; i < x->x_ic->nb_streams; i++){
        if(x->x_ic->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
            audio_stream_index = i;
            break;
        }
    }
    if(audio_stream_index == -1){
        fprintf(stderr, "Could not find any audio stream in the file\n");
        goto error;
    }
    x->x_stream_idx = audio_stream_index;
    AVCodecParameters *codec_parameters = x->x_ic->streams[audio_stream_index]->codecpar;
    return(codec_parameters->ch_layout);
error:
    if(x->x_ic)
        avformat_close_input(&x->x_ic);
    AVChannelLayout l;
    av_channel_layout_default(&l, 1);
    return(l);
}

static void *playfile_new(t_symbol *s, int ac, t_atom *av){
    t_playfile *x = (t_playfile *)pd_new(playfile_class);
    x->x_canvas = canvas_getcurrent();
    x->x_open = x->x_play = 0;
    x->x_sr = -1;
    x->x_file_nch = 1;
    x->x_pkt = av_packet_alloc();
    x->x_frm = av_frame_alloc();
    x->x_ic = NULL;
    t_playlist *pl = &x->x_plist;
    pl->size = 0;
    pl->max = 1;
    pl->arr = (t_symbol **)getbytes(pl->max * sizeof(t_symbol *));
    int nch = 1;
    int filefound = 0;
    AVChannelLayout layout;
    int loop = 0, mc = 0, ncharg = 0;
    t_atom at[1];
    while(av->a_type == A_SYMBOL &&
    (av->a_w.w_symbol == gensym("-loop") ||
     av->a_w.w_symbol == gensym("-mc"))){
        s = av->a_w.w_symbol;
        if(s == gensym("-loop"))
            loop = 1;
        else
            mc = 1;
        ac--, av++;
    }
    if(!ac){
        uint64_t mask = 0;
        for(int ch = 0; ch < nch; ch++)
            mask |= (ch + 1);
        av_channel_layout_from_mask(&layout, mask);
    }
    else{ // ac
        if(av->a_type == A_FLOAT){ // Num channels from float arg
            if(!x->x_mc){
                ncharg = 1;
                nch = atom_getfloat(av);
                if(nch < 1)
                    nch = 1;
                uint64_t mask = 0;
                for(int ch = 0; ch < nch; ch++)
                    mask |= (ch + 1);
                av_channel_layout_from_mask(&layout, mask);
            }
            else // ignore arg if MC and give warning
                post("[play.file~] channels arg is ignore in -mc mode");
            ac--, av++;
        }
        if(ac && av->a_type == A_SYMBOL){
            char dir[MAXPDSTRING];
            char* file = NULL;
            t_symbol *sym =  atom_getsymbol(av);
            filefound = playfile_find_file(x, sym, dir, &file);
            if(filefound){
                SETSYMBOL(at, sym);
                if(!ncharg){ // Num channels from file
                    layout = playfile_get_channel_layout_for_file(x, dir, file);
                    x->x_file_nch = nch = layout.nb_channels;
                }
            }
            ac--, av++;
        }
    }
    // channel layout masking details: libavutil/channel_layout.h
    x->x_mc = mc;
    x->x_layout = layout;
    x->x_nch = nch;
    if(x->x_mc)
        outlet_new(&x->x_obj, &s_signal);
    else{
        x->x_outs = (t_sample **)getbytes(nch * sizeof(t_sample *));
        while(nch--)
            outlet_new(&x->x_obj, &s_signal);
    }
    x->x_bang_out = outlet_new(&x->x_obj, 0);
    if(filefound)
        playfile_open(x, gensym("open"), 1, at);
    if(ac){  // Autostart/Loops args
        if(av->a_type != A_FLOAT)
            goto errstate;
        playfile_start(x, atom_getfloat(av) != 0, 0.0f);
        ac--, av++;
        if(ac){ // Loop argument
            if(av->a_type != A_FLOAT){
            errstate:
                pd_error(x, "[play.file~] improper args");
                return(NULL);
            }
            loop = atom_getfloat(av) != 0;
        }
    }
    x->x_speed = 1;
    x->x_loop = loop;
    x->x_buffer = (t_sample *)getbytes(x->x_nch * FRAMES * sizeof(t_sample));
    char buf[50];
    snprintf(buf, 50, "d%lx", (t_int)x);
    x->x_openpanel_sym = gensym(buf);
    pd_bind(&x->x_obj.ob_pd, x->x_openpanel_sym);
    return(x);
}

void setup_play0x2efile_tilde(void) {
    playfile_class = class_new(gensym("play.file~"), (t_newmethod)playfile_new,
        (t_method)playfile_free, sizeof(t_playfile), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addbang(playfile_class, playfile_bang);
    class_addfloat(playfile_class, playfile_float); // toggle on/off
//    class_addmethod(playfile_class, (t_method)player_play, gensym("start"), A_GIMME, 0);
    class_addmethod(playfile_class, (t_method)playfile_bang, gensym("start"), A_NULL);
    class_addmethod(playfile_class, (t_method)playfile_stop, gensym("stop"), A_NULL);
    class_addmethod(playfile_class, (t_method)playfile_open, gensym("open"), A_GIMME, 0);
//    class_addmethod(playfile_class, (t_method)playfile_stream, gensym("stream"), A_SYMBOL, 0);
    class_addmethod(playfile_class, (t_method)playfile_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(playfile_class, (t_method)playfile_seek, gensym("seek"), A_FLOAT, 0);
    class_addmethod(playfile_class, (t_method)playfile_loop, gensym("loop"), A_FLOAT, 0);
    class_addmethod(playfile_class, (t_method)playfile_continue, gensym("continue"), A_NULL);
    class_addmethod(playfile_class, (t_method)playfile_pause, gensym("pause"), A_NULL);
    class_addmethod(playfile_class, (t_method)playfile_set, gensym("set"), A_SYMBOL, 0);
    class_addmethod(playfile_class, (t_method)playfile_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT,0);
    class_addmethod(playfile_class, (t_method)playfile_openpanel_callback, gensym("callback"), A_GIMME, 0);
}
