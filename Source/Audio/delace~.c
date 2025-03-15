// porres

#include <m_pd.h>
#include <math.h>

static t_class *delace_class;

typedef struct _delace{
    t_object x_obj;
    t_int    x_nouts;
    int         x_zero;
    t_symbol   *x_ignore;
}t_delace;

static void delace_dsp(t_delace *x, t_signal **sp){
    int i, j, k, n = sp[0]->s_n, inchs = sp[0]->s_nchans;
    int chs = (int)ceil((float)inchs / (float)x->x_nouts);
    int step = (int)ceil((float)inchs / (float)chs);
    for(i = 0; i < x->x_nouts; i++){
        if(x->x_zero){
            signal_setmultiout(&sp[i+1], chs);
            for(j = 0, k = i; k < inchs; j++, k += step)
                dsp_add_copy(sp[0]->s_vec+k*n, sp[i+1]->s_vec+j*n, n);
        }
        else{
            j = 0;
            for(k = i; k < inchs; k += step)
                j++;
            signal_setmultiout(&sp[i+1], j);
            for(j = 0, k = i; k < inchs; j++, k += step)
                dsp_add_copy(sp[0]->s_vec+k*n, sp[i+1]->s_vec+j*n, n);
        }
    }
}

static void *delace_new(t_symbol *s, int ac, t_atom* av){
    t_delace *x = (t_delace *)pd_new(delace_class);
    x->x_ignore = s;
    int n = 2;
    x->x_zero = 0;
    if(av->a_type == A_SYMBOL){
        post("if delace");
        if(atom_getsymbol(av) == gensym("-z")){
            x->x_zero = 1;
            ac--, av++;
        }
        else{
            post("else delace");
            goto errstate;
        }
    }
    if(ac)
        n = atom_getint(av);
    x->x_nouts = n < 2 ? 2 : n;
    for(int i = 0; i < x->x_nouts; i++)
        outlet_new(&x->x_obj, &s_signal);
    return(x);
errstate:
    pd_error(x, "[delace~]: improper args");
    return(NULL);
}

void delace_tilde_setup(void){
    delace_class = class_new(gensym("delace~"), (t_newmethod)delace_new,
        0, sizeof(t_delace), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(delace_class, nullfn, gensym("signal"), 0);
    class_addmethod(delace_class, (t_method)delace_dsp, gensym("dsp"), 0);
}
