#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <samplerate.h>
#include <m_pd.h>

/* ---------------------- player ------------------------ */

typedef struct _player {
    t_object obj;
    t_sample **outs;
    unsigned char play; /* play/pause toggle */
    unsigned char open; /* true when a file has been successfully opened */
    unsigned nch;       /* number of channels */
    t_outlet *o_meta;   /* outputs bang when finished */
} t_player;

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

static void player_play(t_player *x, t_symbol *s, int ac, t_atom *av) {
    (void)s;
    if (!x->open) {
        return post("No file opened.");
    }
    if (pause_state(&x->play, ac, av)) {
        return;
    }
}

static void player_bang(t_player *x) {
    player_play(x, 0, 0, 0);
}

static t_player *player_new(t_class *cl, unsigned nch) {
    t_player *x = (t_player *)pd_new(cl);
    x->nch = nch;
    x->outs = (t_sample **)getbytes(nch * sizeof(t_sample *));
    while (nch--) {
        outlet_new(&x->obj, &s_signal);
    }
    x->o_meta = outlet_new(&x->obj, 0);
    x->open = x->play = 0;
    return x;
}

static void player_free(t_player *x) {
    freebytes(x->outs, x->nch * sizeof(t_sample *));
}

static t_class *class_player
(t_symbol *s, t_newmethod newm, t_method free, size_t size) {
    t_class *cls = class_new(s, newm, free, size, 0, A_GIMME, 0);
    class_addbang(cls, player_bang);
    class_addmethod(cls, (t_method)player_play, gensym("play"), A_GIMME, 0);
    return cls;
}

/* ---------------------- playlist ------------------------ */

typedef const char *err_t;

typedef struct _playlist {
    t_symbol **arr; /* m3u list of tracks */
    t_symbol *dir;  /* starting directory */
    int size;       /* size of the list */
    int max;        /* size of the memory allocation */
} t_playlist;

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

typedef struct _avstream {
	AVCodecContext *ctx;
	int idx; /* stream index */
} t_avstream;

typedef struct _ffbase {
	t_player p;
	t_avstream a;   /* audio stream */
	AVPacket *pkt;
	AVFrame *frm;
	SwrContext *swr;
	AVFormatContext *ic;
	AVChannelLayout layout;
	t_playlist plist;
    t_canvas* canvas;
} t_ffbase;

static void playfile_base_seek(t_ffbase *x, t_float f) {
	if (!x->p.open) {
		return;
	}
	avformat_seek_file(x->ic, -1, 0, f * 1000, x->ic->duration, 0);
	AVRational ratio = x->ic->streams[x->a.idx]->time_base;
	x->frm->pts = f * ratio.den / (ratio.num * 1000);
	swr_init(x->swr);

	// avcodec_flush_buffers(x->a.ctx); // doesn't always flush properly
	avcodec_free_context(&x->a.ctx);
	x->a.ctx = avcodec_alloc_context3(NULL);
	avcodec_parameters_to_context(x->a.ctx, x->ic->streams[x->a.idx]->codecpar);
	x->a.ctx->pkt_timebase = x->ic->streams[x->a.idx]->time_base;
	const AVCodec *codec = avcodec_find_decoder(x->a.ctx->codec_id);
	avcodec_open2(x->a.ctx, codec, NULL);
}

static inline err_t playfile_base_context(t_ffbase *x, t_avstream *s) {
	int i = s->idx;
	avcodec_free_context(&s->ctx);
	s->ctx = avcodec_alloc_context3(NULL);
	if (!s->ctx) {
		return "Failed to allocate AVCodecContext";
	}
	if (avcodec_parameters_to_context(s->ctx, x->ic->streams[i]->codecpar) < 0) {
		return "Failed to fill codec with parameters";
	}
	s->ctx->pkt_timebase = x->ic->streams[i]->time_base;

	const AVCodec *codec = avcodec_find_decoder(s->ctx->codec_id);
	if (!codec) {
		return "Codec not found";
	}
	if (avcodec_open2(s->ctx, codec, NULL) < 0) {
		return "Failed to open codec";
	}

	return 0;
}

