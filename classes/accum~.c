// Porres 2016

#include "m_pd.h"

static t_class *accum_class;

typedef struct _accum{
    t_object  x_obj;
    t_float   x_sum;
    t_float   x_in;
    t_float   x_start;
    t_inlet  *x_triglet;
    t_outlet *x_outlet;
} t_accum;

static void accum_bang(t_accum *x){
    x->x_sum = x->x_start;
}

static void accum_set(t_accum *x, t_floatarg f){
    x->x_start = f;
}

static t_int *accum_perform(t_int *w){
    t_accum *x = (t_accum *)(w[1]);
    int n = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    t_float sum = x->x_sum;
    t_float start = x->x_start;
    while(n--){
        t_float in = *in1++;
        t_float trig = *in2++;
        if(trig == 1)
            *out++ = sum = (start += in);
        else
            *out++ = (sum += in);
    }
    x->x_sum = sum; // next
    return (w + 6);
}

static void accum_dsp(t_accum *x, t_signal **sp){
    dsp_add(accum_perform, 5, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *accum_free(t_accum *x){
    inlet_free(x->x_triglet);
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *accum_new(t_floatarg f){
    t_accum *x = (t_accum *)pd_new(accum_class);
    x->x_sum = x->x_start = f;
    x->x_triglet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return (x);
}

void accum_tilde_setup(void){
    accum_class = class_new(gensym("accum~"),
        (t_newmethod)accum_new, (t_method)accum_free,
        sizeof(t_accum), CLASS_DEFAULT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(accum_class, t_accum, x_in);
    class_addmethod(accum_class, (t_method) accum_dsp, gensym("dsp"), 0);
    class_addmethod(accum_class, (t_method)accum_set, gensym("set"), A_FLOAT, 0);
    class_addbang(accum_class,(t_method)accum_bang);
}
