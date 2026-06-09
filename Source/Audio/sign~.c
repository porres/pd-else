// Porres 2026

#include <m_pd.h>

static t_class *sign_class;

typedef struct _sign{
    t_object  x_obj;
}t_sign;

static t_int * sign_perform(t_int *w){
    int n = (int)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    while(n--){
        float input = (*in++);
        *out++ = (input > 0) - (input < 0);
    }
    return(w+4);
}

static void sign_dsp(t_sign *x, t_signal **sp){
    x = NULL;
    signal_setmultiout(&sp[1], sp[0]->s_nchans);
    dsp_add(sign_perform, 3, (t_int)(sp[0]->s_length * sp[0]->s_nchans),
        sp[0]->s_vec, sp[1]->s_vec);
}

void *sign_new(void){
    t_sign *x = (t_sign *)pd_new(sign_class);
    outlet_new(&x->x_obj, &s_signal);
    return(void *)x;
}

void sign_tilde_setup(void){
    sign_class = class_new(gensym("sign~"),
        (t_newmethod) sign_new, 0, sizeof (t_sign), CLASS_MULTICHANNEL, 0);
    class_addmethod(sign_class, nullfn, gensym("signal"), 0);
    class_addmethod(sign_class, (t_method) sign_dsp, gensym("dsp"), A_CANT, 0);
}
