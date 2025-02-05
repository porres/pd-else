// tim schoen and porres 2024

#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <else_alloca.h>
#include <m_pd.h>

#define FRAMES 4096

t_canvas *glist_getcanvas(t_glist *x);

typedef struct _playlist{
    t_symbol **arr;     // m3u list of tracks
    t_symbol *dir;      // starting directory
    int       size;     // size of the list
    int       max;      // size of the memory allocation
}t_playlist;

typedef struct _playfile{
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
    t_float         x_speed;
    int             x_loop;
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
    x->x_out_buffer_size = 0;
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

static err_t playfile_load(t_playfile *x, int index) {
    char url[MAXPDSTRING];
    const char *fname = x->x_plist.arr[index]->s_name;
    if (fname[0] == '/') // absolute path
        strcpy(url, fname);
    else{
        strcpy(url, x->x_plist.dir->s_name);
        strcat(url, "/");
        strcat(url, fname);
    }
    if(!x->x_ic || !x->x_ic->url || strncmp(x->x_ic->url, url, MAXPDSTRING) != 0) {
        avformat_close_input(&x->x_ic);
        x->x_ic = avformat_alloc_context();
        x->x_ic->probesize = 128;
        x->x_ic->max_probe_packets = 1;
        if(avformat_open_input(&x->x_ic, url, NULL, NULL))
            return("Failed to open input stream");
        if(avformat_find_stream_info(x->x_ic, NULL) < 0)
            return("Failed to find stream information");
        x->x_ic->seek2any = 1;
        int i = -1;
        for(unsigned j = x->x_ic->nb_streams; j--;){
            if(x->x_ic->streams[j]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO){
                i = j;
                break;
            }
        }
        x->x_stream_idx = i;

        if(i < 0)
            return("No audio stream found");
    }

    err_t err_msg = playfile_context(x);

    if(err_msg)
        return(err_msg);
    x->x_frm->pts = 0;
    return (playfile_reset(x));
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

static int playfile_is_network_protocol(const char *filename) {
    const char *protocols[] = { "http://", "https://", "tcp://", "ftp://", "sftp://", "rtsp://", "rtmp://", "udp://", "data://", "gopher://", "ws://", "wss://" };
    size_t num_protocols = sizeof(protocols) / sizeof(protocols[0]);
    for(size_t i = 0; i < num_protocols; i++){
        if(strncmp(filename, protocols[i], strlen(protocols[i])) == 0)
            return(1); // Match found, it's a network protocol
    }
    return(0); // Not a network protocol
}

static int playfile_find_file(t_playfile *x, t_symbol* file, char* dir_out, char** filename_out){
    if(playfile_is_network_protocol(file->s_name)){
        strcpy(dir_out, file->s_name);
        *filename_out = NULL;
        return(2);
    }
    char *bufptr;
    int fd = canvas_open(x->x_canvas, file->s_name, "", dir_out, filename_out, MAXPDSTRING, 1);
    if(fd < 0){
        pd_error(x, "[play.file~] file '%s' not found", file->s_name);
        return(0);
    }
    return(1);
}

static void playfile_openpanel_callback(t_playfile *x, t_symbol *s, int argc, t_atom *argv){
    if(argc == 1 && argv->a_type == A_SYMBOL){
        err_t err_msg = 0;
        t_symbol* path = atom_getsymbol(argv);
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

static void playfile_open(t_playfile *x, t_symbol *s, int ac, t_atom *av){
    x->x_play = 0;
    err_t err_msg = 0;
    if(ac == 0){
        pdgui_vmess("pdtk_openpanel", "ssic", x->x_openpanel_sym->s_name,
            canvas_getdir(x->x_canvas)->s_name, 0, glist_getcanvas(x->x_canvas));
    }
    else if(av->a_type == A_SYMBOL) {
        const char *sym = av->a_w.w_symbol->s_name;
        if(strlen(sym) >= MAXPDSTRING)
            err_msg = "File path is too long";
        else if(strncmp(av->a_w.w_symbol->s_name, "http:", strlen("http:")) == 0 ||
        strncmp(av->a_w.w_symbol->s_name, "https:", strlen("https:")) == 0 ||
        strncmp(av->a_w.w_symbol->s_name, "ftp:", strlen("ftp:")) == 0){
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
            if (ext && !strcmp(ext + 1, "m3u"))
                err_msg = playlist_m3u(pl, s);
            else{
                pl->size = 1;
                pl->arr[0] = gensym(fname);
            }
            if(err_msg || (err_msg = playfile_load(x, 0)))
                pd_error(x, "[play.file~]: open: %s.", err_msg);
            x->x_open = !err_msg;
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
    x->x_play_next = s;
}

static t_int *playfile_perform(t_int *w){
    t_playfile *x = (t_playfile *)(w[1]);
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
                            playfile_open(x, gensym("open"), 1,
                                &(t_atom){.a_type = A_SYMBOL, .a_w = { .w_symbol = x->x_play_next }});
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

static void playfile_dsp(t_playfile *x, t_signal **sp){
    for(int i = x->x_nch; i--;)
        x->x_outs[i] = sp[i]->s_vec;
    dsp_add(playfile_perform, 3, x, sp[0]->s_n, sp[0]->s_vec);
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
    freebytes(x->x_out, x->x_nch * sizeof(t_sample) * FRAMES);
    pd_unbind(&x->x_obj.ob_pd, x->x_openpanel_sym);
}

static void *playfile_new(t_symbol *s, int ac, t_atom *av){
    int loop = 0;
    for(int i = 0; i < ac; i++){
        if(av[i].a_type == A_SYMBOL){ // if name not passed so far, count arg as array name
            s = atom_getsymbolarg(i, ac, av);
            if(s == gensym("-loop")){
                loop = 1;
                for(int j = i; j < ac; j++){
                    if(j < ac-1)
                        av[j] = av[j + 1];
                    else
                        ac--;
                }
            }
        }
    }
    t_playfile *x = (t_playfile *)pd_new(playfile_class);
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
    int filefound = 0;
    AVChannelLayout layout;
    if(ac && av[0].a_type == A_FLOAT){ // Num channels from float arg
        nch = atom_getfloat(av) > nch ? atom_getfloat(av) : nch;
        uint64_t mask = 0;
        for(int ch = 0; ch < nch; ch++)
            mask |= (ch + 1);
        av_channel_layout_from_mask(&layout, mask);
    }
    else if(ac && av[0].a_type == A_SYMBOL){ // Num channels from file
        char dir[MAXPDSTRING];
        char* file = NULL;
        if(playfile_find_file(x, atom_getsymbol(av), dir, &file))
            filefound = 1;
        if(filefound){
            layout = playfile_get_channel_layout_for_file(x, dir, file);
            nch = layout.nb_channels;
        }
        else
            nch = 1;
    }
    else{
        uint64_t mask = 0;
        for(int ch = 0; ch < nch; ch++)
            mask |= (ch + 1);
        av_channel_layout_from_mask(&layout, mask);
    }
    // channel layout masking details: libavutil/channel_layout.h
    x->x_layout = layout;
    x->x_nch = nch;
    x->x_outs = (t_sample **)getbytes(nch * sizeof(t_sample *));
    while(nch--)
        outlet_new(&x->x_obj, &s_signal);
    x->x_o_meta = outlet_new(&x->x_obj, 0);
    int shift = ac > 0 && av[0].a_type == A_SYMBOL;
    if(filefound && ac > 1 - shift && av[1 - shift].a_type == A_SYMBOL)
        playfile_open(x, gensym("open"), 1, av + 1 - shift);
    // Autostart argument
    if(ac > 2 - shift && av[2 - shift].a_type == A_FLOAT)
        playfile_start(x, atom_getfloat(av + 2 - shift), 0.0f);
    // Loop argument
    if(ac > 3 - shift && av[3 - shift].a_type == A_FLOAT)
        loop = atom_getfloat(av + 3 - shift);
    x->x_speed = 1;
    x->x_loop = loop;
    x->x_out = (t_sample *)getbytes(x->x_nch * FRAMES * sizeof(t_sample));
    char buf[50];
    snprintf(buf, 50, "d%lx", (t_int)x);
    x->x_openpanel_sym = gensym(buf);
    pd_bind(&x->x_obj.ob_pd, x->x_openpanel_sym);
    return(x);
}

void setup_play0x2efile_tilde(void) {
    playfile_class = class_new(gensym("play.file~"), (t_newmethod)playfile_new,
        (t_method)playfile_free, sizeof(t_playfile), 0, A_GIMME, 0);
    class_addbang(playfile_class, playfile_bang);
    class_addfloat(playfile_class, playfile_float); // toggle on/off
//    class_addmethod(playfile_class, (t_method)player_play, gensym("start"), A_GIMME, 0);
    class_addmethod(playfile_class, (t_method)playfile_bang, gensym("start"), A_NULL);
    class_addmethod(playfile_class, (t_method)playfile_stop, gensym("stop"), A_NULL);
    class_addmethod(playfile_class, (t_method)playfile_open, gensym("open"), A_GIMME, 0);
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
