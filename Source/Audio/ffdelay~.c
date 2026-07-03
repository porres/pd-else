// ffdelay~

#include <m_pd.h>
#include <buffer.h>
#include <magic.h>
#include <string.h>
#include <stdlib.h>

#define FFDEL_GUARD     4           // guard points for interpolation
#define MAXLEN          1024

typedef struct _ffdelay{
    t_object        x_obj;
    t_sample        *x_buf;         // contiguous per-channel segments
    unsigned int    x_allocsize;    // total floats currently allocated (all channels)
    unsigned int    x_maxsize;      // per-channel buffer length in samples (excl. guard)
    int             x_phase;        // write-head offset, shared across channels
    t_float         x_sr_khz;       // sample rate in khz
    unsigned int    x_ms;           // ms flag
    unsigned int    x_freeze;       // freeze flag
    int             x_nchans;       // channel count = max(ch1, ch2)
    int             x_chs1;         // channel count of the audio inlet
    int             x_chs2;         // channel count of the delay-time inlet
    int             x_n;            // block size
    float          *x_deltime_list;
    t_int           x_list_size;
    t_glist        *x_glist;
    t_int           x_sig2;
    t_float        *x_signalscalar; // right inlet's float field
    t_float         x_time_float;   // float from magic input
    t_clock        *x_clock;
}t_ffdelay;

static t_class *ffdelay_class;

static void ffdelay_tick(t_ffdelay *x){
    (void)x;
    canvas_update_dsp();
}

// (re)allocate the buffer for the given per-channel size / channel count.
// Only grows; never shrinks the actual allocation, mirroring del_update()'s
// approach of resizing exactly when size/nchans change, but keeping capacity
// around rather than paying for a resizebytes() on every fluctuation.
static void ffdelay_alloc(t_ffdelay *x, unsigned int maxsize, int nchans){
    unsigned int chsize = maxsize + 2 * FFDEL_GUARD - 1;
    unsigned int needed = chsize * (unsigned int)nchans;
    if(needed > x->x_allocsize){
        x->x_buf = (t_sample *)(x->x_buf ?
            resizebytes(x->x_buf, x->x_allocsize * sizeof(t_sample), needed * sizeof(t_sample)) :
            getbytes(needed * sizeof(t_sample)));
        x->x_allocsize = needed;
    }
    x->x_maxsize = maxsize;
    x->x_nchans = nchans;
    memset(x->x_buf, 0, needed * sizeof(t_sample));
    x->x_phase = FFDEL_GUARD - 1;
}

static void ffdelay_clear(t_ffdelay *x){
    unsigned int chsize = x->x_maxsize + 2 * FFDEL_GUARD - 1;
    if(x->x_buf)
        memset(x->x_buf, 0, chsize * (unsigned int)x->x_nchans * sizeof(t_sample));
}

static void ffdelay_size(t_ffdelay *x, t_float size){
    if(x->x_ms)
        size *= x->x_sr_khz;
    unsigned int maxsize = (size < 1 ? 1 : (unsigned int)size);
    ffdelay_alloc(x, maxsize, x->x_nchans);
}

static void ffdelay_freeze(t_ffdelay *x, t_float f){
    x->x_freeze = (unsigned int)(f != 0);
}

static void ffdelay_time(t_ffdelay *x, t_symbol *s, int ac, t_atom * av){
    (void)s;
    if(ac == 0)
        return;
    if(ac > MAXLEN)
        ac = MAXLEN;
    for(int i = 0; i < ac; i++)
        x->x_deltime_list[i] = atom_getfloat(av+i);
    else_magic_setnan(x->x_signalscalar);
    if(x->x_list_size != ac){
        x->x_list_size = ac;
        canvas_update_dsp();
    }
}

static void ffdelay_set(t_ffdelay *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(ac != 2)
        return;
    int i = atom_getint(av);
    float f = atom_getfloat(av+1);
    if(i >= x->x_list_size)
        i = x->x_list_size;
    if(i <= 0)
        i = 1;
    i--;
    x->x_deltime_list[i] = f;
}

static t_int *ffdelay_perform(t_int *w){
    t_ffdelay *x = (t_ffdelay *)(w[1]);
    t_sample *in1 = (t_sample *)(w[2]);
    t_sample *in2 = (t_sample *)(w[3]);
    t_sample *out = (t_sample *)(w[4]);
    int n = x->x_n, nchans = x->x_nchans, phase = x->x_phase;
    unsigned int maxsize = x->x_maxsize, chsize = maxsize + 2 * FFDEL_GUARD - 1;
    if(!x->x_sig2){
        t_float *scalar = x->x_signalscalar;
        if(!else_magic_isnan(*x->x_signalscalar)){
            t_float input_delay = *scalar;
            x->x_deltime_list[0] = input_delay;
            else_magic_setnan(x->x_signalscalar);
            if(x->x_list_size != 1){
                x->x_list_size = 1;
                clock_delay(x->x_clock, 0); // update canvas
            }
        }
    }
    for(int ch = 0; ch < nchans; ch++){
        t_sample *buf = x->x_buf + (unsigned int)ch * chsize;
        t_sample *bp = buf + FFDEL_GUARD - 1;
        t_sample *ep = buf + chsize;
        t_sample *wp = buf + x->x_phase;
        t_sample *input = in1 + (ch % x->x_chs1) * n;
        t_sample *time;
        double del_time;
        if(x->x_sig2)
            time = in2 + (ch % x->x_chs2) * n;
        else
            del_time = x->x_deltime_list[ch % x->x_chs2];
        t_sample *output = out + ch * n;
        int k = n;
        while(k--){
            t_sample in = *input++;
            if(PD_BIGORSMALL(in))
                in = 0.;
            double del;
            if(x->x_sig2)
                del = *time++;
            else
                del = del_time;
            if(x->x_ms) // convert from ms
                del *= x->x_sr_khz;
            if(del < 0) // clamp to 0
                del = 0;
            int lin = 0;
            if(del >= 1)
                del -= 1;
            else
                lin = 1;
            if(del > maxsize - 1) // clamp again
                del = maxsize - 1;
            int idel = (int)del;                    // integer part of delay time in samples
            t_sample frac = del - (t_sample)idel;   // fractional part
            if(!x->x_freeze) // write if not frozen
                *wp = in;
            t_sample *rp = wp - idel; // read head behind write head
            if(rp < bp) // wrap
                rp += (maxsize + FFDEL_GUARD);
            t_sample a = rp[0], b = rp[-1],  c = rp[-2], d = rp[-3];
            *output++ = lin ? a + (b - a) * frac : interp_spline(frac, a, b, c, d);
            if(++wp == ep){ // advance and wrap
                buf[0] = wp[-3];
                buf[1] = wp[-2];
                buf[2] = wp[-1];
                wp = bp;
            }
        }
        if(ch == 0)
            phase = (int)(wp - buf);
    }
    x->x_phase = phase;
    return(w+5);
}

