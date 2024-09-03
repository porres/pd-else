// Porres 2017-2023

#include "m_pd.h"
#include <math.h>

static t_class *trunc_class;

typedef struct _trunc{
    t_object  x_obj;
}t_trunc;

static t_int * trunc_perform(t_int *w){
    int n = (int)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    while(n--)
        *out++ = trunc(*in++);
    return(w+4);
}

static void trunc_dsp(t_trunc *x, t_signal **sp){
    x = NULL;
    signal_setmultiout(&sp[1], sp[0]->s_nchans);
    dsp_add(trunc_perform, 3, (t_int)(sp[0]->s_length * sp[0]->s_nchans),
        sp[0]->s_vec, sp[1]->s_vec);
}

void *trunc_new(void){
    t_trunc *x = (t_trunc *)pd_new(trunc_class);
    outlet_new(&x->x_obj, &s_signal);
    return(void *)x;
}

void trunc_tilde_setup(void){
    trunc_class = class_new(gensym("trunc~"),
        (t_newmethod) trunc_new, 0, sizeof (t_trunc), CLASS_MULTICHANNEL, 0);
    class_addmethod(trunc_class, nullfn, gensym("signal"), 0);
    class_addmethod(trunc_class, (t_method) trunc_dsp, gensym("dsp"), A_CANT, 0);
}
