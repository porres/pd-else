// Porres 2016

#include "m_pd.h"
#include <math.h>

static t_class *cents2ratio_class;

typedef struct _cents2ratio {
    t_object x_obj;
}t_cents2ratio;

static t_int * cents2ratio_perform(t_int *w){
    int n = (int)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    while(n--){
        float f = *in++;
        *out++ = pow(2, (f/1200));
    }
    return(w+4);
}

static void cents2ratio_dsp(t_cents2ratio *x, t_signal **sp){
    x = NULL;
    signal_setmultiout(&sp[1], sp[0]->s_nchans);
    dsp_add(cents2ratio_perform, 3, (t_int)(sp[0]->s_length * sp[0]->s_nchans),
        sp[0]->s_vec, sp[1]->s_vec);
}

void *cents2ratio_new(void){
    t_cents2ratio *x = (t_cents2ratio *)pd_new(cents2ratio_class);
    outlet_new(&x->x_obj, &s_signal);
    return(void *)x;
}

void cents2ratio_tilde_setup(void){
    cents2ratio_class = class_new(gensym("cents2ratio~"), (t_newmethod) cents2ratio_new,
        0, sizeof (t_cents2ratio), CLASS_MULTICHANNEL, 0);
    class_addmethod(cents2ratio_class, nullfn, gensym("signal"), 0);
    class_addmethod(cents2ratio_class, (t_method) cents2ratio_dsp, gensym("dsp"), A_CANT, 0);
}