static void ffdelay_dsp(t_ffdelay *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    x->x_n = sp[0]->s_length;
    x->x_sig2 = else_magic_inlet_connection((t_object *)x, x->x_glist, 1, &s_signal);
    x->x_chs1 = sp[0]->s_nchans, x->x_chs2 = x->x_sig2 ? sp[1]->s_nchans : x->x_list_size;
    // nchans is the max of inputs and the narrower wraps
    int nchans = x->x_chs1 > x->x_chs2 ? x->x_chs1 : x->x_chs2;
    if(nchans != x->x_nchans)
        ffdelay_alloc(x, x->x_maxsize, nchans);
    signal_setmultiout(&sp[2], nchans);
    dsp_add(ffdelay_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void ffdelay_free(t_ffdelay *x){
    if(x->x_buf)
        freebytes(x->x_buf, x->x_allocsize * sizeof(t_sample));
    freebytes(x->x_deltime_list, MAXLEN * sizeof(float));
    clock_free(x->x_clock);
}

static void *ffdelay_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_ffdelay *x = (t_ffdelay *)pd_new(ffdelay_class);
    x->x_sr_khz = sys_getsr() * 0.001;
    float delsize = 0;
    float del_time = 0;
    x->x_freeze = 0;
    x->x_ms = 1;
    x->x_deltime_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_deltime_list[0] = 0;
    x->x_list_size = 1;
    int size_flag = 0;
    int argnum = 0;
    int n = 0;
    while(ac > 0){
        if(av->a_type == A_FLOAT){
            float max_time = 0;
            while(ac && av->a_type == A_FLOAT){
                if(n < MAXLEN){
                    x->x_deltime_list[n] = atom_getfloat(av);
                    if(x->x_deltime_list[n] < 0)
                        x->x_deltime_list[n] = 0;
                    if(x->x_deltime_list[n] > max_time)
                        max_time = x->x_deltime_list[n];
                }
                ac--, av++, n++;
            }
            x->x_list_size = n;
            if(x->x_list_size > MAXLEN)
                x->x_list_size = MAXLEN;
            if(!size_flag)
                delsize = max_time;
            argnum++;
        }
        else if(av->a_type == A_SYMBOL && !argnum){
            t_symbol * cursym = atom_getsymbolarg(0, ac, av);
            if(cursym == gensym("-size")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    delsize = atom_getfloatarg(1, ac, av);
                    size_flag = 1;
                    ac-=2, av+=2;
                }
                else
                    goto errstate;
            }
            else if(cursym == gensym("-samps")){
                x->x_ms = 0;
                ac--, av++;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    };
    if(x->x_ms)
        delsize *= x->x_sr_khz;
    if(delsize <= 0)
        delsize = 1000 * x->x_sr_khz;
    x->x_buf = NULL;
    x->x_allocsize = 0;
    x->x_nchans = x->x_chs1 = x->x_chs2 = 1;
    ffdelay_alloc(x, (unsigned int)delsize, 1);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    x->x_glist = canvas_getcurrent();
    x->x_signalscalar = obj_findsignalscalar((t_object *)x, 1);
    else_magic_setnan(x->x_signalscalar);
    x->x_clock = clock_new(x, (t_method)ffdelay_tick);
    return(x);
errstate:
    pd_error(x, "[ffdelay~]: improper args");
    return(NULL);
}

void ffdelay_tilde_setup(void){
    ffdelay_class = class_new(gensym("ffdelay~"), (t_newmethod)ffdelay_new, (t_method)ffdelay_free,
        sizeof(t_ffdelay), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(ffdelay_class, nullfn, gensym("signal"), 0);
    class_addmethod(ffdelay_class, (t_method)ffdelay_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(ffdelay_class, (t_method)ffdelay_clear, gensym("clear"), 0);
    class_addmethod(ffdelay_class, (t_method)ffdelay_size, gensym("size"), A_DEFFLOAT, 0);
    class_addmethod(ffdelay_class, (t_method)ffdelay_freeze, gensym("freeze"), A_DEFFLOAT, 0);
    class_addmethod(ffdelay_class, (t_method)ffdelay_set, gensym("set"), A_GIMME, 0);
    class_addmethod(ffdelay_class, (t_method)ffdelay_time, gensym("time"), A_GIMME, 0);
}
