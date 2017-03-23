// Porres 2017

#include "m_pd.h"

static t_class *noisy_class;

typedef struct _noisy
{
    t_object  x_obj;
    int x_val;
    t_float  x_lastout;
    t_float  x_sr;
    t_float  x_phase;
    t_float  x_freq;
    t_outlet *x_outlet;
} t_noisy;


static t_int *noisy_perform(t_int *w)
{
    t_noisy *x = (t_noisy *)(w[1]);
    int nblock = (t_int)(w[2]);
    int *vp = (int *)(w[3]);
    t_float *in = (t_float *)(w[4]);
    t_sample *out = (t_sample *)(w[5]);
    int val = *vp;
    t_float lastout = x->x_lastout;
    double phase = x->x_phase;
    double sr = x->x_sr;
    while (nblock--)
    {
        t_float hz = *in++;
        double phase_step = hz / sr; // phase_step
        phase_step = phase_step > 1 ? 1. : phase_step < 0 ? 0 : phase_step; // clipped phase_step
        
        t_float noise = ((float)((val & 0x7fffffff) - 0x40000000)) * (float)(1.0 / 0x40000000);
        *out++ = noise;
        val = val * 435898247 + 382842987;
    }
     *vp = val;
    x->x_phase = phase;
    x->x_lastout = lastout;
    return (w + 6);
}

static void noisy_dsp(t_noisy *x, t_signal **sp)
{
    dsp_add(noisy_perform, 5, x, sp[0]->s_n, &x->x_val, sp[0]->s_vec, sp[1]->s_vec);
}

static void *noisy_free(t_noisy *x)
{
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *noisy_new(t_floatarg f)
{
    t_noisy *x = (t_noisy *)pd_new(noisy_class);
    x->x_freq = f;
    x->x_sr = sys_getsr(); // sample rate
    static int init = 307;
    x->x_val = (init *= 1319);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return (x);
}


void noisy_tilde_setup(void)
{
    noisy_class = class_new(gensym("noisy~"),
        (t_newmethod)noisy_new, (t_method)noisy_free,
        sizeof(t_noisy), 0, 0);
    class_addmethod(noisy_class, nullfn, gensym("signal"), 0);
    class_addmethod(noisy_class, (t_method) noisy_dsp, gensym("dsp"), A_CANT, 0);
}
