// Porres 2016

#include "m_pd.h"
#include "math.h"


static t_class *parabolic_class;

typedef struct _parabolic
{
    t_object x_obj;
    double  x_phase;
    double  x_last_phase_offset;
    t_float  x_freq;
    t_inlet  *x_inlet_phase;
    t_inlet  *x_inlet_sync;
    t_outlet *x_outlet_dsp_0;
    t_float x_sr;
} t_parabolic;

static t_int *parabolic_perform(t_int *w)
{
    t_parabolic *x = (t_parabolic *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]); // freq
    t_float *in2 = (t_float *)(w[4]); // phase
    t_float *in3 = (t_float *)(w[5]); // sync
    t_float *out = (t_float *)(w[6]);
    double phase = x->x_phase;
    double last_phase_offset = x->x_last_phase_offset;
    double sr = x->x_sr;
    while (nblock--)
    {
        double hz = *in1++;
        double phase_step = hz / sr; // phase_step
        phase_step = phase_step > 1 ? 1. : phase_step < -1 ? -1 : phase_step; // clipped phase_step
        double phase_offset = *in2++;
        double trig = *in3++;
        if (trig > 0 && trig <= 1)
            {
            phase = trig;
            if(phase == 1) phase = 0;
            }
        else
            {
            double phase_dev = phase_offset - last_phase_offset;
            if (phase_dev >= 1 || phase_dev <= -1)
                phase_dev = fmod(phase_dev, 1); // fmod(phase_dev)
            phase = phase + phase_dev;
            if(phase >= 1) phase = phase - 1;
            if(phase < 0) phase = phase + 1;
            }
        *out++ = (1 - pow(fmod(phase * 2, 1) * 2 - 1, 2)) * (phase <= 0.5 ? 1 : -1);
        phase = fmod(phase + phase_step, 1); // next phase (wrapped)
        last_phase_offset = phase_offset; // last phase offset
    }
    x->x_phase = phase;
    x->x_last_phase_offset = last_phase_offset;
    return (w + 7);
}

static void parabolic_dsp(t_parabolic *x, t_signal **sp)
{
    dsp_add(parabolic_perform, 6, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *parabolic_free(t_parabolic *x)
{
    inlet_free(x->x_inlet_phase);
    inlet_free(x->x_inlet_sync);
    outlet_free(x->x_outlet_dsp_0);
    return (void *)x;
}

static void *parabolic_new(t_floatarg f1, t_floatarg f2)
{
    t_parabolic *x = (t_parabolic *)pd_new(parabolic_class);
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
    return (x);
}

void parabolic_tilde_setup(void)
{
    parabolic_class = class_new(gensym("parabolic~"),
        (t_newmethod)parabolic_new, (t_method)parabolic_free,
        sizeof(t_parabolic), CLASS_DEFAULT, A_DEFFLOAT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(parabolic_class, t_parabolic, x_freq);
    class_addmethod(parabolic_class, (t_method)parabolic_dsp, gensym("dsp"), A_CANT, 0);
}
