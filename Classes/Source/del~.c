// based on delwrite~/delread4~

#include "m_pd.h"
#include <string.h>
extern int ugen_getsortno(void);

// ----------------------------- del~ in -----------------------------
static t_class *del_in_class;

typedef struct delwritectl{
    int         c_n;
    t_sample   *c_vec;
    int         c_phase;
}t_delwritectl;

typedef struct _del_in{
    t_object        x_obj;
    t_symbol       *x_sym;
    t_float         x_deltime;  // delay in msec (added by Mathieu Bouchard)
    t_delwritectl   x_cspace;
    int             x_sortno;   // DSP sort number at which this was last put on chain
    int             x_rsortno;  // DSP sort # for first delread or write in chain
    int             x_vecsize;  // vector size for del~ out to use
    int             x_freeze;
    t_float         x_sr;
    t_float         x_f;
//    t_outlet       *x_symout;
}t_del_in;

#define XTRASAMPS 4
#define SAMPBLK 4

static void del_in_update(t_del_in *x){ // added by Mathieu Bouchard
    int nsamps = x->x_deltime * x->x_sr * (t_float)(0.001f);
    if(nsamps < 1)
        nsamps = 1;
    nsamps += ((- nsamps) & (SAMPBLK - 1));
    nsamps += x->x_vecsize;
    if(x->x_cspace.c_n != nsamps){
        x->x_cspace.c_vec = (t_sample *)resizebytes(x->x_cspace.c_vec,
            (x->x_cspace.c_n + XTRASAMPS) * sizeof(t_sample), (nsamps + XTRASAMPS) * sizeof(t_sample));
        x->x_cspace.c_n = nsamps;
        x->x_cspace.c_phase = XTRASAMPS;
    }
}

/*static void del_in_outsym(t_del_in *x){
    if(x->x_sym)
        outlet_symbol(x->x_symout, x->x_sym);
}*/

static void del_in_clear(t_del_in *x){
    if(x->x_cspace.c_n > 0)
        memset(x->x_cspace.c_vec, 0, sizeof(t_sample)*(x->x_cspace.c_n + XTRASAMPS));
}

static void del_in_freeze(t_del_in *x, t_floatarg f){
    x->x_freeze = (int)(f != 0);
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
    t_delwritectl *c = (t_delwritectl *)(w[2]);
    int n = (int)(w[3]);
    t_del_in *x = (t_del_in *)(w[4]);
    int phase = c->c_phase, nsamps = c->c_n;
    t_sample *vp = c->c_vec; // vector point
    t_sample *bp = vp + phase; // vector point + phase = input
    t_sample *ep = vp + (nsamps + XTRASAMPS); // end point = vector point + nsamples
    phase += n; // phase + block size
    if(x->x_freeze){
        while(n--){
            bp++;
            if(bp == ep){
                vp[0] = ep[-4];
                vp[1] = ep[-3];
                vp[2] = ep[-2];
                vp[3] = ep[-1];
                bp = vp + XTRASAMPS;
                phase -= nsamps;
            }
        }
    }
    else{
        while(n--){
            t_sample f = *in++;
            if(PD_BIGORSMALL(f))
                f = 0;
            *bp++ = f;
            if(bp == ep){
                vp[0] = ep[-4];
                vp[1] = ep[-3];
                vp[2] = ep[-2];
                vp[3] = ep[-1];
                bp = vp + XTRASAMPS;
                phase -= nsamps;
            }
        }
    }
    c->c_phase = phase; // update phase
    return(w+5);
}

static void del_in_dsp(t_del_in *x, t_signal **sp){
    dsp_add(del_in_perform, 4, sp[0]->s_vec, &x->x_cspace, (t_int)sp[0]->s_n, x);
    x->x_sortno = ugen_getsortno();
    del_in_checkvecsize(x, sp[0]->s_n, sp[0]->s_sr);
    del_in_update(x);
//    del_in_outsym(x);
}

static void del_in_free(t_del_in *x){
    pd_unbind(&x->x_obj.ob_pd, x->x_sym);
    freebytes(x->x_cspace.c_vec, (x->x_cspace.c_n + XTRASAMPS) * sizeof(t_sample));
}

static void *del_in_new(t_symbol *s, int ac, t_atom *av){
    post("del_in_new ac = %d", ac);
    s = NULL;
    t_del_in *x = (t_del_in *)pd_new(del_in_class);
    x->x_deltime = 1000;
    char buf[MAXPDSTRING];
    sprintf(buf, "#%lx", (long)x);
    x->x_sym = gensym(buf);
//    x->x_sym = &s_;
    if(!ac){
        post("[del~] please define a delay line name");
    }
    else if(av->a_type == A_SYMBOL){
        x->x_sym = av->a_w.w_symbol;
        ac--, av++;
        if((av)->a_type == A_FLOAT)
            x->x_deltime = (av)->a_w.w_float;
    }
//    post("s = %s / ms = %f", x->x_sym->s_name, x->x_deltime);
    pd_bind(&x->x_obj.ob_pd, x->x_sym);
//    x->x_symout = outlet_new(&x->x_obj, &s_symbol);
    x->x_cspace.c_n = 0;
    x->x_cspace.c_vec = getbytes(XTRASAMPS * sizeof(t_sample));
    x->x_sortno = 0;
    x->x_vecsize = 0;
    x->x_sr = 0;
    x->x_f = 0;
    x->x_freeze = 0;
    return(x);
}

