// Porres 2017

#include "m_pd.h"

static t_class *lfnoise_class;

typedef struct _lfnoise
{
    t_object  x_obj;
    int x_val;
    t_float  x_lastout;
    t_float  x_sr;
    double  x_phase;
    t_float  x_freq;
    t_outlet *x_outlet;
} t_lfnoise;


static t_int *lfnoise_perform(t_int *w)
{
    t_lfnoise *x = (t_lfnoise *)(w[1]);
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
        phase_step = phase_step > 1 ? 1. : phase_step < -1 ? -1 : phase_step; // clipped phase_step
        int trig;
        if (hz >= 0)
            {
            trig = phase >= 1.;
            if (phase >= 1.) phase = phase - 1;
            }
        else
            {
            trig = (phase <= 0.);
            if (phase <= 0.) phase = phase + 1.;
            }
        t_float noise = ((float)((val & 0x7fffffff) - 0x40000000)) * (float)(1.0 / 0x40000000);

        if (trig) lastout = noise;
        *out++ = lastout;
    
        phase += phase_step;
        val = val * 435898247 + 382842987;
    }
     *vp = val;
    x->x_phase = phase;
    x->x_lastout = lastout;
    return (w + 6);
}

static void lfnoise_dsp(t_lfnoise *x, t_signal **sp)
{
    x->x_sr = sp[0]->s_sr;
    dsp_add(lfnoise_perform, 5, x, sp[0]->s_n, &x->x_val, sp[0]->s_vec, sp[1]->s_vec);
}

static void *lfnoise_free(t_lfnoise *x)
{
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *lfnoise_new(t_floatarg f)
{
    t_lfnoise *x = (t_lfnoise *)pd_new(lfnoise_class);
    if(f >= 0) x->x_phase = 1;
    x->x_freq  = f;
    x->x_sr = sys_getsr();
    x->x_lastout = 0;
    static int init = 307;
    x->x_val = (init *= 1319);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return (x);
}


void lfnoise_tilde_setup(void)
{
    lfnoise_class = class_new(gensym("lfnoise~"),
        (t_newmethod)lfnoise_new, (t_method)lfnoise_free,
        sizeof(t_lfnoise), 0, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(lfnoise_class, t_lfnoise, x_freq);
    class_addmethod(lfnoise_class, (t_method)lfnoise_dsp, gensym("dsp"), A_CANT, 0);
}
