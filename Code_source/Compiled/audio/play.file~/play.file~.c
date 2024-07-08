#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <samplerate.h>
#include <m_pd.h>

typedef struct _avstream {
    AVCodecContext *ctx;
    int idx; /* stream index */
} t_avstream;

typedef struct _playlist {
    t_symbol **arr; /* m3u list of tracks */
    t_symbol *dir;  /* starting directory */
    int size;       /* size of the list */
    int max;        /* size of the memory allocation */
} t_playlist;

typedef struct _libsamplerate {
    SRC_STATE *state;
    SRC_DATA data;
    double ratio;  /* resampling ratio */
} t_libsamplerate;

typedef struct _playfile {
    t_object x_obj;
    t_sample **x_outs;
    unsigned char x_play; /* play/pause toggle */
    unsigned char x_open; /* true when a file has been successfully opened */
    unsigned x_nch;       /* number of channels */
    t_outlet *x_o_meta;   /* outputs bang when finished */
    t_avstream x_a;   /* audio stream */
    AVPacket *x_pkt;
    AVFrame *x_frm;
    SwrContext *x_swr;
    AVFormatContext *x_ic;
    AVChannelLayout x_layout;
    t_playlist x_plist;
    t_canvas* x_canvas;
    t_libsamplerate x_r;
    t_sample *x_in;
    t_sample *x_out;
    t_float x_speed;
    int x_loop;
    t_symbol* x_play_next;
} t_playfile;
/* ---------------------- player ------------------------ */

// @return 1 if the current state is the same as the one being requested
static inline int pause_state(unsigned char *pause, int ac, t_atom *av) {
    if (ac && av->a_type == A_FLOAT) {
        int state = (av->a_w.w_float != 0);
        if (*pause == state) {
            return 1;
        } else {
            *pause = state;
        }
    } else {
        *pause = !*pause;
    }
    return 0;
}

static void player_play(t_playfile *x, t_symbol *s, int ac, t_atom *av) {
    (void)s;
    if (!x->x_open) {
        return post("No file opened.");
    }
    if (pause_state(x, ac, av)) {
        return;
    }
}

static void player_bang(t_playfile *x) {
    player_play(x, 0, 0, 0);
}

static void player_free(t_playfile *x) {
    freebytes(x->x_outs, x->x_nch * sizeof(t_sample *));
}


/* ---------------------- playlist ------------------------ */

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

static int m3u_size(FILE *fp, char *dir, int dlen) {
    int size = 0;
    M3U_MAIN (
      size += m3u_size(m3u, dir, dlen + len)
    , size++
    )
    return size;
}

static int playlist_fill(t_playlist *pl, FILE *fp, char *dir, int dlen, int i) {
    int oldlen = strlen(pl->dir->s_name);
    M3U_MAIN (
      i = playlist_fill(pl, m3u, dir, dlen + len, i)
    , pl->arr[i++] = gensym(dir + oldlen)
    )
    return i;
}

static inline err_t playlist_m3u(t_playlist *pl, t_symbol *s) {
    FILE *fp = fopen(s->s_name, "r");
    if (!fp) {
        return "Could not open m3u";
    }

    depth = 1;
    char dir[MAXPDSTRING];
    strcpy(dir, pl->dir->s_name);
    int size = m3u_size(fp, dir, strlen(dir));
    if (size < 1) {
        return "Playlist is empty";
    }
    if (size > pl->max) {
        pl->arr = (t_symbol **)resizebytes(pl->arr
        , pl->max * sizeof(t_symbol *), size * sizeof(t_symbol *));
        pl->max = size;
    }
    pl->size = size;
    rewind(fp);

    playlist_fill(pl, fp, dir, strlen(pl->dir->s_name), 0);
    fclose(fp);
    return 0;
}

/* ---------------------- FFmpeg player (base class) ------------------------ */
static err_t (*playfile_base_reset)(void *);

static void playfile_base_seek(t_playfile *x, t_float f) {
	if (!x->x_open) {
		return;
	}
	avformat_seek_file(x->x_ic, -1, 0, f * 1000, x->x_ic->duration, 0);
	AVRational ratio = x->x_ic->streams[x->x_a.idx]->time_base;
	x->x_frm->pts = f * ratio.den / (ratio.num * 1000);
	swr_init(x->x_swr);

	// avcodec_flush_buffers(x->x_a.ctx); // doesn't always flush properly
	avcodec_free_context(&x->x_a.ctx);
	x->x_a.ctx = avcodec_alloc_context3(NULL);
	avcodec_parameters_to_context(x->x_a.ctx, x->x_ic->streams[x->x_a.idx]->codecpar);
	x->x_a.ctx->pkt_timebase = x->x_ic->streams[x->x_a.idx]->time_base;
	const AVCodec *codec = avcodec_find_decoder(x->x_a.ctx->codec_id);
	avcodec_open2(x->x_a.ctx, codec, NULL);
}

