// limit~ - lookahead limiter stolen and adapted from SuperCollider

#include <m_pd.h>
#include <math.h>
#include <string.h>

// per-channel state: a triple buffer (in/mid/out segments of one block, rotated)
// plus the running gain-ramp state, mirroring LIMITER_Ctor/LIMITER_next in SC.
typedef struct _lchan{
    t_float *l_table;  // raw allocation: 3 * bufsize floats
    t_float *l_xin;    // segment currently being written (incoming audio)
    t_float *l_xmid;   // segment "in flight" (written last block, being analyzed)
    t_float *l_xout;   // segment being played out now (already gain-scanned)
    long     l_pos;    // write/read position within current bufsize
    int      l_flips;  // how many times buffers have rotated (need >=2 before output starts)
    t_float  l_slope;  // per-sample gain increment for this block
    t_float  l_level;  // current gain
    t_float  l_prevmax; // peak abs value found 2 buffers ago
    t_float  l_curmax;  // peak abs value being accumulated for the buffer being written
}t_lchan;

typedef struct _limit{
    t_object  x_obj;
    void     *x_outlet;
    t_float   x_amp;      // limiting threshold/ceiling, hot-settable, read every block
    t_float   x_dur;      // lookahead duration in ms, settable but triggers realloc
    long      x_bufsize;  // lookahead buffer size in samples
    t_float   x_sr;
    int       x_nchans;
    int       x_db;
    t_lchan  *x_chans;
}t_limit;

static t_class *limit_class;

// (re)allocate per-channel buffers for a new channel count / buffer size.
// Any channels that existed before are reset (their table pointers change size,
// so there's no sane way to preserve in-flight audio across a resize).
static void limit_resize(t_limit *x, int nchans, long bufsize){
    if(bufsize < 1)
        bufsize = 1;
    if(x->x_chans){
        for(int j = 0; j < x->x_nchans; j++)
            if(x->x_chans[j].l_table)
                freebytes(x->x_chans[j].l_table, 3 * x->x_bufsize * sizeof(t_float));
        x->x_chans = (t_lchan *)resizebytes(x->x_chans,
            x->x_nchans * sizeof(t_lchan), nchans * sizeof(t_lchan));
    }
    else
        x->x_chans = (t_lchan *)getbytes(nchans * sizeof(t_lchan));
    for(int j = 0; j < nchans; j++){
        t_lchan *c = &x->x_chans[j];
        c->l_table = (t_float *)getbytes(3 * bufsize * sizeof(t_float));
        memset(c->l_table, 0, 3 * bufsize * sizeof(t_float));
        c->l_xin  = c->l_table;
        c->l_xmid = c->l_table + bufsize;
        c->l_xout = c->l_table + 2 * bufsize;
        c->l_pos = 0;
        c->l_flips = 0;
        c->l_slope = 0;
        c->l_level = 1;
        c->l_prevmax = 0;
        c->l_curmax = 0;
    }
    x->x_nchans = nchans;
    x->x_bufsize = bufsize;
}