static AVChannelLayout playfile_base_layout(t_ffbase *x) {
	AVChannelLayout layout_in;
	if (x->a.ctx->ch_layout.u.mask) {
		av_channel_layout_from_mask(&layout_in, x->a.ctx->ch_layout.u.mask);
	} else {
		av_channel_layout_default(&layout_in, x->a.ctx->ch_layout.nb_channels);
	}
	return layout_in;
}

static err_t playfile_base_load(t_ffbase *x, int index) {
	char url[MAXPDSTRING];
	const char *fname = x->plist.arr[index]->s_name;
	if (fname[0] == '/') { // absolute path
		strcpy(url, fname);
	} else {
		strcpy(url, x->plist.dir->s_name);
		strcat(url, fname);
	}

	avformat_close_input(&x->ic);
	x->ic = avformat_alloc_context();
    int err = 0;
	if ((err = avformat_open_input(&x->ic, url, NULL, NULL))!=0) {
		return "Failed to open input stream";
	}
	if (avformat_find_stream_info(x->ic, NULL) < 0) {
		return "Failed to find stream information";
	}
	x->ic->seek2any = 1;

	int i = -1;
	for (unsigned j = x->ic->nb_streams; j--;) {
		if (x->ic->streams[j]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
			i = j;
			break;
		}
	}
	x->a.idx = i;
	if (i < 0) {
		return "No audio stream found";
	}
	err_t err_msg = playfile_base_context(x, &x->a);
	if (err_msg) {
		return err_msg;
	}
	x->frm->pts = 0;
	return playfile_base_reset(x);
}

static void playfile_base_start(t_ffbase *x, t_float f, t_float ms) {
    int track = f;
    err_t err_msg = "";
    if (0 < track && track <= x->plist.size) {
        if ( (err_msg = playfile_base_load(x, track - 1)) ) {
            pd_error(x, "playfile_base_start: %s.", err_msg);
        } else if (ms > 0) {
            playfile_base_seek(x, ms);
        }
        x->p.open = !err_msg;
    } else {
        playfile_base_seek(x, 0);
    }
    x->p.play = !err_msg;
}

static void playfile_base_open(t_ffbase *x, t_symbol *s) {
	x->p.play = 0;
	err_t err_msg = 0;

	const char *sym = s->s_name;
	if (strlen(sym) >= MAXPDSTRING) {
		err_msg = "File path is too long";
	} else {
        const char *filename = s->s_name;
        const char *dirname = canvas_getdir(x->canvas)->s_name;

        char dir[MAXPDSTRING];
        char* file;
        open_via_path(dirname, filename, "", dir, &file, MAXPDSTRING-1, 0);

        t_symbol* filesym = gensym(file);
        strcat(dir, "/");
		t_playlist *pl = &x->plist;
		pl->dir = gensym(dir);

		const char *ext = strrchr(file, '.');
		if (ext && !strcmp(ext + 1, "m3u")) {
			err_msg = playlist_m3u(pl, filesym);
		} else {
			pl->size = 1;
			pl->arr[0] = filesym;
		}
	}

	if (err_msg || (err_msg = playfile_base_load(x, 0))) {
		pd_error(x, "playfile_base_open: %s.", err_msg);
	}
	x->p.open = !err_msg;

    playfile_base_start(x, 1.0f, 0.0f);
}

static void playfile_base_float(t_ffbase *x, t_float f) {
	playfile_base_start(x, f, 0);
}

static void playfile_base_stop(t_ffbase *x) {
	playfile_base_start(x, 0, 0);
	x->frm->pts = 0; // reset internal position
}

