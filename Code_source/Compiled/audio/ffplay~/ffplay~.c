#include "ffbase.h"
#include <samplerate.h>

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
static t_class *ffplay_class;

typedef struct _ffplay {
	t_ffbase b;
	t_libsamplerate r;
	t_sample *in;
	t_sample *out;
	t_vinlet speed; /* rate of playback */
} t_ffplay;

static void ffplay_seek(t_ffplay *x, t_float f) {
	ffbase_seek(&x->b, f);
	libsamplerate_reset(&x->r);
}

static void ffplay_speed(t_ffplay *x, t_float f) {
	*x->speed.p = f;
}

static void ffplay_interp(t_ffplay *x, t_float f) {
	int err = libsamplerate_interp(&x->r, x->b.p.nch, f);
	if (err) {
		x->b.p.open = x->b.p.play = 0;
	}
}

static t_int *ffplay_perform(t_int *w) {
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
		t_sample *in2 = (t_sample *)(w[3]);
		t_libsamplerate *r = &x->r;
		SRC_DATA *data = &r->data;
		for (; n--; in2++) {
			if (data->output_frames_gen > 0) {
				perform:
				for (int i = nch; i--;) {
					*outs[i]++ = data->data_out[i];
				}
				data->data_out += nch;
				data->output_frames_gen--;
				continue;
			}
			x->speed.v = *in2;
			libsamplerate_speed(r, *in2);

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
				} else if (b->pkt->stream_index == b->sub.idx) {
					int got;
					AVSubtitle sub;
					if (avcodec_decode_subtitle2(b->sub.ctx, &sub, &got, b->pkt) >= 0 && got) {
						post("\n%s", b->pkt->data);
					}
				}
			}

			// reached the end
			if (p->play) {
				p->play = 0;
				n++; // don't iterate in case there's another track
				outlet_anything(p->o_meta, s_done, 0, 0);
			} else {
				ffplay_seek(x, 0);
				t_atom play = { .a_type = A_FLOAT, .a_w = {.w_float = p->play} };
				outlet_anything(p->o_meta, s_play, 1, &play);
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

static void ffplay_dsp(t_ffplay *x, t_signal **sp) {
	t_player *p = &x->b.p;
	for (int i = p->nch; i--;) {
		p->outs[i] = sp[i + 1]->s_vec;
	}
	dsp_add(ffplay_perform, 3, x, sp[0]->s_n, sp[0]->s_vec);
}

static err_t ffplay_reset(void *y) {
	t_ffplay *x = (t_ffplay *)y;
	t_ffbase *b = &x->b;
	swr_free(&b->swr);
	AVChannelLayout layout_in = ffbase_layout(b);
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
	libsamplerate_speed(&x->r, x->speed.v);
	return 0;
}

static void *ffplay_new(t_symbol *s, int ac, t_atom *av) {
    struct _inlet {
        t_pd i_pd;
        struct _inlet *i_next;
        t_object *i_owner;
        t_pd *i_dest;
        t_symbol *i_symfrom;
        union {
            t_symbol *iu_symto;
            t_gpointer *iu_pointerslot;
            t_float *iu_floatslot;
            t_symbol **iu_symslot;
            t_float iu_floatsignalvalue;
        };
    };

	(void)s;
	t_ffplay *x = (t_ffplay *)ffbase_new(ffplay_class, ac, av);
    int err = libsamplerate_init(&x->r, ac == 0 ? 1 : ac);
	if (err) {
		player_free(&x->b.p);
		pd_free((t_pd *)x);
		return NULL;
	}
	t_inlet *in2 = signalinlet_new(&x->b.p.obj, (x->speed.v = 1.0));
	x->speed.p = &in2->iu_floatsignalvalue;
	x->in  = (t_sample *)getbytes(ac * FRAMES * sizeof(t_sample));
	x->out = (t_sample *)getbytes(ac * FRAMES * sizeof(t_sample));
	return x;
}

static void ffplay_free(t_ffplay *x) {
	ffbase_free(&x->b);
	src_delete(x->r.state);
	freebytes(x->in, x->b.p.nch * sizeof(t_sample) * FRAMES);
	freebytes(x->out, x->b.p.nch * sizeof(t_sample) * FRAMES);
}

void ffplay_tilde_setup(void) {
	ffbase_reset = ffplay_reset;
	ffplay_class = class_ffbase(gensym("ffplay~")
	, (t_newmethod)ffplay_new, (t_method)ffplay_free
	, sizeof(t_ffplay));

	class_addmethod(ffplay_class, (t_method)ffplay_dsp
	, gensym("dsp"), A_CANT, 0);
	class_addmethod(ffplay_class, (t_method)ffplay_seek
	, gensym("seek"), A_FLOAT, 0);
	class_addmethod(ffplay_class, (t_method)ffplay_speed
	, gensym("speed"), A_FLOAT, 0);
	class_addmethod(ffplay_class, (t_method)ffplay_interp
	, gensym("interp"), A_FLOAT, 0);
}
