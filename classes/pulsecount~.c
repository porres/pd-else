// Porres 2017

#include "m_pd.h"

static t_class *pulsecount_class;

typedef struct _pulsecount
{
    t_object  x_obj;
    t_float   x_count;
    t_float   x_lastin;
    t_inlet  *x_triglet;
    t_outlet *x_outlet;
} t_pulsecount;

static t_int *pulsecount_perform(t_int *w)
{
    t_pulsecount *x = (t_pulsecount *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    t_float lastin = x->x_lastin;
    t_float count = x->x_count;
    while (nblock--)
    {
        t_float in = *in1++;
        t_float trig = *in2++;
        long pulse = (in > 0 && lastin <= 0);
        if(trig > 0)
            count = 0;
        else if(pulse)
            count++;
            
        *out++ = count;
        lastin = in;
    }
    x->x_lastin = lastin;
    x->x_count = count;
    return (w + 6);
}

static void pulsecount_dsp(t_pulsecount *x, t_signal **sp)
{
    dsp_add(pulsecount_perform, 5, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *pulsecount_free(t_pulsecount *x)
{
    inlet_free(x->x_triglet);
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *pulsecount_new(void)
{
    t_pulsecount *x = (t_pulsecount *)pd_new(pulsecount_class);
    x->x_lastin = 1;
    x->x_count = 0;
    x->x_triglet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return (x);
}

void pulsecount_tilde_setup(void)
{
    pulsecount_class = class_new(gensym("pulsecount~"),
        (t_newmethod)pulsecount_new, (t_method)pulsecount_free,
        sizeof(t_pulsecount), CLASS_DEFAULT, 0);
    class_addmethod(pulsecount_class, nullfn, gensym("signal"), 0);
    class_addmethod(pulsecount_class, (t_method) pulsecount_dsp, gensym("dsp"), A_CANT, 0);
}
