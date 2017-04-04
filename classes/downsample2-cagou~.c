// Porres 2017

#include "m_pd.h"
#include "math.h"

static t_class *downsample2_class;

typedef struct _downsample2
{
    t_object x_obj;
    t_float  x_phase;
    t_float  x_yn;
    t_float  x_ynm1;
    t_float  x_interp;
    t_inlet  *x_inlet;
    float    x_sr;
} t_downsample2;

static t_int *downsample2_perform(t_int *w)
{
    t_downsample2 *x = (t_downsample2 *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    t_float phase = x->x_phase;
    t_float yn = x->x_yn;
    t_float ynm1 = x->x_ynm1;
    t_float sr = x->x_sr;
    t_float interp = x->x_interp;
    while (nblock--)
    {
        float input = *in1++;
        float hz = *in2++;
        double phase_step = hz / sr; // phase_step
        phase_step = phase_step > 1 ? 1. : phase_step < -1 ? -1 : phase_step; // clipped phase_step
        int trig = (phase >= 1. || phase <= 0.);
        if (hz >= 0)
            {
            if (trig) // update
                {
                phase = phase - 1;
                ynm1 = yn;
                yn = input;
                }
            // if (interp)
            //    *out++ = ynm1 + (yn - ynm1) * phase;
            // else
                *out++ = yn;
            }
        else
            {
            if (trig) // update
                {
                phase = phase + 1;
                ynm1 = yn;
                yn = input;
                }
            if (interp)
                *out++ = ynm1 + (yn - ynm1) * (1 - phase);
            else
                *out++ = yn;
            }
        phase += phase_step;
    }
    x->x_yn = yn;
    x->x_ynm1 = ynm1;
    x->x_phase = phase;
    return (w + 6);
}

static void downsample2_dsp(t_downsample2 *x, t_signal **sp)
{
    x->x_sr = sp[0]->s_sr;
    dsp_add(downsample2_perform, 5, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *downsample2_free(t_downsample2 *x)
{
    inlet_free(x->x_inlet);
    return (void *)x;
}

static void *downsample2_new(t_floatarg f1, t_floatarg f2)
{
    t_downsample2 *x = (t_downsample2 *)pd_new(downsample2_class);
    if (f1 >= 0) x->x_phase = 1;
    x->x_interp = (f2 != 0);
    x->x_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet, (f1));
    outlet_new((t_object *)x, &s_signal);
    return (x);
}


void downsample2_tilde_setup(void)
{
    downsample2_class = class_new(gensym("downsample2~"),
        (t_newmethod)downsample2_new,
        (t_method)downsample2_free,
        sizeof(t_downsample2),
        CLASS_DEFAULT,
        A_DEFFLOAT, A_DEFFLOAT,
        0);
        class_addmethod(downsample2_class, nullfn, gensym("signal"), 0);
        class_addmethod(downsample2_class, (t_method)downsample2_dsp, gensym("dsp"), A_CANT, 0);
}
