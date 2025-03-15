// porres

#include <m_pd.h>
#include <stdlib.h>

static t_class *lace_class;

typedef struct _lace{
    t_object x_obj;
    t_int   *x_nch;   // number of channels in each input
    t_int   *x_totch;
    t_int    x_n_ins;
    t_int    x_zero;
    t_symbol *x_ignore;
}t_lace;

static void lace_dsp(t_lace *x, t_signal **sp){
    int n = sp[0]->s_n, i, maxch, totchs = 0, offset = 0;
    if(x->x_zero){
        for(i = 0; i < x->x_n_ins; i++){
            int chs = x->x_nch[i] = sp[i]->s_nchans;
            if(chs > maxch)
                maxch = chs;
        }
        signal_setmultiout(&sp[x->x_n_ins], maxch * x->x_n_ins);
        for(int ch = 0; ch < maxch; ch++){
            for(i = 0; i < x->x_n_ins; i++){
                int outpos = (offset++)*n;
                if(ch < x->x_nch[i])
                    dsp_add_copy(sp[i]->s_vec+ch*n, sp[x->x_n_ins]->s_vec+outpos, n);
                else
                    dsp_add_zero(sp[x->x_n_ins]->s_vec+outpos, n);
            }
        }
    }
    else{
        for(i = 0; i < x->x_n_ins; i++){
            int chs = x->x_nch[i] = sp[i]->s_nchans;
            x->x_totch[i+1] = (totchs += chs);
            if(chs > maxch)
                maxch = chs;
        }
        signal_setmultiout(&sp[x->x_n_ins], totchs);
        for(int ch = 0; ch < maxch; ch++){
            for(i = 0; i < x->x_n_ins; i++){
                if(ch < x->x_nch[i]){
                    int outpos = (offset++)*n;
                    dsp_add_copy(sp[i]->s_vec+ch*n, sp[x->x_n_ins]->s_vec+outpos, n);
                }
            }
        }
    }
}

void lace_free(t_lace *x){
    freebytes(x->x_nch, x->x_n_ins * sizeof(*x->x_nch));
    freebytes(x->x_totch, (x->x_n_ins+1) * sizeof(*x->x_nch));
}

static void *lace_new(t_symbol *s, int ac, t_atom* av){
    t_lace *x = (t_lace *)pd_new(lace_class);
    x->x_ignore = s;
    int n = 2;
    if(av->a_type == A_SYMBOL){
        post("if lace");
        if(atom_getsymbol(av) == gensym("-z")){
            x->x_zero = 1;
            ac--, av++;
        }
        else{
            post("else lace");
            goto errstate;
        }
    }
    if(av->a_type == A_FLOAT)
        n = atom_getint(av);
    x->x_n_ins = n < 2 ? 2 : n;
    x->x_nch = (t_int *)getbytes(x->x_n_ins * sizeof(*x->x_nch));
    x->x_totch = (t_int *)getbytes((x->x_n_ins+1) * sizeof(*x->x_nch));
    x->x_totch[0] = 0;
    for(int i = 1; i < x->x_n_ins; i++)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    return(x);
errstate:
    pd_error(x, "[lace~]: improper args");
    return(NULL);
}

void lace_tilde_setup(void){
    lace_class = class_new(gensym("lace~"), (t_newmethod)lace_new,
        (t_method)lace_free, sizeof(t_lace), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(lace_class, nullfn, gensym("signal"), 0);
    class_addmethod(lace_class, (t_method)lace_dsp, gensym("dsp"), 0);
}
