// Porres 2017

#include "m_pd.h"
#include "math.h"

static t_class *triwave_class;

typedef struct _triwave
{
    t_object x_obj;
    double  x_phase;
    double  x_last_phase_offset;
    t_float  x_freq;
    t_inlet  *x_inlet_width;
    t_inlet  *x_inlet_phase;
    t_inlet  *x_inlet_sync;
    t_outlet *x_outlet;
    t_float x_sr;
} t_triwave;

static t_int *triwave_perform(t_int *w)
{
    t_triwave *x = (t_triwave *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]); // freq
    t_float *in2 = (t_float *)(w[4]); // phase
    t_float *in3 = (t_float *)(w[5]); // sync
    t_float *out = (t_float *)(w[6]);
    double phase = x->x_phase;
    double last_phase_offset = x->x_last_phase_offset;
    double sr = x->x_sr;
    double output;
    while (nblock--)
    {
        double hz = *in1++;
        double phase_offset = *in2++;
        double trig = *in3++;
        double phase_step = hz / sr; // phase_step
        phase_step = phase_step > 0.5 ? 0.5 : phase_step < -0.5 ? -0.5 : phase_step; // clipped to nyq
        double phase_dev = phase_offset - last_phase_offset;
        if (phase_dev >= 1 || phase_dev <= -1)
            phase_dev = fmod(phase_dev, 1); // fmod(phase_dev)
        if (trig > 0 && trig <= 1) phase = trig;
        else
            {
            phase = phase + phase_dev;
            if (phase <= 0) phase = phase + 1.; // wrap deviated phase
            if (phase >= 1) phase = phase - 1.; // wrap deviated phase
            }
        output = phase * 4;
        if (output >= 1 && output < 3)
            output = 1 - (output - 1);
        else if (output >= 3 && output)
            output = (output - 4);
        *out++ = output;
        phase = phase + phase_step; // next phase
        last_phase_offset = phase_offset; // last phase offset
    }
    x->x_phase = phase;
    x->x_last_phase_offset = last_phase_offset;
    return (w + 7);
}

static void triwave_dsp(t_triwave *x, t_signal **sp)
{
    dsp_add(triwave_perform, 6, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *triwave_free(t_triwave *x)
{
    inlet_free(x->x_inlet_phase);
    inlet_free(x->x_inlet_sync);
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *triwave_new(t_floatarg f1, t_floatarg f2)
{
    t_triwave *x = (t_triwave *)pd_new(triwave_class);
    
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
    
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return (x);
}

void triwave_tilde_setup(void)
{
    triwave_class = class_new(gensym("triwave~"),
        (t_newmethod)triwave_new, (t_method)triwave_free,
        sizeof(t_triwave), CLASS_DEFAULT, A_DEFFLOAT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(triwave_class, t_triwave, x_freq);
    class_addmethod(triwave_class, (t_method)triwave_dsp, gensym("dsp"), A_CANT, 0);
}