static t_ffbase *playfile_base_new(t_class *cl, int ac, t_atom *av) {
    int nch = ac ? atom_getfloat(av) : 1;

	// channel layout masking details: libavutil/channel_layout.h
	uint64_t mask = 0;
	AVChannelLayout layout;
    for (int ch = 0; ch < nch; ch++) {
        mask |= (ch + 1);
	}
	int err = av_channel_layout_from_mask(&layout, mask);
	if (err) {
		pd_error(0, "playfile_base_new: invalid channel layout (%d).", err);
		return NULL;
	}

	t_ffbase *x = (t_ffbase *)player_new(cl, nch);
	x->pkt = av_packet_alloc();
	x->frm = av_frame_alloc();
	x->layout = layout;
    x->canvas = canvas_getcurrent();

	t_playlist *pl = &x->plist;
	pl->size = 0;
	pl->max = 1;
	pl->arr = (t_symbol **)getbytes(pl->max * sizeof(t_symbol *));

	return x;
}

static void playfile_base_free(t_ffbase *x) {
	av_channel_layout_uninit(&x->layout);
	avcodec_free_context(&x->a.ctx);
	avformat_close_input(&x->ic);
	av_packet_free(&x->pkt);
	av_frame_free(&x->frm);
	swr_free(&x->swr);

	t_playlist *pl = &x->plist;
	freebytes(pl->arr, pl->max * sizeof(t_symbol *));
	player_free(&x->p);
}

#define FRAMES 0x10

/* ------------------------- libsamplerate helpers ------------------------- */

typedef struct _libsamplerate {
    SRC_STATE *state;
    SRC_DATA data;
    double ratio;  /* resampling ratio */
} t_libsamplerate;

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

typedef struct _ffplay {
	t_ffbase b;
	t_libsamplerate r;
	t_sample *in;
	t_sample *out;
	t_float speed;
    int loop;
    t_symbol* play_next;
} t_ffplay;

static void playfile_seek(t_ffplay *x, t_float f) {
	playfile_base_seek(&x->b, f);
	libsamplerate_reset(&x->r);
}

static void playfile_speed(t_ffplay *x, t_float f) {
	x->speed = f;
}

static void playfile_loop(t_ffplay *x, t_float f) {
    x->loop = f;
}

static void playfile_set(t_ffplay *x, t_symbol* s) {
    x->play_next = s;
}

