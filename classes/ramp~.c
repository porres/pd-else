// Porres 2017

#include "m_pd.h"
#include "math.h"

static t_class *ramp_class;

typedef struct _ramp
{
    t_object x_obj;
    double  x_phase;
    float   x_min;
    float   x_max;
    t_inlet  *x_inlet_inc;
    t_inlet  *x_inlet_min;
    t_inlet  *x_inlet_max;
    t_outlet *x_outlet;
} t_ramp;

static t_int *ramp_perform(t_int *w)
{
    t_ramp *x = (t_ramp *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]); // trigger
    t_float *in2 = (t_float *)(w[4]); // increment
    t_float *in3 = (t_float *)(w[5]); // min
    t_float *in4 = (t_float *)(w[6]); // max
    t_float *out = (t_float *)(w[7]);
    double phase = x->x_phase;
    double output;
    while (nblock--)
    {
        double trig = *in1++; // trigger
        double phase_step = *in2++; // phase_step
        float min = *in3++; // min
        float max = *in4++; // max
        if(min == max)
            output = min;
        else
            {
            if (trig == 1)
                phase = min;
            else
                {
                if(min > max)
                    { // swap values
                        float temp;
                        temp = max;
                        max = min;
                        min = temp;
                    };
                if(phase < min || phase >= max) // wrap phase
                    {
                    float range = max - min;
                    if(phase < min)
                        {
                        while(phase < min)
                        phase += range; // wrapped phase
                        }
                    else
                        phase = fmod(phase - min, range) + min; // wrapped phase
                    }
                }
            output = phase;
            }
        *out++ = output;
        phase += phase_step; // next phase
    }
    x->x_phase = phase;
    return (w + 8);
}

static void ramp_dsp(t_ramp *x, t_signal **sp)
{
    dsp_add(ramp_perform, 7, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
}

static void *ramp_free(t_ramp *x)
{
    inlet_free(x->x_inlet_inc);
    inlet_free(x->x_inlet_min);
    inlet_free(x->x_inlet_max);
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *ramp_new(t_symbol *s, int ac, t_atom *av)
{
    t_ramp *x = (t_ramp *)pd_new(ramp_class);
    x->x_inlet_inc = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_inc, 0);
    x->x_inlet_min = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_min, 0);
    x->x_inlet_max = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_max, 0);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return (x);
}

void ramp_tilde_setup(void)
{
    ramp_class = class_new(gensym("ramp~"),
        (t_newmethod)ramp_new, (t_method)ramp_free,
        sizeof(t_ramp), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(ramp_class, nullfn, gensym("signal"), 0);
    class_addmethod(ramp_class, (t_method)ramp_dsp, gensym("dsp"), A_CANT, 0);
}
