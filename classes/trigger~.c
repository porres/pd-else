// Porres 2016

#include "m_pd.h"

static t_class *trigger_class;

typedef struct _trigger{
    t_object x_obj;
    t_inlet  *x_thresh_inlet;
    t_float  x_in;
    t_float  x_lastin;
} t_trigger;

static t_int *trigger_perform(t_int *w){
    t_trigger *x = (t_trigger *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    t_float lastin = x->x_lastin;
    while (nblock--){
        float input = *in1++;
        float threshold = *in2++;
        float output;
        if(input > threshold && lastin <= threshold)
            output = 1;
        else
            output = 0;
        *out++ = output;
        lastin = input;
    }
    x->x_lastin = lastin;
    return (w + 6);
}

static void trigger_dsp(t_trigger *x, t_signal **sp){
    dsp_add(trigger_perform, 5, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *trigger_free(t_trigger *x){
    inlet_free(x->x_thresh_inlet);
    return (void *)x;
}

static void *trigger_new(t_floatarg f){
    t_trigger *x = (t_trigger *)pd_new(trigger_class);
    x->x_lastin = 0;
    x->x_thresh_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_thresh_inlet, f);
    outlet_new((t_object *)x, &s_signal);
    return (x);
}

void trigger_tilde_setup(void){
    trigger_class = class_new(gensym("trigger~"), (t_newmethod)trigger_new, (t_method)trigger_free,
        sizeof(t_trigger), CLASS_DEFAULT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(trigger_class, t_trigger, x_in);
    class_addmethod(trigger_class, (t_method)trigger_dsp, gensym("dsp"), A_CANT, 0);
}

