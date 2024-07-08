#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <m_pd.h>
/* ---------------------- player ------------------------ */

static t_atom(*fn_meta)(void *, t_symbol *);

typedef struct _vinlet {
    t_float *p; // inlet pointer
    t_float v; // internal value
} t_vinlet;

typedef struct _player {
    t_object obj;
    t_sample **outs;
    unsigned char play; /* play/pause toggle */
    unsigned char open; /* true when a file has been successfully opened */
    unsigned nch;       /* number of channels */
    t_outlet *o_meta;   /* outputs track metadata */
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

static inline t_atom player_time(t_float ms) {
    return (t_atom) {
        .a_type = A_FLOAT, .a_w = { .w_float = ms }
    };
}

static inline t_atom player_ftime(int64_t ms) {
    char time[33], *t = time;
    double hr = ms / 3600000.;
    float mn = (hr - (int)hr) * 60;
    float sc = (mn - (int)mn) * 60;
    if (hr >= 1) {
        sprintf(t, "%d:", (int)hr);
        t += strlen(t);
    }
    sprintf(t, "%02d:%02d", (int)mn, (int)sc);
    return (t_atom) {
        .a_type = A_SYMBOL, .a_w = { .w_symbol = gensym(time) }
    };
}

static void player_info_custom(t_player *x, int ac, t_atom *av) {
    for (; ac--; av++) {
        if (av->a_type == A_SYMBOL) {
            const char *sym = av->a_w.w_symbol->s_name, *pct, *end;
            while ((pct = strchr(sym, '%')) && (end = strchr(pct + 1, '%'))) {
                int len = pct - sym;
                if (len) {
                    char before[len + 1];
                    strncpy(before, sym, len);
                    before[len] = 0;
                    startpost("%s", before);
                    sym += len;
                }
                pct++;
                len = end - pct;
                char buf[len + 1];
                strncpy(buf, pct, len);
                buf[len] = 0;
                t_atom meta = fn_meta(x, gensym(buf));
                t_symbol *s = meta.a_w.w_symbol;
                switch (meta.a_type) {
                case A_FLOAT: startpost("%g", meta.a_w.w_float); break;
                case A_SYMBOL: startpost("%s", s == &s_bang ? "" : s->s_name); break;
                default: startpost("");
                }
                sym += len + 2;
            }
            startpost("%s%s", sym, ac ? " " : "");
        } else if (av->a_type == A_FLOAT) {
            startpost("%g%s", av->a_w.w_float, ac ? " " : "");
        }
    }
    endpost();
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
static err_t (*ffbase_reset)(void *);

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

static void ffbase_seek(t_ffbase *x, t_float f) {
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

static inline err_t ffbase_context(t_ffbase *x, t_avstream *s) {
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

static err_t ffbase_stream(t_ffbase *x, t_avstream *s, int i, enum AVMediaType type) {
	if (!x->p.open || i == s->idx) {
		return 0;
	}
	if (i >= (int)x->ic->nb_streams) {
		return "Index out of bounds";
	}
	if (x->ic->streams[i]->codecpar->codec_type != type) {
		return "Stream type mismatch";
	}
	t_avstream stream = { .ctx = NULL, .idx = i };
	err_t err_msg = ffbase_context(x, &stream);
	if (err_msg) {
		return err_msg;
	}
	AVCodecContext *ctx = s->ctx;
	*s = stream;
	avcodec_free_context(&ctx);
	return 0;
}

static AVChannelLayout ffbase_layout(t_ffbase *x) {
	AVChannelLayout layout_in;
	if (x->a.ctx->ch_layout.u.mask) {
		av_channel_layout_from_mask(&layout_in, x->a.ctx->ch_layout.u.mask);
	} else {
		av_channel_layout_default(&layout_in, x->a.ctx->ch_layout.nb_channels);
	}
	return layout_in;
}

static void ffbase_audio(t_ffbase *x, t_float f) {
	err_t err_msg = ffbase_stream(x, &x->a, f, AVMEDIA_TYPE_AUDIO);
	if (err_msg) {
		logpost(x, PD_DEBUG, "ffbase_audio: %s.", err_msg);
	} else if ( (err_msg = ffbase_reset(x)) ) {
		logpost(x, PD_DEBUG, "ffbase_audio: %s.", err_msg);
		x->p.open = x->p.play = 0;
	}
}

static err_t ffbase_load(t_ffbase *x, int index) {
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
	err_t err_msg = ffbase_context(x, &x->a);
	if (err_msg) {
		return err_msg;
	}
	x->frm->pts = 0;
	return ffbase_reset(x);
}

static void ffbase_start(t_ffbase *x, t_float f, t_float ms) {
    int track = f;
    err_t err_msg = "";
    if (0 < track && track <= x->plist.size) {
        if ( (err_msg = ffbase_load(x, track - 1)) ) {
            pd_error(x, "ffbase_start: %s.", err_msg);
        } else if (ms > 0) {
            ffbase_seek(x, ms);
        }
        x->p.open = !err_msg;
    } else {
        ffbase_seek(x, 0);
    }
    x->p.play = !err_msg;
}

static void ffbase_open(t_ffbase *x, t_symbol *s) {
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

	if (err_msg || (err_msg = ffbase_load(x, 0))) {
		pd_error(x, "ffbase_open: %s.", err_msg);
	}
	x->p.open = !err_msg;
    
    ffbase_start(x, 1.0f, 0.0f);
}

static t_symbol *dict[11];

static t_atom ffbase_meta(void *y, t_symbol *s) {
	t_ffbase *x = (t_ffbase *)y;
	t_atom meta;
	if (s == dict[0] || s == dict[1]) { // path || url
		SETSYMBOL(&meta, gensym(x->ic->url));
	} else if (s == dict[2]) { // filename
		const char *name = strrchr(x->ic->url, '/');
		name = name ? name + 1 : x->ic->url;
		SETSYMBOL(&meta, gensym(name));
	} else if (s == dict[3]) { // time
		meta = player_time(x->ic->duration / 1000.);
	} else if (s == dict[4]) { // ftime
		meta = player_ftime(x->ic->duration / 1000);
	} else if (s == dict[5]) { // tracks
		SETFLOAT(&meta, x->plist.size);
	} else if (s == dict[6]) { // samplefmt
		SETSYMBOL(&meta, gensym(av_get_sample_fmt_name(x->a.ctx->sample_fmt)));
	} else if (s == dict[7]) { // samplerate
		SETFLOAT(&meta, x->a.ctx->sample_rate);
	} else if (s == dict[8]) { // bitrate
		SETFLOAT(&meta, x->ic->bit_rate / 1000.);
	} else {
		AVDictionaryEntry *entry = av_dict_get(x->ic->metadata, s->s_name, 0, 0);
		if (!entry) { // try some aliases for common terms
			if (s == dict[9]) { // date
				if (!(entry = av_dict_get(x->ic->metadata, "time", 0, 0))
				 && !(entry = av_dict_get(x->ic->metadata, "tyer", 0, 0))
				 && !(entry = av_dict_get(x->ic->metadata, "tdat", 0, 0))
				 && !(entry = av_dict_get(x->ic->metadata, "tdrc", 0, 0))) {
				}
			} else if (s == dict[10]) { // bpm
				entry = av_dict_get(x->ic->metadata, "tbpm", 0, 0);
			}
		}

		if (entry) {
			SETSYMBOL(&meta, gensym(entry->value));
		} else {
			meta = (t_atom){ A_SYMBOL, {.w_symbol = &s_bang} };
		}
	}
	return meta;
}

static void ffbase_print(t_ffbase *x, t_symbol *s, int ac, t_atom *av) {
	(void)s;
	if (!x->p.open) {
		return post("No file opened.");
	}
	if (ac) {
		return player_info_custom(&x->p, ac, av);
	}

	AVDictionary *meta = x->ic->metadata;
	AVDictionaryEntry *artist = av_dict_get(meta, "artist", 0, 0);
	AVDictionaryEntry *title = av_dict_get(meta, "title", 0, 0);
	if (artist || title) {
		// general track info: %artist% - %title%
		post("%s%s%s"
		, artist ? artist->value : ""
		, " - "
		, title ? title->value : ""
		);
	}
}

static void ffbase_list(t_ffbase *x, t_symbol *s, int ac, t_atom *av) {
	(void)s;
	ffbase_start(x, atom_getfloatarg(0, ac, av), atom_getfloatarg(1, ac, av));
}

static void ffbase_float(t_ffbase *x, t_float f) {
	ffbase_start(x, f, 0);
}

static void ffbase_stop(t_ffbase *x) {
	ffbase_start(x, 0, 0);
	x->frm->pts = 0; // reset internal position
}

static t_ffbase *ffbase_new(t_class *cl, int ac, t_atom *av) {
    int nch = ac ? atom_getfloat(av) : 1;
    
	// channel layout masking details: libavutil/channel_layout.h
	uint64_t mask = 0;
	AVChannelLayout layout;
    for (int ch = 0; ch < nch; ch++) {
        mask |= (ch + 1);
	}
	int err = av_channel_layout_from_mask(&layout, mask);
	if (err) {
		pd_error(0, "ffbase_new: invalid channel layout (%d).", err);
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

static void ffbase_free(t_ffbase *x) {
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

static t_class *class_ffbase
(t_symbol *s, t_newmethod newm, t_method free, size_t size) {
	dict[0] = gensym("path");
	dict[1] = gensym("url");
	dict[2] = gensym("filename");
	dict[3] = gensym("time");
	dict[4] = gensym("ftime");
	dict[5] = gensym("tracks");
	dict[6] = gensym("samplefmt");
	dict[7] = gensym("samplerate");
	dict[8] = gensym("bitrate");
	dict[9] = gensym("date");
	dict[10] = gensym("bpm");

	fn_meta = ffbase_meta;

	t_class *cls = class_player(s, newm, free, size);
	class_addfloat(cls, ffbase_float);
	class_addlist(cls, ffbase_list);
	class_addmethod(cls, (t_method)ffbase_audio, gensym("audio"), A_FLOAT, 0);
	class_addmethod(cls, (t_method)ffbase_print, gensym("print"), A_GIMME, 0);
	class_addmethod(cls, (t_method)ffbase_open, gensym("open"), A_SYMBOL, 0);
	class_addmethod(cls, (t_method)ffbase_stop, gensym("stop"), A_NULL);
	return cls;
}
