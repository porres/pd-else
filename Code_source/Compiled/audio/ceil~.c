// Porres 2017-2023

#include "m_pd.h"
#include <math.h>

static t_class *ceil_class;

typedef struct _ceil{
    t_object  x_obj;
}t_ceil;

static t_int * ceil_perform(t_int *w){
    int n = (int)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    while(n--)
        *out++ = ceil(*in++);
    return(w+4);
}

static void ceil_dsp(t_ceil *x, t_signal **sp){
    x = NULL;
    signal_setmultiout(&sp[1], sp[0]->s_nchans);
    dsp_add(ceil_perform, 3, (t_int)(sp[0]->s_length * sp[0]->s_nchans),
        sp[0]->s_vec, sp[1]->s_vec);
}

void *ceil_new(void){
    t_ceil *x = (t_ceil *)pd_new(ceil_class);
    outlet_new(&x->x_obj, &s_signal);
    return(void *)x;
}

void ceil_tilde_setup(void){
    ceil_class = class_new(gensym("ceil~"),
        (t_newmethod) ceil_new, 0, sizeof (t_ceil), CLASS_MULTICHANNEL, 0);
    class_addmethod(ceil_class, nullfn, gensym("signal"), 0);
    class_addmethod(ceil_class, (t_method) ceil_dsp, gensym("dsp"), A_CANT, 0);
}
