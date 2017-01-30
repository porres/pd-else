// Porres 2017

#include "m_pd.h"

static t_class *trigcount_class;

typedef struct _trigcount
{
    t_object  x_obj;
    t_float   x_count;
    t_float   x_lastin;
    t_inlet  *x_triglet;
    t_outlet *x_outlet;
} t_trigcount;

static t_int *trigcount_perform(t_int *w)
{
    t_trigcount *x = (t_trigcount *)(w[1]);
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
        if(trig > 0) count = 0;
        *out++ = count += (in > 0 && lastin <= 0);
        lastin = in;
    }
    x->x_lastin = lastin;
    x->x_count = count;
    return (w + 6);
}

static void trigcount_dsp(t_trigcount *x, t_signal **sp)
{
    dsp_add(trigcount_perform, 5, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *trigcount_free(t_trigcount *x)
{
    inlet_free(x->x_triglet);
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *trigcount_new(void)
{
    t_trigcount *x = (t_trigcount *)pd_new(trigcount_class);
    x->x_lastin = x->x_count = 0;
    x->x_triglet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return (x);
}

void trigcount_tilde_setup(void)
{
    trigcount_class = class_new(gensym("trigcount~"),
        (t_newmethod)trigcount_new, (t_method)trigcount_free,
        sizeof(t_trigcount), CLASS_DEFAULT, 0);
    class_addmethod(trigcount_class, nullfn, gensym("signal"), 0);
    class_addmethod(trigcount_class, (t_method) trigcount_dsp, gensym("dsp"), 0);
}
