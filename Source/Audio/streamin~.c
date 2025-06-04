// tim schoen and porres 2024

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <else_alloca.h>
#include <m_pd.h>
#include <libavutil/error.h>  // for av_strerror()

#define FRAMES 4096

t_canvas *glist_getcanvas(t_glist *x);

typedef struct _playlist{
    t_symbol **arr;     // m3u list of tracks
    t_symbol *dir;      // starting directory
    int       size;     // size of the list
    int       max;      // size of the memory allocation
}t_playlist;

typedef struct _streamin{
    t_object        x_obj;
    t_sample      **x_outs;
    unsigned char   x_play;     // play/pause toggle
    unsigned char   x_open;     // true when a file has been successfully opened
    unsigned        x_nch;      // number of channels
    t_outlet       *x_o_meta;   // outputs bang when finished
    AVCodecContext *x_stream_ctx;
    int            x_stream_idx;
    AVPacket       *x_pkt;
    AVFrame        *x_frm;
    SwrContext     *x_swr;
    AVFormatContext *x_ic;
    AVChannelLayout x_layout;
    t_playlist      x_plist;
    t_canvas       *x_canvas;
    t_sample       *x_out;
    int             x_out_buffer_index;
    int             x_out_buffer_size;
    int             x_loop;
    t_symbol       *x_play_next;
    t_symbol       *x_openpanel_sym;
}t_streamin;

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


static void streamin_seek(t_streamin *x, t_float f){
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
    x->x_out_buffer_size = 0;
}

static inline err_t streamin_context(t_streamin *x){
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

static AVChannelLayout streamin_layout(t_streamin *x){
    AVChannelLayout layout_in;
    if(x->x_stream_ctx->ch_layout.u.mask)
        av_channel_layout_from_mask(&layout_in, x->x_stream_ctx->ch_layout.u.mask);
    else
        av_channel_layout_default(&layout_in, x->x_stream_ctx->ch_layout.nb_channels);
    return(layout_in);
}

static err_t streamin_reset(void *y){
    t_streamin *x = (t_streamin *)y;
    t_streamin *b = x;
    swr_free(&x->x_swr);
    AVChannelLayout layout_in = streamin_layout(b);
    swr_alloc_set_opts2(&x->x_swr, &x->x_layout, AV_SAMPLE_FMT_FLT,
        x->x_stream_ctx->sample_rate, &layout_in, x->x_stream_ctx->sample_fmt,
        x->x_stream_ctx->sample_rate, 0, NULL);
    if(swr_init(x->x_swr) < 0)
        return("SWResampler initialization failed");
    return(0);
}

static err_t streamin_load(t_streamin *x, int index) {
    char url[MAXPDSTRING];
    const char *fname = x->x_plist.arr[index]->s_name;

    if (fname[0] == '/') { // absolute path
        strcpy(url, fname);
    } else {
        size_t dir_len = strlen(x->x_plist.dir->s_name);
        // Copy directory name
        strcpy(url, x->x_plist.dir->s_name);
        // If directory does NOT end with '/', append one
        if (dir_len > 0 && x->x_plist.dir->s_name[dir_len - 1] != '/') {
            strcat(url, "/");
        }
        // Append the file name
        strcat(url, fname);
    }

    if (!x->x_ic || !x->x_ic->url || strncmp(x->x_ic->url, url, MAXPDSTRING) != 0) {
        avformat_close_input(&x->x_ic);
        x->x_ic = avformat_alloc_context();
        x->x_ic->probesize = 128;
        x->x_ic->max_probe_packets = 1;

        int ret = avformat_open_input(&x->x_ic, url, NULL, NULL);
        if (ret < 0) {
            char errbuf[256];
            av_strerror(ret, errbuf, sizeof(errbuf));
            post("streamin~: Failed to open input stream '%s': %s", url, errbuf);
            return "Failed to open input stream";
        }

        if (avformat_find_stream_info(x->x_ic, NULL) < 0) {
            post("streamin~: Failed to find stream information for '%s'", url);
            return "Failed to find stream information";
        }
        x->x_ic->seek2any = 1;
        int i = -1;
        for (unsigned j = x->x_ic->nb_streams; j--;) {
            if (x->x_ic->streams[j]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
                i = j;
                break;
            }
        }
        x->x_stream_idx = i;
        if (i < 0) {
            post("streamin~: No audio stream found in '%s'", url);
            return "No audio stream found";
        }
    }

    err_t err_msg = streamin_context(x);
    if (err_msg)
        return err_msg;

    x->x_frm->pts = 0;
    return streamin_reset(x);
}


static void streamin_pause(t_streamin *x){
    x->x_play = 0;
}

static void streamin_continue(t_streamin *x){
    x->x_play = 1;
}

static void streamin_start(t_streamin *x, t_float f, t_float ms){
    int track = f;
    err_t err_msg = "";
    if(0 < track && track <= x->x_plist.size){
        if((err_msg = streamin_load(x, track - 1)))
            pd_error(x, "[streamin~] 'base start': %s.", err_msg);
        streamin_seek(x, ms);
        x->x_open = !err_msg;
    }
    else
        streamin_seek(x, 0);
    x->x_play = !err_msg;
}

/*static int streamin_is_network_protocol(const char *filename) {
    const char *protocols[] = { "http://", "https://", "tcp://", "ftp://", "sftp://", "rtsp://", "rtmp://", "udp://", "data://", "gopher://", "ws://", "wss://" };
    size_t num_protocols = sizeof(protocols) / sizeof(protocols[0]);
    for(size_t i = 0; i < num_protocols; i++){
        if(strncmp(filename, protocols[i], strlen(protocols[i])) == 0)
            return(1); // Match found, it's a network protocol
    }
    return(0); // Not a network protocol
}*/

static int streamin_find_file(t_streamin *x, t_symbol* file, char* dir_out, char** filename_out){
/*    if(streamin_is_network_protocol(file->s_name)){
        strcpy(dir_out, file->s_name);
        *filename_out = NULL;
        return(2);
    }*/
    char *bufptr;
    int fd = canvas_open(x->x_canvas, file->s_name, "", dir_out, filename_out, MAXPDSTRING, 1);
    if(fd < 0){
        pd_error(x, "[streamin~] file '%s' not found", file->s_name);
        return(0);
    }
    return(1);
}

static void streamin_openurl(t_streamin *x, t_symbol *s){
    x->x_play = 0;
    err_t err_msg = 0;
    const char *url = s->s_name;
    if(strlen(url) >= MAXPDSTRING){
        err_msg = "URL is too long";
        pd_error(x, "[streamin~]: %s.", err_msg);
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
        if(err_msg || (err_msg = streamin_load(x, 0)))
            pd_error(x, "[streamin~]: open: %s.", err_msg);
        x->x_open = !err_msg;
    }
}

static void streamin_open(t_streamin *x, t_symbol *s, int ac, t_atom *av){
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
            pd_error(x, "[streamin~]: %s.", err_msg);
        }
        else{
            char dirname[MAXPDSTRING];
            char* filename = NULL;
            if(!streamin_find_file(x, av->a_w.w_symbol, dirname, &filename))
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
            if(err_msg || (err_msg = streamin_load(x, 0)))
                pd_error(x, "[streamin~]: open: %s.", err_msg);
            x->x_open = !err_msg;
        }
    }
}

