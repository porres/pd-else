// Porres 2017-2023

#include "m_pd.h"
#include <math.h>

static t_class *floor_class;

typedef struct _floor{
    t_object  x_obj;
}t_floor;

static t_int * floor_perform(t_int *w){
    int n = (int)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    while(n--)
        *out++ = floor(*in++);
    return(w+4);
}

static void floor_dsp(t_floor *x, t_signal **sp){
    x = NULL;
    signal_setmultiout(&sp[1], sp[0]->s_nchans);
    dsp_add(floor_perform, 3, (t_int)(sp[0]->s_length * sp[0]->s_nchans),
        sp[0]->s_vec, sp[1]->s_vec);
}

void *floor_new(void){
    t_floor *x = (t_floor *)pd_new(floor_class);
    outlet_new(&x->x_obj, &s_signal);
    return(void *)x;
}

void floor_tilde_setup(void){
    floor_class = class_new(gensym("floor~"),
        (t_newmethod) floor_new, 0, sizeof (t_floor), CLASS_MULTICHANNEL, 0);
    class_addmethod(floor_class, nullfn, gensym("signal"), 0);
    class_addmethod(floor_class, (t_method) floor_dsp, gensym("dsp"), A_CANT, 0);
}
