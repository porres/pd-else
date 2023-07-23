// Porres 2023

#include "m_pd.h"

static t_class *sum_class;

typedef struct _sum{
    t_object  x_obj;
    int       x_sum;
}t_sum;

static void sum_sum(t_sum *x, t_floatarg f){
    x->x_sum = (f != 0.);
    canvas_update_dsp();
}

static t_int * sum_perform(t_int *w){
    int n = (int)(w[1]);
    t_sample *in = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int nchs = (int)(w[4]);
    for(int i = 0; i < n; i++){
        for(int j = 0; j < nchs-1; j++)
            out[i] += in[j*n + i];
    }
    return(w+5);
}

static void sum_dsp(t_sum *x, t_signal **sp){
    if(x->x_sum){
        signal_setmultiout(&sp[1], 1);
        dsp_add(sum_perform, 4, (t_int)sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, (t_int)sp[0]->s_nchans);
    }
    else{
        signal_setmultiout(&sp[1], sp[0]->s_nchans);
        dsp_add_copy(sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_nchans*sp[0]->s_n);
    }
}

void *sum_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_sum *x = (t_sum *)pd_new(sum_class);
    x->x_sum = 1;
    if(ac)
        x->x_sum = atom_getfloatarg(0, ac, av) != 0;
    outlet_new(&x->x_obj, &s_signal);
    return(void *)x;
}

void sum_tilde_setup(void){
    sum_class = class_new(gensym("sum~"), (t_newmethod)sum_new, 0,
        sizeof(t_sum), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(sum_class, nullfn, gensym("signal"), 0);
    class_addmethod(sum_class, (t_method)sum_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(sum_class, (t_method)sum_sum, gensym("sum"), A_FLOAT, 0);
}
