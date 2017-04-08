// Porres 2017

#include "m_pd.h"
#include <math.h>

#define PI M_PI

typedef struct _ring {
    t_object    x_obj;
    t_int       x_n;
    t_inlet    *x_inlet_freq;
    t_inlet    *x_inlet_q;
    t_outlet   *x_out;
    t_float     x_nyq;
    double  x_xnm1;
    double  x_xnm2;
    double  x_ynm1;
    double  x_ynm2;
    } t_ring;

static t_class *ring_class;

static t_int *ring_perform(t_int *w)
{
    t_ring *x = (t_ring *)(w[1]);
    int nblock = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *in3 = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);
    double xnm1 = x->x_xnm1;
    double xnm2 = x->x_xnm2;
    double ynm1 = x->x_ynm1;
    double ynm2 = x->x_ynm2;
    t_float nyq = x->x_nyq;
    while (nblock--)
    {
        double xn = *in1++, f = *in2++, t60 = *in3++;
        double omega, alphaQ, a0, cos_w, a1, a2, b0, b1, b2, yn;
        double rec_bw = (PI * t60/1000) / log(1000); // reciprocal bandwidth
        if (f < 0) f = 0;
        if (f > nyq) f = nyq;
        double q = f * rec_bw;
        if (q < 0.1) q = 0.1;
        
        omega = f * PI/nyq;
        alphaQ = sinf(omega) / (2*q);
        cos_w = cosf(omega);
        b0 = alphaQ + 1;
        a0 = alphaQ*q / b0;
        a1 = 0;
        a2 = -a0;
        b1 = -2*cos_w / b0;
        b2 = (1 - alphaQ) / b0;

        *out++ = yn = a0 * xn + a1 * xnm1 + a2 * xnm2 -b1 * ynm1 -b2 * ynm2;
        
        xnm2 = xnm1;
        xnm1 = xn;
        ynm2 = ynm1;
        ynm1 = yn;
    }
    x->x_xnm1 = xnm1;
    x->x_xnm2 = xnm2;
    x->x_ynm1 = ynm1;
    x->x_ynm2 = ynm2;
    return (w + 7);
}

static void ring_dsp(t_ring *x, t_signal **sp)
{
    x->x_nyq = sp[0]->s_sr / 2;
    dsp_add(ring_perform, 6, x, sp[0]->s_n, sp[0]->s_vec,sp[1]->s_vec, sp[2]->s_vec,
            sp[3]->s_vec);
}

static void *ring_new(t_floatarg f1, t_floatarg f2)
{
    t_ring *x = (t_ring *)pd_new(ring_class);
    x->x_inlet_freq = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_freq, f1);
    x->x_inlet_q = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_q, f2);
    x->x_out = outlet_new((t_object *)x, &s_signal);
    return (x);
}

void ring_tilde_setup(void)
{
    ring_class = class_new(gensym("ring~"), (t_newmethod)ring_new, 0,
        sizeof(t_ring), CLASS_DEFAULT, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(ring_class, (t_method)ring_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(ring_class, nullfn, gensym("signal"), 0);
}
