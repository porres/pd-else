// Porres 2016

#include "m_pd.h"
#include <math.h>

#define HALF_PI (M_PI * 0.5)

static t_class *rotate_class;

typedef struct _rotate
{
    t_object x_obj;
    t_inlet  *x_pos_inlet;
} t_rotate;

static t_int *rotate_perform(t_int *w)
{
    t_rotate *x = (t_rotate *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]); // in1
    t_float *in2 = (t_float *)(w[4]); // in2
    t_float *in3 = (t_float *)(w[5]); // pos
    t_float *out1 = (t_float *)(w[6]); // L
    t_float *out2 = (t_float *)(w[7]); // R
    while (nblock--)
        {
        float in_l = *in1++;
        float in_r = *in2++;
        float pos = *in3++;
        if(pos > 1.)
            pos = 1.;
        if(pos < -1.)
            pos = -1.;
        float cosine = (pos == 1 || pos == -1 ? 0 : cos(pos * HALF_PI));
        float sine = fabs(sin(pos * HALF_PI));
        *out1++ = (in_l * cosine) + (in_r * sine);
        *out2++ = (in_l * sine) + (in_r * cosine);
        }
    return (w + 8);
}

static void rotate_dsp(t_rotate *x, t_signal **sp)
{
    dsp_add(rotate_perform, 7, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
}

static void *rotate_free(t_rotate *x)
{
    inlet_free(x->x_pos_inlet);
    return (void *)x;
}

static void *rotate_new (t_floatarg f1)
{
    t_rotate *x = (t_rotate *)pd_new(rotate_class);
/////
    float init_pos;
    if(f1 > 1.)
        f1 = 1.;
    if (f1 < -1.)
        f1 = -1.;
    init_pos = f1;
/////
    inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_pos_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_pos_inlet, init_pos);
    outlet_new((t_object *)x, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    return (x);
    errstate:
        pd_error(x, "rotate~: improper args");
        return NULL;
}

void rotate_tilde_setup(void)
{
    rotate_class = class_new(gensym("rotate~"), (t_newmethod)rotate_new,
        (t_method)rotate_free, sizeof(t_rotate), CLASS_DEFAULT, A_DEFFLOAT, 0);
        class_addmethod(rotate_class, nullfn, gensym("signal"), 0);
        class_addmethod(rotate_class, (t_method)rotate_dsp, gensym("dsp"), A_CANT, 0);
}
