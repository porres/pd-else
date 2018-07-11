// Porres 2016

#include "m_pd.h"
#include <math.h>

static t_class *step_class;

typedef struct _step{
    t_object  x_obj;
    t_float   x_sum;
    t_float   x_start;
    t_float   x_max;
    t_inlet  *x_triglet;
    t_outlet *x_outlet;
} t_step;

static void step_bang(t_step *x){
    x->x_sum ++;
}

static void step_set(t_step *x, t_floatarg f){
    x->x_start = f;
}

static t_int *step_perform(t_int *w){
    t_step *x = (t_step *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    t_float sum = x->x_sum;
    t_float start = x->x_start;
    t_float max = x->x_max;
    while (nblock--){
        t_float in = *in1++;
        t_float trig = *in2++;
        if (trig == 1)
            sum = 0;
        else if(in == 1){
            sum = fmod((sum + in), max);
        }
        *out++ = sum + 1;
    }
    x->x_sum = sum; // next
    return (w + 6);
}

static void step_dsp(t_step *x, t_signal **sp){
    dsp_add(step_perform, 5, x, sp[0]->s_n, sp[0]->s_vec,
            sp[1]->s_vec, sp[2]->s_vec);
}

static void *step_free(t_step *x){
    inlet_free(x->x_triglet);
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *step_new(t_floatarg f){
    t_step *x = (t_step *)pd_new(step_class);
    x->x_max = f;
    x->x_triglet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return (x);
}

void step_tilde_setup(void){
    step_class = class_new(gensym("step~"),
        (t_newmethod)step_new, (t_method)step_free,
        sizeof(t_step), CLASS_DEFAULT, A_DEFFLOAT, 0);
    class_addmethod(step_class, nullfn, gensym("signal"), 0);
    class_addmethod(step_class, (t_method) step_dsp, gensym("dsp"),A_CANT,  0);
    class_addmethod(step_class, (t_method)step_set, gensym("set"), A_FLOAT, 0);
    class_addbang(step_class,(t_method)step_bang);
}
