// a bad fix, but that I actually like it and might turn into another external

#include "m_pd.h"
#include "math.h"
#include "shared.h"
#include "sickle/sic.h"

typedef struct _downsamp
{
    t_sic      x_sic;
    t_float    x_samples;
    t_float    x_phase;
} t_downsamp;

static t_class *downsamp_class;

static t_int *downsamp_perform(t_int *w)
{
    t_downsamp *x = (t_downsamp *)(w[1]);
    int nblock = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    float phase = x->x_phase;
    while (nblock--)
    {
        float next_phase;
//		float input = *in1++;
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

static void downsamp_dsp(t_downsamp *x, t_signal **sp)
{
    dsp_add(downsamp_perform, 5, x, sp[0]->s_n,
	    sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}


static void *downsamp_new(t_floatarg f)
{
    t_downsamp *x = (t_downsamp *)pd_new(downsamp_class);
    x->x_samples = f;
    x->x_phase = 0;
    inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    return (x);
}


void downsamp_tilde_setup(void)
{
    downsamp_class = class_new(gensym("downsamp~"),
        (t_newmethod)downsamp_new, 0, sizeof(t_downsamp),
        0, A_DEFFLOAT, 0);
    sic_setup(downsamp_class, downsamp_dsp, SIC_FLOATTOSIGNAL);
}