static void streamin_float(t_streamin *x, t_float f){
    streamin_start(x, f, 0);
}

static void streamin_bang(t_streamin *x){
    streamin_float(x, 1);
}

static void streamin_stop(t_streamin *x){
    streamin_float(x, 0);
}

static t_class *streamin_class;

static void streamin_loop(t_streamin *x, t_float f){
    x->x_loop = f;
}

static void streamin_set(t_streamin *x, t_symbol* s){
    x->x_play_next = s;
}

static t_int *streamin_perform(t_int *w){
    t_streamin *x = (t_streamin *)(w[1]);
    unsigned nch = x->x_nch;
    t_sample** outs = ALLOCA(t_sample*, nch);
    for (int i = nch; i--;)
        outs[i] = x->x_outs[i];
    int n = (int)(w[2]);
    int samples_filled = 0;
    if(x->x_play){
        while (samples_filled < n) {
            if (x->x_out_buffer_index >= x->x_out_buffer_size) {
                // Need to read and convert more data
                x->x_out_buffer_index = 0;
                x->x_out_buffer_size = 0;
                while(av_read_frame(x->x_ic, x->x_pkt) >= 0){
                    if(x->x_pkt->stream_index == x->x_stream_idx){
                        if(avcodec_send_packet(x->x_stream_ctx, x->x_pkt) < 0
                        || avcodec_receive_frame(x->x_stream_ctx, x->x_frm) < 0)
                            continue;
                        int samples_converted = swr_convert(x->x_swr, (uint8_t **)&x->x_out, FRAMES,
                            (const uint8_t **)x->x_frm->extended_data, x->x_frm->nb_samples);
                        x->x_out_buffer_size = samples_converted;
                        if(samples_converted < 0){
                            fprintf(stderr, "Error converting samples\n");
                            x->x_out_buffer_size = 0;
                            continue;
                        }
                        x->x_out_buffer_size = samples_converted * nch;
                        break; // Break out of the inner while loop
                    }
                    av_packet_unref(x->x_pkt);
                }
                if(x->x_out_buffer_size == 0){
                    if(x->x_play){
                        x->x_play = 0;
                        outlet_bang(x->x_o_meta);
                    }
                    if(x->x_loop){
                        if(x->x_play_next){
                            streamin_open(x, gensym("open"), 1, &(t_atom){.a_type = A_SYMBOL, .a_w = { .w_symbol = x->x_play_next }});
                            x->x_play_next = NULL;
                        }
                        streamin_seek(x, 0.0f);
                        x->x_play = 1;
                    }
                    else{
                        if(x->x_play_next){
                            streamin_open(x, gensym("open"), 1, &(t_atom){.a_type = A_SYMBOL, .a_w = { .w_symbol = x->x_play_next }});
                            streamin_stop(x);
                            x->x_play_next = NULL;
                        }
                        streamin_seek(x, 0);
                        goto silence;
                    }
                }
            }
            // Fill the output buffer
            while(samples_filled < n && x->x_out_buffer_index < x->x_out_buffer_size){
                for(unsigned int ch = 0; ch < nch; ch++)
                    outs[ch][samples_filled] = x->x_out[x->x_out_buffer_index + ch];
                x->x_out_buffer_index += nch;
                samples_filled++;
            }
        }
    }
    else {
        while(samples_filled < n){
        silence:
            for(int ch = nch; ch--;)
                outs[ch][samples_filled] = 0.0f;
            samples_filled++;
        }
    }

    FREEA(outs, t_sample*, nch);
    return(w+4);
}

