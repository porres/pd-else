// Porres 2016

#include "m_pd.h"

static t_class *noisy_class;

typedef struct _noisy
{
    t_object  x_obj;
    int x_val;
    t_outlet *x_outlet;
} t_noisy;


static t_int *noisy_perform(t_int *w)
{
    t_noisy *x = (t_noisy *)(w[1]);
    int nblock = (t_int)(w[2]);
    int *vp = (int *)(w[3]);
    t_float *input = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    // t_sample *out = (t_sample *)(w[5]);
    int val = *vp;
    while (nblock--)
    {
        t_float in = *input++;
        *out++ = in;
        // *out++ = ((float)((val & 0x7fffffff) - 0x40000000)) * (float)(1.0 / 0x40000000);
        val = val * 435898247 + 382842987;
    }
     *vp = val;
    return (w + 6);
}

static void noisy_dsp(t_noisy *x, t_signal **sp)
{
    dsp_add(noisy_perform, 5, x, sp[0]->s_n, &x->x_val, sp[0]->s_vec, sp[1]->s_vec);
}



static void *noisy_free(t_noisy *x)
{
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *noisy_new(void)
{
    t_noisy *x = (t_noisy *)pd_new(noisy_class);
    static int init = 307;
    x->x_val = (init *= 1319);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return (x);
}


void noisy_tilde_setup(void)
{
    noisy_class = class_new(gensym("noisy~"),
        (t_newmethod)noisy_new, (t_method)noisy_free,
        sizeof(t_noisy), 0, 0);
    class_addmethod(noisy_class, nullfn, gensym("signal"), 0);
    class_addmethod(noisy_class, (t_method) noisy_dsp, gensym("dsp"), A_CANT, 0);
}
