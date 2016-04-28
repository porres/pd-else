// Porres 2016

#include "m_pd.h"

static t_class *imp_class;

typedef struct _imp
{
    t_object x_obj;
    t_float  x_phase;
    t_float  x_freq;
    t_inlet  *x_inlet;
    t_float x_sr;
} t_imp;

static t_int *imp_perform(t_int *w)
{
    t_imp *x = (t_imp *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out1 = (t_float *)(w[5]);
    t_float phase = x->x_phase;
    t_float sr = x->x_sr;
    while (nblock--)
    {
        float hz = *in1++;
        float phase_step = hz / sr; // phase_step
        if (hz >= 0)
        {
        *out1++ = phase >= 1.;
        if (phase >= 1.) phase = phase - 1.; // wrapped phase
        }
        else
        {
            *out1++ = phase <= 0.;
            if (phase <= 0.) phase = phase + 1.; // wrapped phase
        }
//      *out2++ = phase; // wrapped phase
        phase = phase + phase_step; // next phase
    }
    x->x_phase = phase;
    return (w + 6);
}

static void imp_dsp(t_imp *x, t_signal **sp)
{
    dsp_add(imp_perform, 5, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *imp_free(t_imp *x)
{
    inlet_free(x->x_inlet);
    return (void *)x;
}

static void *imp_new(t_floatarg f)
{
    t_imp *x = (t_imp *)pd_new(imp_class);
    t_float freq = f;
    x->x_phase = f >= 0.; // initial phase
    x->x_sr = sys_getsr(); // sample rate
    x->x_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet, 0);
    outlet_new((t_object *)x, &s_signal);
    
    x->x_freq = freq;
    
    return (x);
}

void imp_tilde_setup(void)
{
    imp_class = class_new(gensym("imp~"),
        (t_newmethod)imp_new, (t_method)imp_free,
        sizeof(t_imp), CLASS_DEFAULT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(imp_class, t_imp, x_freq);
    class_addmethod(imp_class, (t_method)imp_dsp, gensym("dsp"), A_CANT, 0);
}