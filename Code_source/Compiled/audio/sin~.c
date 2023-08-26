// porres

#include <math.h>
#include "m_pd.h"

#define TWO_PI (3.14159265358979323846 * 2)

typedef struct _sin{
    t_object    x_obj;
}t_sin;

static t_class *sin_class;

static t_int *sin_perform(t_int *w){
    int n = (int)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    while(n--){
        double f = *in++;
        if(f >= 1 || f <= -1) f = fmod(f, 1);
        if(f == 0.5)
            *out++ = 0;
        else
            *out++ = sin(f * TWO_PI);
    }
    return(w+4);
}

static void sin_dsp(t_sin *x, t_signal **sp){
    x = NULL;
    signal_setmultiout(&sp[1], sp[0]->s_nchans);
    dsp_add(sin_perform, 3, (t_int)(sp[0]->s_length * sp[0]->s_nchans),
        sp[0]->s_vec, sp[1]->s_vec);
}

void *sin_new(void){
    t_sin *x = (t_sin *)pd_new(sin_class);
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void sin_tilde_setup(void){
    sin_class = class_new(gensym("sin~"), (t_newmethod)sin_new,
        0, sizeof(t_sin), CLASS_MULTICHANNEL, 0);
    class_addmethod(sin_class, nullfn, gensym("signal"), 0);
    class_addmethod(sin_class, (t_method) sin_dsp, gensym("dsp"), A_CANT,  0);
}