static t_int *playfile_perform(t_int *w) {
	t_ffplay *x = (t_ffplay *)(w[1]);
	t_ffbase *b = &x->b;
	t_player *p = &b->p;
	unsigned nch = p->nch;
	t_sample *outs[nch];
	for (int i = nch; i--;) {
		outs[i] = p->outs[i];
	}

	int n = (int)(w[2]);
	if (p->play) {
		t_libsamplerate *r = &x->r;
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

			libsamplerate_speed(r, x->speed);

			process:
			if (data->input_frames > 0) {
				data->data_out = x->out;
				src_process(r->state, data);
				data->input_frames -= data->input_frames_used;
				if (data->input_frames <= 0) {
					data->data_in = x->in;
					data->input_frames = swr_convert(b->swr
					, (uint8_t **)&x->in, FRAMES
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
			data->data_in = x->in;
			for (; av_read_frame(b->ic, b->pkt) >= 0; av_packet_unref(b->pkt)) {
				if (b->pkt->stream_index == b->a.idx) {
					if (avcodec_send_packet(b->a.ctx, b->pkt) < 0
					 || avcodec_receive_frame(b->a.ctx, b->frm) < 0) {
						continue;
					}
					data->input_frames = swr_convert(b->swr
					, (uint8_t **)&x->in, FRAMES
					, (const uint8_t **)b->frm->extended_data, b->frm->nb_samples);
					av_packet_unref(b->pkt);
					goto process;
				}
			}

			// reached the end
			if (p->play) {
				p->play = 0;
				n++; // don't iterate in case there's another track
				outlet_bang(p->o_meta);
			}
			if(x->loop) {
			    if(x->play_next)
                {
                    playfile_base_open(&x->b, x->play_next);
                    x->play_next = NULL;
                }
                playfile_base_start(&x->b, 1.0f, 0.0f);
            } else {
                if(x->play_next)
                {
                    playfile_base_open(&x->b, x->play_next);
                    playfile_base_stop(&x->b);
                    x->play_next = NULL;
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

static void playfile_dsp(t_ffplay *x, t_signal **sp) {
	t_player *p = &x->b.p;
	for (int i = p->nch; i--;) {
		p->outs[i] = sp[i]->s_vec;
	}
	dsp_add(playfile_perform, 3, x, sp[0]->s_n, sp[0]->s_vec);
}

static err_t playfile_reset(void *y) {
	t_ffplay *x = (t_ffplay *)y;
	t_ffbase *b = &x->b;
	swr_free(&b->swr);
	AVChannelLayout layout_in = playfile_base_layout(b);
	swr_alloc_set_opts2(&b->swr
	, &b->layout, AV_SAMPLE_FMT_FLT   , b->a.ctx->sample_rate
	, &layout_in, b->a.ctx->sample_fmt, b->a.ctx->sample_rate
	, 0, NULL);
	if (swr_init(b->swr) < 0) {
		return "SWResampler initialization failed";
	}

	if (!x->r.state) {
		return "SRC has not been initialized";
	}
	libsamplerate_reset(&x->r);
	x->r.ratio = (double)x->b.a.ctx->sample_rate / sys_getsr();
	libsamplerate_speed(&x->r, x->speed);
	return 0;
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

	t_ffplay *x = (t_ffplay *)playfile_base_new(playfile_class, ac, av);
    int err = libsamplerate_init(&x->r, ac == 0 ? 1 : (int)atom_getfloat(av));
    libsamplerate_interp(&x->r, x->b.p.nch, 1);

	if (err) {
		player_free(&x->b.p);
		pd_free((t_pd *)x);
		return NULL;
	}

    // File argument
    if(ac > 1 && av[1].a_type == A_SYMBOL)
    {
        playfile_base_open(&x->b, atom_getsymbol(av + 1));
        playfile_base_stop(&x->b); // open normally also starts playback
    }

    // Autostart argument
    if(ac > 2 && av[2].a_type == A_FLOAT)  {
        playfile_base_start(&x->b, atom_getfloat(av + 2), 0.0f);
    }

    // Loop argument
    if(ac > 3 && av[3].a_type == A_FLOAT) loop = atom_getfloat(av + 3);
    else loop = 0;

	x->in  = (t_sample *)getbytes(x->b.p.nch * FRAMES * sizeof(t_sample));
    x->out = (t_sample *)getbytes(x->b.p.nch * FRAMES * sizeof(t_sample));
    x->speed = 1;
    x->loop = loop;


	return x;
}

static void playfile_free(t_ffplay *x) {
	playfile_base_free(&x->b);
	src_delete(x->r.state);
	freebytes(x->in, x->b.p.nch * sizeof(t_sample) * FRAMES);
	freebytes(x->out, x->b.p.nch * sizeof(t_sample) * FRAMES);
}

void setup_play0x2efile_tilde(void) {
	playfile_base_reset = playfile_reset;
	playfile_class = class_player(gensym("play.file~"), (t_newmethod)playfile_new, (t_method)playfile_free, sizeof(t_ffplay));

	class_addfloat(playfile_class, playfile_base_float);
	class_addmethod(playfile_class, (t_method)playfile_base_open, gensym("open"), A_SYMBOL, 0);
	class_addmethod(playfile_class, (t_method)playfile_base_stop, gensym("stop"), A_NULL);

	class_addmethod(playfile_class, (t_method)playfile_dsp, gensym("dsp"), A_CANT, 0);
	class_addmethod(playfile_class, (t_method)playfile_seek, gensym("seek"), A_FLOAT, 0);
	class_addmethod(playfile_class, (t_method)playfile_speed, gensym("speed"), A_FLOAT, 0);
    class_addmethod(playfile_class, (t_method)playfile_loop, gensym("loop"), A_FLOAT, 0);
    class_addmethod(playfile_class, (t_method)playfile_set, gensym("set"), A_SYMBOL, 0);
}
