// porres

#include <math.h>
#include "m_pd.h"

#define TWO_PI (M_PI * 2)

typedef struct _sin {
    t_object x_obj;
    t_inlet *sin;
    t_outlet *x_outlet;
} t_sin;

void *sin_new(void);
static t_int * sin_perform(t_int *w);
static void sin_dsp(t_sin *x, t_signal **sp);

static t_class *sin_class;

static t_int *sin_perform(t_int *w)
{
    int nblock = (int)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    while (nblock--)
    {
        double f = *in++;
        if(f >= 1 || f <= -1) f = fmod(f, 1);
        if (f == 0.5) *out++ = 0;
        else *out++ = sin(f * TWO_PI);
    }
    return (w + 4);
}

static void sin_dsp(t_sin *x, t_signal **sp)
{
    dsp_add(sin_perform, 3, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
}

void *sin_new(void)
{
    t_sin *x = (t_sin *)pd_new(sin_class);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return (x);
}

void sin_tilde_setup(void)
{
    sin_class = class_new(gensym("sin~"), (t_newmethod)sin_new, 0,
                           sizeof(t_sin), CLASS_DEFAULT, 0);
    class_addmethod(sin_class, nullfn, gensym("signal"), 0);
    class_addmethod(sin_class, (t_method) sin_dsp, gensym("dsp"), 0);
}
