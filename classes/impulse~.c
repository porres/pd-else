// Porres 2016

#include "m_pd.h"

static t_class *impulse_class;

typedef struct _impulse
{
    t_object x_obj;
    t_float  x_phase;
    t_float  x_freq;
    t_inlet  *x_inlet_phase;
    t_inlet  *x_inlet_sync;
    t_outlet *x_outlet_dsp_0;
    t_outlet *x_outlet_dsp_1;
    t_float x_sr;
} t_impulse;

static t_int *impulse_perform(t_int *w)
{
    t_impulse *x = (t_impulse *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]); // freq
    t_float *in2 = (t_float *)(w[3]); // phase
    t_float *in3 = (t_float *)(w[5]); // sync
    t_float *out1 = (t_float *)(w[6]);
    t_float *out2 = (t_float *)(w[7]);
    t_float phase = x->x_phase;
    t_float sr = x->x_sr;
    while (nblock--)
    {
        float hz = *in1++;
        float phase_step = hz / sr; // phase_step
        float trig = *in3++;
        if (hz >= 0)
        {
        if (trig > 0 && trig <= 1) phase = trig;
        *out1++ = phase >= 1.;
        if (phase >= 1.) phase = phase - 1.; // wrapped phase
        }
        else
        {  if (trig > 0 && trig < 1) phase = trig;
           else if (trig == 1) phase = 0;
            *out1++ = phase <= 0.;
            if (phase <= 0.) phase = phase + 1.; // wrapped phase
        }
        *out2++ = phase == 1? 0 : phase; // wrapped phase
        phase = phase + phase_step; // next phase
    }
    x->x_phase = phase;
    return (w + 8);
}

static void impulse_dsp(t_impulse *x, t_signal **sp)
{
    dsp_add(impulse_perform, 7, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
}

static void *impulse_free(t_impulse *x)
{
    inlet_free(x->x_inlet_phase);
    inlet_free(x->x_inlet_sync);
    outlet_free(x->x_outlet_dsp_0);
    outlet_free(x->x_outlet_dsp_1);
    return (void *)x;
}

static void *impulse_new(t_floatarg f)
{
    t_impulse *x = (t_impulse *)pd_new(impulse_class);
    t_float freq = f;
    x->x_phase = f >= 0.; // initial phase
    x->x_sr = sys_getsr(); // sample rate
    x->x_inlet_phase = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_phase, 0);
    x->x_inlet_sync = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_sync, 0);
    x->x_outlet_dsp_0 = outlet_new(&x->x_obj, &s_signal);
    x->x_outlet_dsp_1 = outlet_new(&x->x_obj, &s_signal);
    x->x_freq = freq;
    return (x);
}

void impulse_tilde_setup(void)
{
    impulse_class = class_new(gensym("impulse~"),
        (t_newmethod)impulse_new, (t_method)impulse_free,
        sizeof(t_impulse), CLASS_DEFAULT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(impulse_class, t_impulse, x_freq);
    class_addmethod(impulse_class, (t_method)impulse_dsp, gensym("dsp"), A_CANT, 0);
}