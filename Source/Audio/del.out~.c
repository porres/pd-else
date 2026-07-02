// based on delread4~

#include "m_pd.h"
#include "del.h"

static t_class *delout_class;

static t_int *delout_perform(t_int *w){
    t_sample *in = (t_sample *)(w[1]);
    t_sample *out = (t_sample *)(w[2]);
    t_delinctl *ctl = (t_delinctl *)(w[3]);
    t_delout *x = (t_delout *)(w[4]);
    int chn = (int)(w[5]);
    int n = (int)(w[6]);
    int nsamps = ctl->c_n;
    t_sample sr = x->x_sr;
    t_sample limit = nsamps - n;
    t_sample nm1 = n-1;
    t_sample *vp = ctl->c_vec + chn * (nsamps + XTRASAMPS),
        *bp, *wp = vp + ctl->c_phase;
    t_sample zerodel = x->x_zerodel;
    if(limit < 0){ // blocksize is larger than delout~ buffer size
        while(n--)
            *out++ = 0;
        return(w+7);
    }
    while(n--){
        t_sample samps = *in++;
        if(x->x_ms)
            samps *= sr;
        samps -= zerodel;
        if(samps < 0.0f)
            samps = 0.0f;
        else if(samps > limit)
            samps = limit;
        samps += nm1;
        nm1 -= 1.0f;
        int isamps = (int)samps;
        t_sample frac = samps - (t_sample)isamps;
        bp = wp - isamps;
        if(bp < vp + XTRASAMPS)
            bp += nsamps;
        t_sample a = bp[0], b = bp[-1], c = bp[-2], d = bp[-3];
        *out++ = interp_spline(frac, a, b, c, d);
    }
    return(w+7);
}

static void delout_set(t_delout *x, t_symbol *name){
    x->x_sym = name;
    canvas_update_dsp();
}

static void delout_dsp(t_delout *x, t_signal **sp){
    t_delin *delwriter = (t_delin *)pd_findbyclassname(x->x_sym, gensym("del.in~"));
    int i, length = sp[0]->s_length, nchans;
    x->x_sr = sp[0]->s_sr * 0.001;
    if(delwriter){
            /* The output channel count is the maximum of the phase input channels
            and the delin~ channels. The smaller one simply wraps around. */
        nchans = sp[0]->s_nchans > delwriter->x_nchans ?
            sp[0]->s_nchans : delwriter->x_nchans;
        signal_setmultiout(&sp[1], nchans);
        del_check(delwriter, length, sp[0]->s_sr);
        del_update(delwriter);
        x->x_zerodel = (delwriter->x_sortno == ugen_getsortno() ?
            0 : delwriter->x_vecsize);
            /* NB: do not pass a direct pointer to the delay buffer because
            it might get resized by another object, see del_update() */
        for(i = 0; i < nchans; i++)
            dsp_add(delout_perform, 6,
                sp[0]->s_vec + (i % sp[0]->s_nchans) * length,
                    sp[1]->s_vec + i * length, &delwriter->x_cspace,
                        x, (t_int)(i % delwriter->x_nchans), (t_int)length);
        // check block size - but only if delwriter has been initialized
        if(delwriter->x_cspace.c_n > 0 && length > delwriter->x_cspace.c_n)
            pd_error(x, "[del.out~] %s: blocksize larger than [del.in~] buffer", x->x_sym->s_name);
    }
    else{
        if (*x->x_sym->s_name)
            pd_error(x, "[del.out~] %s: no such [del.in~]",x->x_sym->s_name);
        signal_setmultiout(&sp[1], 1);
        dsp_add_zero(sp[1]->s_vec, length);
    }
}

static void *delout_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_delout *x = (t_delout *)pd_new(delout_class);
    t_canvas *canvas = canvas_getrootfor(canvas_getcurrent());
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING, "$0-delay-.x%lx.c", (long unsigned int)canvas);
    x->x_sym = canvas_realizedollar(canvas, gensym(buf));
    x->x_sr = 1;
    x->x_ms = 1;
    x->x_zerodel = 0;
    x->x_f = 0;
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
                x->x_sym = atom_getsymbol(av);
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
    delout_class = class_new(gensym("del.out~"), (t_newmethod)(void*)delout_new, 0,
        sizeof(t_delout), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(delout_class, t_delout, x_f);
    class_addmethod(delout_class, (t_method)delout_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(delout_class, (t_method)delout_set, gensym("set"), A_SYMBOL, 0);
}