static inline err_t playfile_base_context(t_playfile *x, t_avstream *s) {
	int i = s->idx;
	avcodec_free_context(&s->ctx);
	s->ctx = avcodec_alloc_context3(NULL);
	if (!s->ctx) {
		return "Failed to allocate AVCodecContext";
	}
	if (avcodec_parameters_to_context(s->ctx, x->x_ic->streams[i]->codecpar) < 0) {
		return "Failed to fill codec with parameters";
	}
	s->ctx->pkt_timebase = x->x_ic->streams[i]->time_base;

	const AVCodec *codec = avcodec_find_decoder(s->ctx->codec_id);
	if (!codec) {
		return "Codec not found";
	}
	if (avcodec_open2(s->ctx, codec, NULL) < 0) {
		return "Failed to open codec";
	}

	return 0;
}

static AVChannelLayout playfile_base_layout(t_playfile *x) {
	AVChannelLayout layout_in;
	if (x->x_a.ctx->ch_layout.u.mask) {
		av_channel_layout_from_mask(&layout_in, x->x_a.ctx->ch_layout.u.mask);
	} else {
		av_channel_layout_default(&layout_in, x->x_a.ctx->ch_layout.nb_channels);
	}
	return layout_in;
}

static err_t playfile_base_load(t_playfile *x, int index) {
	char url[MAXPDSTRING];
	const char *fname = x->x_plist.arr[index]->s_name;
	if (fname[0] == '/') { // absolute path
		strcpy(url, fname);
	} else {
		strcpy(url, x->x_plist.dir->s_name);
		strcat(url, fname);
	}

	avformat_close_input(&x->x_ic);
	x->x_ic = avformat_alloc_context();
	if (avformat_open_input(&x->x_ic, url, NULL, NULL)) {
		return "Failed to open input stream";
	}
	if (avformat_find_stream_info(x->x_ic, NULL) < 0) {
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
	x->x_a.idx = i;
	if (i < 0) {
		return "No audio stream found";
	}
	err_t err_msg = playfile_base_context(x, &x->x_a);
	if (err_msg) {
		return err_msg;
	}
	x->x_frm->pts = 0;
	return playfile_base_reset(x);
}

static void playfile_base_start(t_playfile *x, t_float f, t_float ms) {
    int track = f;
    err_t err_msg = "";
    if (0 < track && track <= x->x_plist.size) {
        if ( (err_msg = playfile_base_load(x, track - 1)) ) {
            pd_error(x, "playfile_base_start: %s.", err_msg);
        } else if (ms > 0) {
            playfile_base_seek(x, ms);
        }
        x->x_open = !err_msg;
    } else {
        playfile_base_seek(x, 0);
    }
    x->x_play = !err_msg;
}

static void playfile_find_file(t_playfile *x, t_symbol* file, t_symbol** dir_out, t_symbol** filename_out)
{
    const char *filename = file->s_name;
    const char *dirname = canvas_getdir(x->x_canvas)->s_name;

    char dirout[MAXPDSTRING];
    char* fileout;
    open_via_path(dirname, filename, "", dirout, &fileout, MAXPDSTRING-1, 0);

    *filename_out = gensym(fileout);
    strcat(dirout, "/"); // need to happen after filename has been gensym'd!
    *dir_out = gensym(dirout);
}

static void playfile_base_open(t_playfile *x, t_symbol *s) {
	x->x_play = 0;
	err_t err_msg = 0;

	const char *sym = s->s_name;
	if (strlen(sym) >= MAXPDSTRING) {
		err_msg = "File path is too long";
	} else {
        t_symbol* filename;
        t_symbol* dirname;
        playfile_find_file(x, s, &dirname, &filename);
		t_playlist *pl = &x->x_plist;
        pl->dir = dirname;

		const char *ext = strrchr(filename->s_name, '.');
		if (ext && !strcmp(ext + 1, "m3u")) {
			err_msg = playlist_m3u(pl, filename);
		} else {
			pl->size = 1;
			pl->arr[0] = filename;
		}
	}

	if (err_msg || (err_msg = playfile_base_load(x, 0))) {
		pd_error(x, "playfile_base_open: %s.", err_msg);
	}
	x->x_open = !err_msg;

    playfile_base_start(x, 1.0f, 0.0f);
}

static void playfile_base_float(t_playfile *x, t_float f) {
	playfile_base_start(x, f, 0);
}

static void playfile_base_stop(t_playfile *x) {
	playfile_base_start(x, 0, 0);
	x->x_frm->pts = 0; // reset internal position
}

static void playfile_base_free(t_playfile *x) {
	av_channel_layout_uninit(&x->x_layout);
	avcodec_free_context(&x->x_a.ctx);
	avformat_close_input(&x->x_ic);
	av_packet_free(&x->x_pkt);
	av_frame_free(&x->x_frm);
	swr_free(&x->x_swr);

	t_playlist *pl = &x->x_plist;
	freebytes(pl->arr, pl->max * sizeof(t_symbol *));
	player_free(x);
}

#define FRAMES 0x10

/* ------------------------- libsamplerate helpers ------------------------- */


// SRC can get stuck if it's too close to the fastest possible speed
static const t_float fastest = FRAMES - (1. / 128.);
static const t_float slowest = 1. / FRAMES;

static inline void libsamplerate_speed(t_libsamplerate *x, t_float f) {
    f *= x->ratio;
    f = f > fastest ? fastest : (f < slowest ? slowest : f);
    x->data.src_ratio = 1. / f;
}

static inline void libsamplerate_reset(t_libsamplerate *x) {
    src_reset(x->state);
    x->data.output_frames_gen = 0;
    x->data.input_frames = 0;
}

static int libsamplerate_interp(t_libsamplerate *x, int nch, t_float f) {
    int d = f;
    if (d < SRC_SINC_BEST_QUALITY || d > SRC_LINEAR) {
        return 1;
    }

    int err;
    src_delete(x->state);
    if ((x->state = src_new(d, nch, &err)) == NULL) {
        post("Error : src_new() failed : %s.", src_strerror(err));
    }
    return err;
}

static int libsamplerate_init(t_libsamplerate *x, unsigned nch) {
    int err;
    SRC_STATE *state;
    if (!(state = src_new(SRC_SINC_FASTEST, nch, &err))) {
        pd_error(0, "src_new() failed : %s.", src_strerror(err));
    }
    x->state = state;
    x->data.src_ratio = x->ratio = 1.0;
    x->data.output_frames = FRAMES;
    return err;
}

/* ------------------------- FFmpeg player ------------------------- */
static t_class *playfile_class;

static void playfile_seek(t_playfile *x, t_float f) {
	playfile_base_seek(x, f);
	libsamplerate_reset(&x->x_r);
}

static void playfile_speed(t_playfile *x, t_float f) {
	x->x_speed = f;
}

static void playfile_loop(t_playfile *x, t_float f) {
    x->x_loop = f;
}

static void playfile_set(t_playfile *x, t_symbol* s) {
    x->x_play_next = s;
}

static t_int *playfile_perform(t_int *w) {
	t_playfile *x = (t_playfile *)(w[1]);
	unsigned nch = x->x_nch;
	t_sample *outs[nch];
	for (int i = nch; i--;) {
		outs[i] = x->x_outs[i];
	}

	int n = (int)(w[2]);
	if (x->x_play) {
		t_libsamplerate *r = &x->x_r;
		SRC_DATA *data = &r->data;
		while (n--) {
			if (data->output_frames_gen > 0) {
				perform:
				for (int i = nch; i--;) {
					*outs[i]++ = data->data_out[i];
				}
				data->data_out += nch;
				data->output_frames_gen--;
				continue;
			}

			libsamplerate_speed(r, x->x_speed);

			process:
			if (data->input_frames > 0) {
				data->data_out = x->x_out;
				src_process(r->state, data);
				data->input_frames -= data->input_frames_used;
				if (data->input_frames <= 0) {
					data->data_in = x->x_in;
					data->input_frames = swr_convert(x->x_swr
					, (uint8_t **)&x->x_in, FRAMES
					, 0, 0);
				} else {
					data->data_in += data->input_frames_used * nch;
				}
				if (data->output_frames_gen > 0) {
					goto perform;
				} else {
					goto process;
				}
			}
			// receive
			data->data_in = x->x_in;
			for (; av_read_frame(x->x_ic, x->x_pkt) >= 0; av_packet_unref(x->x_pkt)) {
				if (x->x_pkt->stream_index == x->x_a.idx) {
					if (avcodec_send_packet(x->x_a.ctx, x->x_pkt) < 0
					 || avcodec_receive_frame(x->x_a.ctx, x->x_frm) < 0) {
						continue;
					}
					data->input_frames = swr_convert(x->x_swr
					, (uint8_t **)&x->x_in, FRAMES
					, (const uint8_t **)x->x_frm->extended_data, x->x_frm->nb_samples);
					av_packet_unref(x->x_pkt);
					goto process;
				}
			}

			// reached the end
			if (x->x_play) {
				x->x_play = 0;
				n++; // don't iterate in case there's another track
				outlet_bang(x->x_o_meta);
			}
			if(x->x_loop) {
			    if(x->x_play_next)
                {
                    playfile_base_open(x, x->x_play_next);
                    x->x_play_next = NULL;
                }
                playfile_base_start(x, 1.0f, 0.0f);
            } else {
                if(x->x_play_next)
                {
                    playfile_base_open(x, x->x_play_next);
                    playfile_base_stop(x);
                    x->x_play_next = NULL;
                }
				playfile_seek(x, 0);
				goto silence;
			}
		}
	} else while (n--) {
		silence:
		for (int i = nch; i--;) {
			*outs[i]++ = 0;
		}
	}
	return (w + 4);
}

static void playfile_dsp(t_playfile *x, t_signal **sp) {
	for (int i = x->x_nch; i--;) {
		x->x_outs[i] = sp[i]->s_vec;
	}
	dsp_add(playfile_perform, 3, x, sp[0]->s_n, sp[0]->s_vec);
}

static err_t playfile_reset(void *y) {
	t_playfile *x = (t_playfile *)y;
	t_playfile *b = x;
	swr_free(&x->x_swr);
	AVChannelLayout layout_in = playfile_base_layout(b);
	swr_alloc_set_opts2(&x->x_swr
	, &x->x_layout, AV_SAMPLE_FMT_FLT   , x->x_a.ctx->sample_rate
	, &layout_in, x->x_a.ctx->sample_fmt, x->x_a.ctx->sample_rate
	, 0, NULL);
	if (swr_init(x->x_swr) < 0) {
		return "SWResampler initialization failed";
	}

	if (!x->x_r.state) {
		return "SRC has not been initialized";
	}
	libsamplerate_reset(&x->x_r);
	x->x_r.ratio = (double)x->x_a.ctx->sample_rate / sys_getsr();
	libsamplerate_speed(&x->x_r, x->x_speed);
	return 0;
}

AVChannelLayout playfile_get_channel_layout_for_file(const char *dirname, const char *filename) {
    char input_path[MAXPDSTRING];
    if (!input_path) {
        fprintf(stderr, "Could not allocate memory for input path\n");
        goto error;
    }
    snprintf(input_path, MAXPDSTRING, "%s/%s", dirname, filename);

    AVFormatContext *format_context = NULL;
    if (avformat_open_input(&format_context, input_path, NULL, NULL) != 0) {
        fprintf(stderr, "Could not open input file '%s'\n", input_path);
        goto error;
    }

    // Retrieve stream information
    if (avformat_find_stream_info(format_context, NULL) < 0) {
        fprintf(stderr, "Could not find stream information\n");
        goto error;
    }

    int audio_stream_index = -1;
    for (unsigned int i = 0; i < format_context->nb_streams; i++) {
        if (format_context->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            audio_stream_index = i;
            break;
        }
    }

    if (audio_stream_index == -1) {
        fprintf(stderr, "Could not find any audio stream in the file\n");
        goto error;
    }

    AVCodecParameters *codec_parameters = format_context->streams[audio_stream_index]->codecpar;
    return codec_parameters->ch_layout;

error:
    if(format_context) avformat_close_input(&format_context);

    AVChannelLayout l;
    av_channel_layout_default(&l, 1);
    return l;
}

static void *playfile_new(t_symbol *s, int ac, t_atom *av) {
    int loop = 0;
    for(int i = 0; i < ac; i++){
        if(av[i].a_type == A_SYMBOL){ // if name not passed so far, count arg as array name
            s = atom_getsymbolarg(i, ac, av);
            if(s == gensym("-loop")){
                loop = 1;
                for(int j = i; j < ac - 1; j++) {
                    av[j] = av[j + 1];
                }
            }
        }
    }

    t_playfile *x = (t_playfile *)pd_new(playfile_class);
    x->x_canvas = canvas_getcurrent();
    x->x_open = x->x_play = 0;

    x->x_pkt = av_packet_alloc();
    x->x_frm = av_frame_alloc();

    t_playlist *pl = &x->x_plist;
    pl->size = 0;
    pl->max = 1;
    pl->arr = (t_symbol **)getbytes(pl->max * sizeof(t_symbol *));

    int nch = 1;
    AVChannelLayout layout;

    if(ac && av[0].a_type == A_FLOAT) // Num channels from float arg
    {
        nch = atom_getfloat(av);
        uint64_t mask = 0;
        for (int ch = 0; ch < nch; ch++) {
            mask |= (ch + 1);
        }
        av_channel_layout_from_mask(&layout, mask);
    }
    else if(ac && av[0].a_type == A_SYMBOL) // Num channels from file
    {
        t_symbol* dir, *file;
        playfile_find_file(x, atom_getsymbol(av), &dir, &file);
        layout = playfile_get_channel_layout_for_file(dir->s_name, file->s_name);
        nch = layout.nb_channels;
    }

    int err = libsamplerate_init(&x->x_r, nch);
    libsamplerate_interp(&x->x_r, nch, 1);

    // channel layout masking details: libavutil/channel_layout.h
    x->x_layout = layout;
    x->x_nch = nch;
    x->x_outs = (t_sample **)getbytes(nch * sizeof(t_sample *));
    while (nch--) {
        outlet_new(&x->x_obj, &s_signal);
    }

    x->x_o_meta = outlet_new(&x->x_obj, 0);

	if (err) {
		player_free(x);
		pd_free((t_pd *)x);
		return NULL;
	}
    int shift = ac > 0 && av[0].a_type == A_SYMBOL;

    if(ac > 1 - shift && av[1 - shift].a_type == A_SYMBOL)
    {
        playfile_base_open(x, atom_getsymbol(av + 1 - shift));
        playfile_base_stop(x); // open normally also starts playback
    }

    // Autostart argument
    if(ac > 2 - shift && av[2 - shift].a_type == A_FLOAT)  {
        playfile_base_start(x, atom_getfloat(av + 2 - shift), 0.0f);
    }

    // Loop argument
    if(ac > 3 - shift && av[3 - shift].a_type == A_FLOAT) loop = atom_getfloat(av + 3 - shift);
    else loop = 0;

	x->x_in  = (t_sample *)getbytes(x->x_nch * FRAMES * sizeof(t_sample));
    x->x_out = (t_sample *)getbytes(x->x_nch * FRAMES * sizeof(t_sample));
    x->x_speed = 1;
    x->x_loop = loop;

	return x;
}

static void playfile_free(t_playfile *x) {
	playfile_base_free(x);
	src_delete(x->x_r.state);
	freebytes(x->x_in, x->x_nch * sizeof(t_sample) * FRAMES);
	freebytes(x->x_out, x->x_nch * sizeof(t_sample) * FRAMES);
}

void setup_play0x2efile_tilde(void) {
	playfile_base_reset = playfile_reset;

    playfile_class = class_new(gensym("play.file~"), (t_newmethod)playfile_new, (t_method)playfile_free, sizeof(t_playfile), 0, A_GIMME, 0);
    class_addbang(playfile_class, player_bang);
    class_addmethod(playfile_class, (t_method)player_play, gensym("start"), A_GIMME, 0);

	class_addfloat(playfile_class, playfile_base_float);
	class_addmethod(playfile_class, (t_method)playfile_base_open, gensym("open"), A_SYMBOL, 0);
	class_addmethod(playfile_class, (t_method)playfile_base_stop, gensym("stop"), A_NULL);

	class_addmethod(playfile_class, (t_method)playfile_dsp, gensym("dsp"), A_CANT, 0);
	class_addmethod(playfile_class, (t_method)playfile_seek, gensym("seek"), A_FLOAT, 0);
	class_addmethod(playfile_class, (t_method)playfile_speed, gensym("speed"), A_FLOAT, 0);
    class_addmethod(playfile_class, (t_method)playfile_loop, gensym("loop"), A_FLOAT, 0);
    class_addmethod(playfile_class, (t_method)playfile_set, gensym("set"), A_SYMBOL, 0);
}
