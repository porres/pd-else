// Porres 2016

#include "m_pd.h"

static t_class *timer_class;

typedef struct _timer
{
    t_object  x_obj;
    t_float   x_count;
    t_float   x_total;
    t_outlet *x_outlet;
} t_timer;

static t_int *timer_perform(t_int *w)
{
    t_timer *x = (t_timer *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    t_float count = x->x_count;
    t_float total = x->x_total;
    while (nblock--)
    {
        t_float trig = *in++;
        if (trig > 0) {
            total = count;
            *out++ = total;
            count = 1;
            }
        else {
        *out++ = total;
        count += 1;
        }
    }
    x->x_count = count;
    x->x_total = total;
    return (w + 5);
}

static void timer_dsp(t_timer *x, t_signal **sp)
{
    dsp_add(timer_perform, 4, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec);
}

static void *timer_free(t_timer *x)
{
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *timer_new(void)
{
    t_timer *x = (t_timer *)pd_new(timer_class);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    x->x_count = x->x_total = 0;
    return (x);
}

void timer_tilde_setup(void)
{
    timer_class = class_new(gensym("timer~"),
        (t_newmethod)timer_new, (t_method)timer_free,
        sizeof(t_timer), CLASS_DEFAULT, 0);
    class_addmethod(timer_class, nullfn, gensym("signal"), 0);
    class_addmethod(timer_class, (t_method) timer_dsp, gensym("dsp"), 0);
}
