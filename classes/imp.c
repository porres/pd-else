// Porres 2016

#include "m_pd.h"

static t_class *imp_class;

typedef struct _imp
{
    t_object x_obj;
    t_float  x_phase;
    t_float  x_lastin;
    t_inlet  *x_inlet;
    t_float  x_lastout;
} t_imp;

static t_int *imp_perform(t_int *w)
{
    t_imp *x = (t_imp *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    t_float phase = x->x_phase;
    t_float lastin = x->x_lastin;
    t_float lastout = x->x_lastout;
while (nblock--)
{
    float next_phase;
    float input = *in1++;
    float samples = *in2++;
    float phase_step = 1. / samples; // phase_step
    {
        next_phase = fmod(phase, 1.) + phase_step;
        if (phase >= 1.)
        {
            *out++ = 1.;
        }
        else *out++ = 0.;
    }
    phase = next_phase;
}
x->x_phase = phase;
return (w + 6);
}


static void imp_dsp(t_imp *x, t_signal **sp)
{
    dsp_add(imp_perform, 5, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void imp_float(t_imp *x, t_float f)
{
    x->x_phase = f;
}


static void *imp_free(t_imp *x)
{
    inlet_free(x->x_inlet);
    return (void *)x;
}

static void *imp_new(t_floatarg f)
{
    t_imp *x = (t_imp *)pd_new(imp_class);
    x->x_phase = f;
    x->x_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet, f);
    outlet_new((t_object *)x, &s_signal);
    x->x_lastin = 0;
    x->x_lastout = 0;
    return (x);
}



void imp_tilde_setup(void)
{
    imp_class = class_new(gensym("imp~"),
        (t_newmethod)imp_new,
        (t_method)imp_free,
        sizeof(t_imp),
        CLASS_DEFAULT,
        A_DEFFLOAT,
        0);
        class_addmethod(imp_class, nullfn, gensym("signal"), 0);
        class_addmethod(imp_class, (t_method)imp_dsp, gensym("dsp"), A_CANT, 0);
        class_addfloat(imp_class, (t_method)imp_float);
}

