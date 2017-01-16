// Porres 2016

#include "m_pd.h"
#include "math.h"


static t_class *pimp_class;

typedef struct _pimp
{
    t_object x_obj;
    t_float  x_phase;
    t_float  x_last_phase_offset;
    t_float  x_freq;
    t_inlet  *x_inlet_phase;
    t_inlet  *x_inlet_sync;
    t_outlet *x_outlet_dsp_0;
    t_outlet *x_outlet_dsp_1;
    t_float x_sr;
} t_pimp;

static t_int *pimp_perform(t_int *w)
{
    t_pimp *x = (t_pimp *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]); // freq
    t_float *in2 = (t_float *)(w[4]); // phase
    t_float *in3 = (t_float *)(w[5]); // sync
    t_float *out1 = (t_float *)(w[6]);
    t_float *out2 = (t_float *)(w[7]);
    t_float phase = x->x_phase;
    t_float last_phase_offset = x->x_last_phase_offset;
    t_float sr = x->x_sr;
    while (nblock--)
    {
        float hz = *in1++;
        float phase_offset = *in2++;
        float trig = *in3++;
        float phase_step = hz / sr; // phase_step
        phase_step = phase_step > 1 ? 1. : phase_step < -1 ? -1 : phase_step; // clipped phase_step
        float phase_dev = phase_offset - last_phase_offset;
        if (hz >= 0)
            {
                if (trig > 0 && trig <= 1) phase = trig;
                else
                    {
                        if (phase_dev > 1 || phase_dev < -1)
                            phase_dev = fmod(phase_dev, 1);
                        phase = phase + phase_dev;
                        if (phase <= 0) phase = phase + 1.; // wrap phase
                    }
                *out2++ = phase >= 1.;
                if (phase >= 1.) phase = phase - 1; // another wrap phase
            }
        else
            {
                if (trig > 0 && trig < 1) phase = trig;
                else if (trig == 1) phase = 0;
                *out2++ = phase <= 0.;
                if (phase <= 0.) phase = phase + 1.; // wrapped phase
            }
        *out1++ = phase == 1 ? 0 : phase; // wrapped phase
        phase = phase + phase_step; // next phase
        last_phase_offset = phase_offset; // last phase offset
    }
    x->x_phase = phase;
    x->x_last_phase_offset = last_phase_offset;
    return (w + 8);
}

static void pimp_dsp(t_pimp *x, t_signal **sp)
{
    dsp_add(pimp_perform, 7, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
}

static void *pimp_free(t_pimp *x)
{
    inlet_free(x->x_inlet_phase);
    inlet_free(x->x_inlet_sync);
    outlet_free(x->x_outlet_dsp_0);
    outlet_free(x->x_outlet_dsp_1);
    return (void *)x;
}

static void *pimp_new(t_floatarg f1, t_floatarg f2)
{
    t_pimp *x = (t_pimp *)pd_new(pimp_class);
    t_float init_freq = f1;
    t_float init_phase = f2;
    init_phase < 0 ? 0 : init_phase >= 1 ? 0 : init_phase; // clipping phase input
    if (init_phase == 0 && init_freq > 0)
        x->x_phase = 1.;
    x->x_last_phase_offset = 0;
    x->x_freq = init_freq;
    x->x_sr = sys_getsr(); // sample rate
    x->x_inlet_phase = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_phase, init_phase);
    x->x_inlet_sync = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_sync, 0);
    x->x_outlet_dsp_0 = outlet_new(&x->x_obj, &s_signal);
    x->x_outlet_dsp_1 = outlet_new(&x->x_obj, &s_signal);
    return (x);
}

void pimp_tilde_setup(void)
{
    pimp_class = class_new(gensym("pimp~"),
        (t_newmethod)pimp_new, (t_method)pimp_free,
        sizeof(t_pimp), CLASS_DEFAULT, A_DEFFLOAT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(pimp_class, t_pimp, x_freq);
    class_addmethod(pimp_class, (t_method)pimp_dsp, gensym("dsp"), A_CANT, 0);
}
