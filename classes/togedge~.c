// Porres 2016

#include "m_pd.h"
#include "math.h"

static t_class *togedge_class;

typedef struct _togedge
{
    t_object x_obj;
    t_float  x_x1m1;
} t_togedge;

static t_int *togedge_perform(t_int *w)
{
    t_togedge *x = (t_togedge *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_float *out1 = (t_float *)(w[4]);
    t_float *out2 = (t_float *)(w[5]);
    t_float x1m1 = x->x_x1m1;
    while (nblock--)
    {
    float x1 = *in++;
    *out1++ =  x1 != 0 && x1m1 == 0;
    *out2++ =  x1 == 0 && x1m1 != 0;
    x1m1 = x1;
    }
    x->x_x1m1 = x1m1;
    return (w + 6);
}

static void togedge_dsp(t_togedge *x, t_signal **sp)
{
    dsp_add(togedge_perform, 5, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *togedge_new(void)
{
    t_togedge *x = (t_togedge *)pd_new(togedge_class);
    x->x_x1m1 = 0;
    outlet_new((t_object *)x, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    return (x);
}

void togedge_tilde_setup(t_floatarg f)
{
    togedge_class = class_new(gensym("togedge~"),
        (t_newmethod)togedge_new,
        0,
        sizeof(t_togedge),
        CLASS_DEFAULT,
        0);
        class_addmethod(togedge_class, nullfn, gensym("signal"), 0);
        class_addmethod(togedge_class, (t_method)togedge_dsp, gensym("dsp"), A_CANT, 0);
}
