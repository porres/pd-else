// based on delwrite~/delread4~

#include <m_pd.h>
#include <g_canvas.h>
#include <buffer.h>
#include <string.h>
extern int ugen_getsortno(void);

static t_class *del_out_class;

typedef struct delwritectl{
    int             c_n;        // number of samples
    t_sample       *c_vec;      // vector
    int             c_phase;    // phase
}t_delwritectl;

typedef struct _del_in{
    t_object        x_obj;
    t_symbol       *x_sym;
    t_float         x_deltime;  // delay size
    t_delwritectl   x_cspace;
    int             x_sortno;   // DSP sort number at which this was last put on chain
    int             x_rsortno;  // DSP sort # for first delread or write in chain
    int             x_vecsize;  // vector size for del~ out to use
    int             x_freeze;
    unsigned int    x_ms;       // ms flag
    unsigned int    x_maxsize;  // buffer size in samples
    unsigned int    x_maxsofar; // largest maxsize so far
    t_float         x_sr;
    t_float         x_f;
}t_del_in;

#define XTRASAMPS 4 // extra number of samples (for guard points)
#define SAMPBLK   4 // ???

typedef struct _del_out{
    t_object        x_obj;
    t_symbol       *x_sym;
    t_float         x_sr;       // samples per msec
    int             x_zerodel;  // 0 or vecsize depending on read/write order
    unsigned int    x_ms;       // ms flag
    t_float         x_f;
}t_del_out;

// check that all ins/outs have the same vecsize
static void del_in_checkvecsize(t_del_in *x, int vecsize, t_float sr){
    if(x->x_rsortno != ugen_getsortno()){
        x->x_vecsize = vecsize;
        x->x_sr = sr;
        x->x_rsortno = ugen_getsortno();
    }
    else{ // Subsequent objects are only allowed to increase the vector size/samplerate
        if(vecsize > x->x_vecsize)
            x->x_vecsize = vecsize;
        if(sr > x->x_sr)
            x->x_sr = sr;
    }
}

static void del_in_update(t_del_in *x){ // added by Mathieu Bouchard
    int nsamps = x->x_deltime;
    if(x->x_ms)
        nsamps *= (x->x_sr * (t_float)(0.001f));
    if(nsamps < 1)
        nsamps = 1;
    nsamps += ((-nsamps) & (SAMPBLK - 1));
    nsamps += x->x_vecsize;
    if(x->x_cspace.c_n != nsamps){
        x->x_cspace.c_vec = (t_sample *)resizebytes(x->x_cspace.c_vec,
            (x->x_cspace.c_n + XTRASAMPS) * sizeof(t_sample), (nsamps + XTRASAMPS) * sizeof(t_sample));
        x->x_cspace.c_n = nsamps;
        x->x_cspace.c_phase = XTRASAMPS;
    }
}

static t_int *del_out_perform(t_int *w){
    t_sample *in = (t_sample *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    t_delwritectl *ctl = (t_delwritectl *)(w[3]);
    t_del_out *x = (t_del_out *)(w[4]);
    int n = (int)(w[5]);
    int nsamps = ctl->c_n;              // number of samples
    t_sample limit = nsamps - n;        // limit = number of samples - block size
    t_sample nm1 = n-1;                 // nm1 = block size - 1
    t_sample *vp = ctl->c_vec;          // vector (buffer memory of delay lne)
    t_sample *bp;
    t_sample *wp = vp + ctl->c_phase;   // wp (write point) = vp + phase
    t_sample zerodel = x->x_zerodel;
    if(limit < 0){      // blocksize is larger than out buffer size
        while(n--)
            *out++ = 0; // output zeros
        return(w+6);
    }
    while(n--){
        t_sample samps = *in++;      // delay in samples = input
        if(x->x_ms)                     // if input is in ms
            samps *= x->x_sr;        // convert to samples
        samps -= zerodel;            // compensate for order of execution
        if(samps < 0.0f)
            samps = 0.0f;
        else if(samps > limit)
            samps = limit;
        samps += nm1;                             // delay in samples + block size - 1
        nm1--;                                    // ???
        int isamps = (int)samps;                  // delay point integer part
        t_sample frac = samps - (t_sample)isamps; // delay point fractional part
        bp = wp - isamps;                         // buffer point = write point - integer delay point
        if(bp < vp + XTRASAMPS)                   // if less than beegining, wrap to upper point
            bp += nsamps;
        t_sample a = bp[0], b = bp[-1], c = bp[-2], d = bp[-3];
        *out++ = interp_spline(frac, a, b, c, d);
    }
    return(w+6);
}

static void del_out_dsp(t_del_out *x, t_signal **sp){
    t_del_in *delwriter = (t_del_in *)pd_findbyclassname(x->x_sym, gensym("del.in~"));
    x->x_sr = sp[0]->s_sr * 0.001;
    if(delwriter){
        del_in_checkvecsize(delwriter, sp[0]->s_n, sp[0]->s_sr);
        del_in_update(delwriter);
        x->x_zerodel = (delwriter->x_sortno == ugen_getsortno() ? 0 : delwriter->x_vecsize);
        dsp_add(del_out_perform, 5, sp[0]->s_vec, sp[1]->s_vec, &delwriter->x_cspace, x, (t_int)sp[0]->s_n);
        // check block size - but only if delwriter has been initialized
        if(delwriter->x_cspace.c_n > 0 && sp[0]->s_n > delwriter->x_cspace.c_n)
            pd_error(x, "%s: read blocksize larger than write buffer", x->x_sym->s_name);
    }
    else if(*x->x_sym->s_name)
        pd_error(x, "[del~ out]: %s: no such delay line", x->x_sym->s_name);
}

static void *del_out_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_del_out *x = (t_del_out *)pd_new(del_out_class);
    t_canvas *canvas = canvas_getrootfor(canvas_getcurrent());
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING, "$0-delay-.x%lx.c", (long unsigned int)canvas);
    x->x_sym = canvas_realizedollar(canvas, gensym(buf));
    x->x_ms = 1;
    x->x_sr = 0;
    x->x_zerodel = 0;
    int argn = 0;
    if(ac){
        if(av->a_type == A_FLOAT){
            x->x_f = (av)->a_w.w_float;
            ac--, av++;
            if(ac)
                goto
                    errstate;
        }
        else if(av->a_type == A_SYMBOL){
            if(atom_getsymbolarg(0, ac, av) == gensym("-samps")){
                x->x_ms = 0;
                ac--, av++;
            }
            if(av->a_type == A_SYMBOL){
                x->x_sym = atom_getsymbolarg(0, ac, av);
                ac--, av++;
                if(ac){
                    if((av)->a_type == A_FLOAT){
                        x->x_f = (av)->a_w.w_float;
                        ac--, av++;
                    }
                    else
                        goto errstate;
                    if(ac)
                        goto errstate;
                }
            }
            else if((av)->a_type == A_FLOAT){
                x->x_f = (av)->a_w.w_float;
                ac--, av++;
                argn = 1;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
        
    }
    outlet_new(&x->x_obj, &s_signal);
    return(x);
    errstate:
        pd_error(x, "[del.out~]: improper args");
        return(NULL);
}

void setup_del0x2eout_tilde(void){
    del_out_class = class_new(gensym("del.out~"), (t_newmethod)del_out_new, 0, sizeof(t_del_out), 0, A_GIMME, 0);
    CLASS_MAINSIGNALIN(del_out_class, t_del_out, x_f);
    class_addmethod(del_out_class, (t_method)del_out_dsp, gensym("dsp"), A_CANT, 0);
}
