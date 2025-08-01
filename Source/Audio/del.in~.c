// based on delwrite~/delread4~

#include <m_pd.h>
#include <g_canvas.h>
#include <buffer.h>
#include <string.h>
extern int ugen_getsortno(void);

static t_class *del_in_class;

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

static void del_in_freeze(t_del_in *x, t_floatarg f){
    x->x_freeze = (int)(f != 0);
}

static void del_in_clear(t_del_in *x){
    if(x->x_cspace.c_n > 0)
        memset(x->x_cspace.c_vec, 0, sizeof(t_sample)*(x->x_cspace.c_n + XTRASAMPS));
}

static void del_in_size(t_del_in *x, t_floatarg f){
    if(f < 0)
        f = 0;
    if(f != x->x_deltime){
        x->x_deltime = f;
        del_in_clear(x);
        del_in_update(x);
    }
}

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

static t_int *del_in_perform(t_int *w){
    t_sample *in = (t_sample *)(w[1]);
    t_delwritectl *c = (t_delwritectl *)(w[3]);
    int n = (int)(w[4]);
    t_del_in *x = (t_del_in *)(w[5]);
    int phase = c->c_phase;                     // phase
    int nsamps = c->c_n;                        // number of samples
    t_sample *vp = c->c_vec;                    // vector
    t_sample *bp = vp + phase;                  // buffer point = vector + phase
    t_sample *ep = vp + (nsamps + XTRASAMPS);   // end point = vector point + nsamples
    phase += n;                                 // phase += block size
    while(n--){
        if(x->x_freeze)
            bp++; // just advance buffer point
        else{
            t_sample f = *in++;
            *bp++ = PD_BIGORSMALL(f) ? 0.f : f; // advance and write input into buffer point
        }
        if(bp == ep){
            vp[0] = ep[-4]; // copy guard points
            vp[1] = ep[-3];
            vp[2] = ep[-2];
            vp[3] = ep[-1];
            bp = vp + XTRASAMPS; // go back to the beginning
            phase -= nsamps;     // go back to the beginning
        }
    }
    c->c_phase = phase; // update phase
    return(w+6);
}

static void del_in_dsp(t_del_in *x, t_signal **sp){
    dsp_add(del_in_perform, 5, sp[0]->s_vec, sp[1]->s_vec, &x->x_cspace, (t_int)sp[0]->s_n, x);
    x->x_sortno = ugen_getsortno();
    del_in_checkvecsize(x, sp[0]->s_n, sp[0]->s_sr);
    del_in_update(x);
}

static void del_in_free(t_del_in *x){
    pd_unbind(&x->x_obj.ob_pd, x->x_sym);
    freebytes(x->x_cspace.c_vec, (x->x_cspace.c_n + XTRASAMPS) * sizeof(t_sample));
}

static void *del_in_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_del_in *x = (t_del_in *)pd_new(del_in_class);
    x->x_deltime = 1000;
    x->x_ms = 1;
    int argn = 0;
    t_canvas *canvas = canvas_getrootfor(canvas_getcurrent());
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING, "$0-delay-.x%lx.c", (long unsigned int)canvas);
    x->x_sym = canvas_realizedollar(canvas, gensym(buf));
    if(ac){
        if(av->a_type == A_FLOAT){
            x->x_deltime = (av)->a_w.w_float;
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
                        x->x_deltime = (av)->a_w.w_float;
                        ac--, av++;
                    }
                    else
                        goto errstate;
                    if(ac)
                        goto errstate;
                }
            }
            else if((av)->a_type == A_FLOAT){
                x->x_deltime = (av)->a_w.w_float;
                ac--, av++;
                argn = 1;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
        
    }
    pd_bind(&x->x_obj.ob_pd, x->x_sym);
    x->x_cspace.c_n = 0;
    x->x_cspace.c_vec = getbytes(XTRASAMPS * sizeof(t_sample));
    x->x_sortno = 0;
    x->x_vecsize = 0;
    x->x_sr = 0;
    x->x_f = 0;
    x->x_freeze = 0;
    outlet_new(&x->x_obj, &s_signal);
    return(x);
    errstate:
        pd_error(x, "[del.in~]: improper args");
        return(NULL);
}

void setup_del0x2ein_tilde(void){
    del_in_class = class_new(gensym("del.in~"), (t_newmethod)del_in_new,
        (t_method)del_in_free, sizeof(t_del_in), 0, A_GIMME, 0);
    CLASS_MAINSIGNALIN(del_in_class, t_del_in, x_f);
    class_addmethod(del_in_class, (t_method)del_in_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(del_in_class, (t_method)del_in_clear, gensym("clear"), 0);
    class_addmethod(del_in_class, (t_method)del_in_freeze, gensym("freeze"), A_DEFFLOAT, 0);
    class_addmethod(del_in_class, (t_method)del_in_size, gensym("size"), A_DEFFLOAT, 0);
}
