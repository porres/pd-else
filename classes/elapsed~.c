// Porres 2016

#include "m_pd.h"

static t_class *elapsed_class;

typedef struct _elapsed
{
    t_object  x_obj;
    t_float   x_count;
    t_float   x_total;
    t_outlet *x_outlet;
} t_elapsed;

static t_int *elapsed_perform(t_int *w)
{
    t_elapsed *x = (t_elapsed *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    t_float count = x->x_count;
    t_float total = x->x_total;
    while (nblock--)
    {
        t_float input = *in++;
        if (input > 0)
            {
            total = count;
            *out++ = total;
            count = 1;
            }
        else
            {
            *out++ = total;
            count += 1;
            }
    }
    x->x_count = count;
    x->x_total = total;
    return (w + 5);
}

static void elapsed_dsp(t_elapsed *x, t_signal **sp)
{
    dsp_add(elapsed_perform, 4, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec);
}

static void *elapsed_free(t_elapsed *x)
{
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *elapsed_new(void)
{
    t_elapsed *x = (t_elapsed *)pd_new(elapsed_class);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    x->x_count = x->x_total = 0;
    return (x);
}

void elapsed_tilde_setup(void)
{
    elapsed_class = class_new(gensym("elapsed~"),
        (t_newmethod)elapsed_new, (t_method)elapsed_free,
        sizeof(t_elapsed), CLASS_DEFAULT, 0);
    class_addmethod(elapsed_class, nullfn, gensym("signal"), 0);
    class_addmethod(elapsed_class, (t_method) elapsed_dsp, gensym("dsp"), 0);
}