static t_int *limit_perform(t_int *w){
    t_limit *x = (t_limit *)(w[1]);
    int n = (int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    t_float amp = x->x_amp;
    long bufsize = x->x_bufsize;
    t_float slopefactor = bufsize > 0 ? 1.0f / (t_float)bufsize : 0;
    for(int j = 0; j < x->x_nchans; j++){
        t_float *inp = in + j*n;
        t_float *outp = out + j*n;
        if(bufsize <= 0 || !x->x_chans){ // not ready yet (???)
            for(int i = 0; i < n; i++)
                outp[i] = 0;
            continue;
        }
        t_lchan *c = &x->x_chans[j];
        t_float *xin = c->l_xin, *xmid = c->l_xmid, *xout = c->l_xout;
        long pos = c->l_pos;
        int flips = c->l_flips;
        t_float slope = c->l_slope, level = c->l_level;
        t_float prevmax = c->l_prevmax, curmax = c->l_curmax;
        for(int i = 0; i < n; i++){
            t_float xn = inp[i];
            xin[pos] = xn;
            outp[i] = flips >= 2 ? level * xout[pos] : 0.f; // lookahead: silence until primed
            level += slope;
            t_float val = fabsf(xn);
            if(val > curmax)
                curmax = val;
            pos++;
            if(pos >= bufsize){ // buffer full: rotate and compute a new gain ramp
                pos = 0;
                t_float maxval2 = prevmax > curmax ? prevmax : curmax;
                prevmax = curmax;
                curmax = 0;
                t_float next_level = maxval2 > amp ? amp / maxval2 : 1.0f;
                slope = (next_level - level) * slopefactor;
                t_float *tmp = xout;
                xout = xmid;
                xmid = xin;
                xin = tmp;
                flips++;
            }
        }
        c->l_pos = pos;
        c->l_flips = flips;
        c->l_slope = slope;
        c->l_level = level;
        c->l_prevmax = prevmax;
        c->l_curmax = curmax;
        c->l_xin = xin;
        c->l_xmid = xmid;
        c->l_xout = xout;
    }
    return(w+5);
}

static void limit_dsp(t_limit *x, t_signal **sp){
    x->x_sr = sp[0]->s_sr;
    int chs = sp[0]->s_nchans, n = sp[0]->s_n;
    long bufsize = (long)ceil(x->x_dur * x->x_sr/1000);
    if(bufsize < 1)
        bufsize = 1;
    if(chs != x->x_nchans || bufsize != x->x_bufsize)
        limit_resize(x, chs, bufsize);
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(limit_perform, 4, x, n, sp[0]->s_vec, sp[1]->s_vec);
}

static void limit_dur(t_limit *x, t_floatarg f){
    x->x_dur = f * 0.5;
    if(x->x_dur < 0.1) // MINDUR
        x->x_dur = 0.1;
    long bufsize = (long)ceil(x->x_dur * x->x_sr/1000);
    if(bufsize < 1)
        bufsize = 1;
    if(bufsize != x->x_bufsize)
        limit_resize(x, x->x_nchans, bufsize);
}

static void limit_amp(t_limit *x, t_floatarg f){
    x->x_amp = x->x_db ? pow(10, f / 20) : f;
    
}

static void limit_db(t_limit *x, t_floatarg f){
    x->x_db = f != 0;
}

static void limit_clear(t_limit *x){ // reset state/buffers
    for(int j = 0; j < x->x_nchans; j++){
        t_lchan *c = &x->x_chans[j];
        if(c->l_table)
            memset(c->l_table, 0, 3 * x->x_bufsize * sizeof(t_float));
        c->l_xin  = c->l_table;
        c->l_xmid = c->l_table + x->x_bufsize;
        c->l_xout = c->l_table + 2 * x->x_bufsize;
        c->l_pos = 0;
        c->l_flips = 0;
        c->l_slope = 0;
        c->l_level = 1;
        c->l_prevmax = 0;
        c->l_curmax = 0;
    }
}

static void *limit_free(t_limit *x){
    if(x->x_chans){
        for(int j = 0; j < x->x_nchans; j++)
            if(x->x_chans[j].l_table)
                freebytes(x->x_chans[j].l_table, 3 * x->x_bufsize * sizeof(t_float));
        freebytes(x->x_chans, x->x_nchans * sizeof(t_lchan));
    }
    return(void *)x;
}

static void *limit_new(t_symbol *s, int ac, t_atom *av){
    t_limit *x = (t_limit *)pd_new(limit_class);
    x->x_db = 0;
    float amp = 1;
    float dur = 1;
    x->x_sr = sys_getsr();
    x->x_bufsize = 0;
    x->x_nchans = 1;
    x->x_chans = NULL;
    int argnum = 0;
    while(ac > 0){
        if(av->a_type == A_FLOAT){
            t_float aval = atom_getfloat(av);
            switch(argnum){
                case 0:
                    amp = aval;
                    break;
                case 1:
                    dur = aval;
                    break;
                default:
                    break;
            };
            argnum++;
            ac--, av++;
        }
        else if(av->a_type == A_SYMBOL && !argnum){
            if(atom_getsymbol(av) == gensym("-db")){
                x->x_db = 1;
                amp = 0;
                ac--, av++;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    };
    limit_amp(x, amp);
    limit_dur(x, dur);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("amp"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_float, gensym("dur"));
    outlet_new(&x->x_obj, gensym("signal"));
    return(x);
errstate:
    pd_error(x, "[limit~]: improper args");
    return(NULL);
}

void limit_tilde_setup(void){
    limit_class = class_new(gensym("limit~"), (t_newmethod)limit_new,
        (t_method)limit_free, sizeof(t_limit), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(limit_class, nullfn, gensym("signal"), 0);
    class_addmethod(limit_class, (t_method)limit_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(limit_class, (t_method)limit_dur, gensym("dur"), A_FLOAT, 0);
    class_addmethod(limit_class, (t_method)limit_amp, gensym("amp"), A_FLOAT, 0);
    class_addmethod(limit_class, (t_method)limit_db, gensym("db"), A_FLOAT, 0);
    class_addmethod(limit_class, (t_method)limit_clear, gensym("clear"), 0);
}
