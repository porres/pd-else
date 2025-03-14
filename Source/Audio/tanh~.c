// Porres 2025

#include <m_pd.h>
#include <math.h>

static t_class *tanh_class;

typedef struct _tanh{
    t_object  x_obj;
}t_tanh;

static t_int * tanh_perform(t_int *w){
    int n = (int)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    while(n--)
        *out++ = tanhf(*in++);
    return(w+4);
}

static void tanh_dsp(t_tanh *x, t_signal **sp){
    x = NULL;
    signal_setmultiout(&sp[1], sp[0]->s_nchans);
    dsp_add(tanh_perform, 3, (t_int)(sp[0]->s_length * sp[0]->s_nchans),
        sp[0]->s_vec, sp[1]->s_vec);
}

void *tanh_new(void){
    t_tanh *x = (t_tanh *)pd_new(tanh_class);
    outlet_new(&x->x_obj, &s_signal);
    return(void *)x;
}

void tanh_tilde_setup(void){
    tanh_class = class_new(gensym("tanh~"),
        (t_newmethod) tanh_new, 0, sizeof (t_tanh), CLASS_MULTICHANNEL, 0);
    class_addmethod(tanh_class, nullfn, gensym("signal"), 0);
    class_addmethod(tanh_class, (t_method) tanh_dsp, gensym("dsp"), A_CANT, 0);
}
