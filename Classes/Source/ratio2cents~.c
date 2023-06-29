// Porres 2016

#include "m_pd.h"
#include <math.h>

static t_class *ratio2cents_class;

typedef struct _ratio2cents {
    t_object x_obj;
}t_ratio2cents;

static t_int * ratio2cents_perform(t_int *w){
    int n = (int)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    while(n--){
        float f = *in++;
        if(f < 0.f) f = 0;
        *out++ = log2(f) * 1200;
    }
    return(w+4);
}

static void ratio2cents_dsp(t_ratio2cents *x, t_signal **sp){
    x = NULL;
    signal_setmultiout(&sp[1], sp[0]->s_nchans);
    dsp_add(ratio2cents_perform, 3, (t_int)(sp[0]->s_length * sp[0]->s_nchans),
        sp[0]->s_vec, sp[1]->s_vec);
}

void *ratio2cents_new(void){
    t_ratio2cents *x = (t_ratio2cents *)pd_new(ratio2cents_class);
    outlet_new(&x->x_obj, &s_signal);
    return(void *)x;
}

void ratio2cents_tilde_setup(void){
    ratio2cents_class = class_new(gensym("ratio2cents~"), (t_newmethod)ratio2cents_new,
        0, sizeof (t_ratio2cents), CLASS_MULTICHANNEL, 0);
    class_addmethod(ratio2cents_class, nullfn, gensym("signal"), 0);
    class_addmethod(ratio2cents_class, (t_method) ratio2cents_dsp, gensym("dsp"), A_CANT, 0);
}
