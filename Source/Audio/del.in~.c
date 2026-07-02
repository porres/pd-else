// based on delwrite~

#include "m_pd.h"
#include "del.h"

static t_class *delin_class;

static t_int *delin_perform(t_int *w){
    t_delin *x = (t_delin *)(w[1]);
    t_sample *in = (t_sample *)(w[2]);
    t_delinctl *c = (t_delinctl *)(w[3]);
    int n = x->x_n, phase;
    for(int i = 0; i < c->c_nchans; i++){
        t_sample *vp = c->c_vec + i * (c->c_n + XTRASAMPS), *bp, *ep;
        int k = n;
        phase = c->c_phase;
        bp = vp + phase;
        ep = vp + (c->c_n + XTRASAMPS);
        phase += k;
        while(k--){
            if(!x->x_freeze){
                t_sample f = *in++;
                *bp = PD_BIGORSMALL(f) ? 0.f : f;
            }
            bp++;
            if(bp == ep){
                vp[0] = ep[-4];
                vp[1] = ep[-3];
                vp[2] = ep[-2];
                vp[3] = ep[-1];
                bp = vp + XTRASAMPS;
                phase -= c->c_n;
            }
        }
    }
    c->c_phase = phase;
    return(w+4);
}

static void delin_clear (t_delin *x){
    if(x->x_cspace.c_n > 0)
        memset(x->x_cspace.c_vec, 0,
            (x->x_cspace.c_n + XTRASAMPS) * x->x_cspace.c_nchans * sizeof(t_sample));
}

static void delin_freeze(t_delin *x, t_floatarg f){
    x->x_freeze = (int)(f != 0);
}

static void delin_size(t_delin *x, t_floatarg f){
    if(f < 0)
        f = 0;
    if(f != x->x_deltime){
        x->x_deltime = f;
        delin_clear(x);
        del_update(x);
    }
}

static void delin_dsp(t_delin *x, t_signal **sp){
    x->x_nchans = sp[0]->s_nchans;
    x->x_n = sp[0]->s_length;
    x->x_sortno = ugen_getsortno();
    del_check(x, x->x_n, sp[0]->s_sr);
    del_update(x); // don't pass buf pointer as it can be resized by del.out~
    signal_setmultiout(&sp[1], 1);
    dsp_add_zero(sp[1]->s_vec, x->x_n);
    dsp_add(delin_perform, 3, x, sp[0]->s_vec, &x->x_cspace);
}

static void delin_free(t_delin *x){
    pd_unbind(&x->x_obj.ob_pd, x->x_sym);
    freebytes(x->x_cspace.c_vec,
        (x->x_cspace.c_n + XTRASAMPS) * x->x_cspace.c_nchans * sizeof(t_sample));
}

static void *delin_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_delin *x = (t_delin *)pd_new(delin_class);
    x->x_deltime = 1000;
    x->x_ms = 1;
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
                x->x_sym = atom_getsymbol(av);
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
            }
            else
                goto errstate;
        }
        else
            goto errstate;
        
    }
    pd_bind(&x->x_obj.ob_pd, x->x_sym);
    x->x_cspace.c_n = 0;
    x->x_cspace.c_nchans = 1;
    x->x_cspace.c_vec = getbytes(XTRASAMPS * sizeof(t_sample));
    x->x_sortno = 0;
    x->x_vecsize = 0;
    x->x_nchans = 1;
    x->x_sr = 0;
    x->x_freeze = 0;
    outlet_new(&x->x_obj, &s_signal);
    return(x);
    errstate:
        pd_error(x, "[del.in~]: improper args");
        return(NULL);
}

void setup_del0x2ein_tilde(void){
    delin_class = class_new(gensym("del.in~"), (t_newmethod)(void*)delin_new,
        (t_method)delin_free, sizeof(t_delin), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(delin_class, nullfn, gensym("signal"), 0);
    class_addmethod(delin_class, (t_method)delin_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(delin_class, (t_method)delin_clear, gensym("clear"), 0);
    class_addmethod(delin_class, (t_method)delin_freeze, gensym("freeze"), A_FLOAT, 0);
    class_addmethod(delin_class, (t_method)delin_size, gensym("size"), A_FLOAT, 0);
}
