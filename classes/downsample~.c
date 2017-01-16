// Porres 2017

#include "m_pd.h"
#include "math.h"

static t_class *downsample_class;

typedef struct _downsample
{
    t_object x_obj;
    t_float  x_phase;
    t_float  x_lastout;
    t_inlet  *x_inlet;
    float    x_sr;
} t_downsample;

static t_int *downsample_perform(t_int *w)
{
    t_downsample *x = (t_downsample *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    t_float phase = x->x_phase;
    t_float lastout = x->x_lastout;
    t_float sr = x->x_sr;
while (nblock--)
{
    float input = *in1++;
    float phase_step;
    float freq = *in2++;
   freq = freq > 0. ? freq : 0.;
    phase_step = freq / sr;
    phase_step = phase_step > 1. ? 1. : phase_step;
        if (phase >= 1.)
        lastout = input;
        *out++ = lastout;
    phase = fmod(phase, 1.) + phase_step;
}
x->x_phase = phase;
x->x_lastout = lastout;
return (w + 6);
}

static void downsample_dsp(t_downsample *x, t_signal **sp)
{
    x->x_sr = sp[0]->s_sr;
    dsp_add(downsample_perform, 5, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *downsample_free(t_downsample *x)
{
    inlet_free(x->x_inlet);
    return (void *)x;
}

static void *downsample_new(t_floatarg f)
{
    t_downsample *x = (t_downsample *)pd_new(downsample_class);
    x->x_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet, (f > 0. ? f : 0.));
    outlet_new((t_object *)x, &s_signal);
    x->x_lastout = 0;
    x->x_phase = 0;
    return (x);
}


void downsample_tilde_setup(void)
{
    downsample_class = class_new(gensym("downsample~"),
        (t_newmethod)downsample_new,
        (t_method)downsample_free,
        sizeof(t_downsample),
        CLASS_DEFAULT,
        A_DEFFLOAT,
        0);
        class_addmethod(downsample_class, nullfn, gensym("signal"), 0);
        class_addmethod(downsample_class, (t_method)downsample_dsp, gensym("dsp"), A_CANT, 0);
}
