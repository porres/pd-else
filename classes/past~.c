// Porres 2017

#include "m_pd.h"

static t_class *past_class;

typedef struct _past{
    t_object x_obj;
    t_inlet  *x_thresh_inlet;
    t_float  x_in;
    t_float  x_lastin;
} t_past;

static t_int *past_perform(t_int *w){
    t_past *x = (t_past *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out1 = (t_float *)(w[5]);
    t_float *out2 = (t_float *)(w[6]);
    t_float lastin = x->x_lastin;
    while (nblock--){
        float input = *in1++;
        float threshold = *in2++;
        *out1++ = input > threshold && lastin <= threshold;
        *out2++ = input <= threshold && lastin > threshold;
        lastin = input;
    }
    x->x_lastin = lastin;
    return (w + 7);
}

static void past_dsp(t_past *x, t_signal **sp){
    dsp_add(past_perform, 6, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *past_free(t_past *x){
    inlet_free(x->x_thresh_inlet);
    return (void *)x;
}

static void *past_new(t_floatarg f){
    t_past *x = (t_past *)pd_new(past_class);
    x->x_lastin = 0;
    x->x_thresh_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_thresh_inlet, f);
    outlet_new((t_object *)x, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    return (x);
}

void past_tilde_setup(void){
    past_class = class_new(gensym("past~"), (t_newmethod)past_new, (t_method)past_free,
        sizeof(t_past), CLASS_DEFAULT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(past_class, t_past, x_in);
    class_addmethod(past_class, (t_method)past_dsp, gensym("dsp"), A_CANT, 0);
}