static void streamin_dsp(t_streamin *x, t_signal **sp){
    for(int i = x->x_nch; i--;)
        x->x_outs[i] = sp[i]->s_vec;
    dsp_add(streamin_perform, 3, x, sp[0]->s_n, sp[0]->s_vec);
}

static void streamin_free(t_streamin *x){
    av_channel_layout_uninit(&x->x_layout);
    avcodec_free_context(&x->x_stream_ctx);
    avformat_close_input(&x->x_ic);
    av_packet_free(&x->x_pkt);
    av_frame_free(&x->x_frm);
    swr_free(&x->x_swr);
    t_playlist *pl = &x->x_plist;
    freebytes(pl->arr, pl->max * sizeof(t_symbol *));
    freebytes(x->x_outs, x->x_nch * sizeof(t_sample *));
    freebytes(x->x_out, x->x_nch * sizeof(t_sample) * FRAMES);
    pd_unbind(&x->x_obj.ob_pd, x->x_openpanel_sym);
}

static void *streamin_new(t_symbol *s, int ac, t_atom *av){
    t_streamin *x = (t_streamin *)pd_new(streamin_class);
    x->x_canvas = canvas_getcurrent();
    x->x_open = x->x_play = 0;
    x->x_pkt = av_packet_alloc();
    x->x_frm = av_frame_alloc();
    x->x_ic = NULL;
    t_playlist *pl = &x->x_plist;
    pl->size = 0;
    pl->max = 1;
    pl->arr = (t_symbol **)getbytes(pl->max * sizeof(t_symbol *));
    int nch = 1;
    AVChannelLayout layout;
    int loop = 0;
    int ncharg = 0;
    t_atom at[1];
    if(atom_getsymbol(av) == gensym("-loop")){
        loop = 1;
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
            ncharg = 1;
            nch = atom_getfloat(av);
            if(nch < 1)
                nch = 1;
            uint64_t mask = 0;
            for(int ch = 0; ch < nch; ch++)
                mask |= (ch + 1);
            av_channel_layout_from_mask(&layout, mask);
            ac--, av++;
        }
    }
    // channel layout masking details: libavutil/channel_layout.h
    x->x_layout = layout;
    x->x_nch = nch;
    x->x_outs = (t_sample **)getbytes(nch * sizeof(t_sample *));
    while(nch--)
        outlet_new(&x->x_obj, &s_signal);
    x->x_o_meta = outlet_new(&x->x_obj, 0);
    x->x_loop = loop;
    x->x_out = (t_sample *)getbytes(x->x_nch * FRAMES * sizeof(t_sample));
    char buf[50];
    snprintf(buf, 50, "d%lx", (t_int)x);
    x->x_openpanel_sym = gensym(buf);
    pd_bind(&x->x_obj.ob_pd, x->x_openpanel_sym);
    return(x);
}

void streamin_tilde_setup(void) {
    streamin_class = class_new(gensym("streamin~"), (t_newmethod)streamin_new,
        (t_method)streamin_free, sizeof(t_streamin), 0, A_GIMME, 0);
    class_addbang(streamin_class, streamin_bang);
    class_addfloat(streamin_class, streamin_float); // toggle on/off
    class_addmethod(streamin_class, (t_method)streamin_bang, gensym("start"), A_NULL);
    class_addmethod(streamin_class, (t_method)streamin_stop, gensym("stop"), A_NULL);
//    class_addmethod(streamin_class, (t_method)streamin_open, gensym("open"), A_GIMME, 0);
    class_addmethod(streamin_class, (t_method)streamin_openurl, gensym("open"), A_SYMBOL, 0);
    class_addmethod(streamin_class, (t_method)streamin_dsp, gensym("dsp"), A_CANT, 0);
//    class_addmethod(streamin_class, (t_method)streamin_seek, gensym("seek"), A_FLOAT, 0);
    class_addmethod(streamin_class, (t_method)streamin_loop, gensym("loop"), A_FLOAT, 0);
    class_addmethod(streamin_class, (t_method)streamin_continue, gensym("continue"), A_NULL);
    class_addmethod(streamin_class, (t_method)streamin_pause, gensym("pause"), A_NULL);
    class_addmethod(streamin_class, (t_method)streamin_set, gensym("set"), A_SYMBOL, 0);
}
