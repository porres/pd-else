// Porres 2016

#include "m_pd.h"
#include "math.h"

static t_class *flipflop_class;

typedef struct _flipflop
{
    t_object x_obj;
    t_float  x_x1m1;
    t_float  x_x2m1;
    t_float  x_init;
    t_float  x_output;
} t_flipflop;

static t_int *flipflop_perform(t_int *w)
{
    t_flipflop *x = (t_flipflop *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    t_float x1m1 = x->x_x1m1;
    t_float x2m1 = x->x_x2m1;
    t_float output = x->x_output;
    while (nblock--)
    {
    float x1 = *in1++;
    float x2 = *in2++;
    if (x->x_init) {
            output = 1;
            x->x_init = 0;
        }
    else {
        t_int off = x2 > 0 && x2m1 <= 0;
        if (off) output  = 0.;
        else {
            t_int on = x1 > 0 && x1m1 <= 0;
            if (on && output == 0) output = 1.;
            }
            }
        *out++ = output;
        x1m1 = x1;
        x2m1 = x2;
    }
    x->x_x1m1 = x1m1;
    x->x_x2m1 = x2m1;
    x->x_output = output;
    return (w + 6);
}

static void flipflop_dsp(t_flipflop *x, t_signal **sp)
{
    dsp_add(flipflop_perform, 5, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *flipflop_new(t_floatarg f)
{
    t_flipflop *x = (t_flipflop *)pd_new(flipflop_class);
    x->x_init = f != 0;
    x->x_x1m1 = x->x_x2m1 = 0;
    inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    return (x);
}

void flipflop_tilde_setup(t_floatarg f)
{
    flipflop_class = class_new(gensym("flipflop~"),
        (t_newmethod)flipflop_new,
        0,
        sizeof(t_flipflop),
        CLASS_DEFAULT,
        A_DEFFLOAT,
        0);
        class_addmethod(flipflop_class, nullfn, gensym("signal"), 0);
        class_addmethod(flipflop_class, (t_method)flipflop_dsp, gensym("dsp"), A_CANT, 0);
}

