// Porres 2016

#include "m_pd.h"

static t_class *inc_class;

typedef struct _inc{
    t_object  x_obj;
    t_float   x_sum;
    t_float   x_in;
    t_float   x_start;
    t_inlet  *x_triglet;
    t_outlet *x_outlet;
} t_inc;

static void inc_bang(t_inc *x){
    x->x_sum = x->x_start;
}

static void inc_set(t_inc *x, t_floatarg f){
    x->x_start = f;
}

static t_int *inc_perform(t_int *w){
    t_inc *x = (t_inc *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    t_float sum = x->x_sum;
    t_float start = x->x_start;
    while (nblock--){
        t_float in = *in1++;
        t_float trig = *in2++;
        if (trig == 1)
            *out++ = sum = start;
        else
            *out++ = sum;
        sum += in;
    }
    x->x_sum = sum; // next
    return (w + 6);
}

static void inc_dsp(t_inc *x, t_signal **sp){
    dsp_add(inc_perform, 5, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *inc_free(t_inc *x){
    inlet_free(x->x_triglet);
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *inc_new(t_floatarg f){
    t_inc *x = (t_inc *)pd_new(inc_class);
    x->x_sum = x->x_start = f;
    x->x_triglet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return (x);
}

void inc_tilde_setup(void){
    inc_class = class_new(gensym("inc~"),
        (t_newmethod)inc_new, (t_method)inc_free,
        sizeof(t_inc), CLASS_DEFAULT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(inc_class, t_inc, x_in);
    class_addmethod(inc_class, (t_method) inc_dsp, gensym("dsp"), 0);
    class_addmethod(inc_class, (t_method)inc_set, gensym("set"), A_FLOAT, 0);
    class_addbang(inc_class,(t_method)inc_bang);
}
