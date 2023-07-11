// Porres 2017-2023

#include "m_pd.h"
#include <math.h>

static t_class *rint_class;

typedef struct _rint{
    t_object  x_obj;
}t_rint;

static t_int * rint_perform(t_int *w){
    int n = (int)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    while(n--)
        *out++ = rint(*in++);
    return(w+4);
}

static void rint_dsp(t_rint *x, t_signal **sp){
    x = NULL;
    signal_setmultiout(&sp[1], sp[0]->s_nchans);
    dsp_add(rint_perform, 3, (t_int)(sp[0]->s_length * sp[0]->s_nchans),
        sp[0]->s_vec, sp[1]->s_vec);
}

void *rint_new(void){
    t_rint *x = (t_rint *)pd_new(rint_class);
    outlet_new(&x->x_obj, &s_signal);
    return(void *)x;
}

void rint_tilde_setup(void){
    rint_class = class_new(gensym("rint~"),
        (t_newmethod) rint_new, 0, sizeof (t_rint), CLASS_MULTICHANNEL, 0);
    class_addmethod(rint_class, nullfn, gensym("signal"), 0);
    class_addmethod(rint_class, (t_method) rint_dsp, gensym("dsp"), A_CANT, 0);
}
