// porres 2017

#include "m_pd.h"
#include "math.h"

typedef struct _power{
    t_object x_obj;
    t_float  x_in;
    t_inlet  *x_f2;
}t_power;

static t_class *power_class;

static t_int *power_perform(t_int *w){
    t_power *x = (t_power *)(w[1]);
    int nblock = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    while (nblock--){
        t_float f1 = *in1++;
        t_float f2 = *in2++;
        *out++ = powf(f1, f2);
    };
    return (w + 6);
}

static void power_dsp(t_power *x, t_signal **sp){
    dsp_add(power_perform, 5, x, sp[0]->s_n, sp[0]->s_vec,
            sp[1]->s_vec, sp[2]->s_vec);
}

static void *power_new(t_floatarg f2){
    t_power *x = (t_power *)pd_new(power_class);
    x->x_f2 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_f2, f2);
    outlet_new((t_object *)x, &s_signal);
    return (x);
}

void power_tilde_setup(void){
    power_class = class_new(gensym("power~"), (t_newmethod)power_new, 0,
                            sizeof(t_power), 0, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(power_class, t_power, x_in);
    class_addmethod(power_class, (t_method) power_dsp, gensym("dsp"), A_CANT, 0);
}