// ----------------------------- del~ out -----------------------------
static t_class *del_out_class;

typedef struct _del_out{
    t_object        x_obj;
    t_symbol       *x_sym;
    t_float         x_sr;       // samples per msec
    int             x_zerodel;  // 0 or vecsize depending on read/write order
    t_float         x_f;
}t_del_out;

/*static void del_out_set(t_del_out *x, t_symbol *s){
    if(s != &s_)
        x->x_sym = s;
}*/

static t_int *del_out_perform(t_int *w){
    t_sample *in = (t_sample *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    t_delwritectl *ctl = (t_delwritectl *)(w[3]);
    t_del_out *x = (t_del_out *)(w[4]);
    int n = (int)(w[5]);
    int nsamps = ctl->c_n;              // number of samples
    t_sample limit = nsamps - n;        // limit = number of samples - block size
    t_sample fn = n-1;                  // fn = block size - 1
    t_sample *vp = ctl->c_vec;          // vp (vector point?) = delay vector
    t_sample *bp;
    t_sample *wp = vp + ctl->c_phase;   // wp (write point?) = vp + phase
    t_sample zerodel = x->x_zerodel;
    if(limit < 0){ // blocksize is larger than out buffer size
        while(n--)
            *out++ = 0;
        return(w+5);
    }
    while(n--){
        t_sample delsamps = x->x_sr * *in++ - zerodel; // delay in samples
        t_sample frac; // delay point fractional part
        int idelsamps; // delay point integer part
        t_sample a, b, c, d, cminusb; // for interpolation
        if(delsamps < 1.0f)    // too small or NAN
            delsamps = 0.0f;
        if(delsamps > limit)           // too big
            delsamps = limit;
        delsamps += fn; // delay in samples + block size - 1
        fn = fn - 1.0f;
        idelsamps = delsamps;
        frac = delsamps - (t_sample)idelsamps;
        bp = wp - idelsamps; // write point - delay point
        if(bp < vp + XTRASAMPS)
            bp += nsamps;
        d = bp[-3];
        c = bp[-2];
        b = bp[-1];
        a = bp[0];
        cminusb = c-b;
        *out++ = b + frac * (
            cminusb - 0.1666667f * (1.-frac) * (
                (d - a - 3.0f * cminusb) * frac + (d + 2.0f*a - 3.0f*b)
            )
        );
    }
    return(w+6);
}

static void del_out_dsp(t_del_out *x, t_signal **sp){
    t_del_in *delwriter = (t_del_in *)pd_findbyclass(x->x_sym, del_in_class);
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
        pd_error(x, "del~: %s: no such delay line",x->x_sym->s_name);
}

static void *del_out_new(t_symbol *s, int ac, t_atom *av){
    t_del_out *x = (t_del_out *)pd_new(del_out_class);
    post("del_out_new / ac = %d", ac);
    s = NULL;
    x->x_sym = &s_;
    
    if(ac && av->a_type == A_SYMBOL){
        x->x_sym = av->a_w.w_symbol;
        post("x->x_sym = %s", x->x_sym->s_name);
        ac--;
        av++;
    }
    
    x->x_sr = 1;
    x->x_zerodel = 0;
    outlet_new(&x->x_obj, &s_signal);
    x->x_f = 0;
    return(x);
}

// ----------------------- global setup ----------------
static void *del_new(t_pd *dummy, t_symbol *s, int ac, t_atom *av){
    dummy = NULL;
    post("del~_new / ac = %d", ac);
    if(!ac){
        post("[del~] please define a delay line name");
        pd_this->pd_newest = del_in_new(s, ac, av);
    }
    else{
        post("del~_new / else (ac)");
        t_symbol *s2 = av[0].a_w.w_symbol;
        if(s2 == gensym("in")){
            post("in");
            pd_this->pd_newest = del_in_new(s, ac-1, av+1);
        }
        else if(s2 == gensym("out")){
            post("out");
            pd_this->pd_newest = del_out_new(s, ac-1, av+1);
        }
        else{
            post("else / neither in or out");
            pd_this->pd_newest = del_in_new(s, ac, av);
        }
    }
    return(pd_this->pd_newest);
}

void del_tilde_setup(void){
    del_in_class = class_new(gensym("del~ in"), (t_newmethod)del_in_new,
        (t_method)del_in_free, sizeof(t_del_in), 0, A_GIMME, 0);
    class_addcreator((t_newmethod)del_new, gensym("del~"), A_GIMME, 0);
    CLASS_MAINSIGNALIN(del_in_class, t_del_in, x_f);
//    class_addbang(del_in_class, del_in_outsym);
    class_addmethod(del_in_class, (t_method)del_in_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(del_in_class, (t_method)del_in_clear, gensym("clear"), 0);
    class_addmethod(del_in_class, (t_method)del_in_freeze, gensym("freeze"), A_FLOAT, 0);

    del_out_class = class_new(gensym("del~ out"), (t_newmethod)del_out_new, 0,
        sizeof(t_del_out), 0, A_GIMME, 0);
    CLASS_MAINSIGNALIN(del_out_class, t_del_out, x_f);
    class_addmethod(del_out_class, (t_method)del_out_dsp, gensym("dsp"), A_CANT, 0);
//    class_addsymbol(del_out_class, del_out_set);
}
