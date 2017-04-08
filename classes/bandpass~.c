// Porres 2017

#include "m_pd.h"
#include <math.h>

#define PI M_PI

typedef struct _bandpass {
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
    } t_bandpass;

static t_class *bandpass_class;

static t_int *bandpass_perform(t_int *w)
{
    t_bandpass *x = (t_bandpass *)(w[1]);
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
        double xn = *in1++, f = *in2++, q = *in3++;
        double omega, alphaQ, cos_w, a0, a2, b0, b1, b2, yn;
        if (f < 0) f = 0;
        if (f > nyq) f = nyq;
        if (q < 0.1) q = 0.1;
        
        omega = f * PI/nyq;
        alphaQ = sin(omega) / (2*q);
        cos_w = cos(omega);
        b0 = alphaQ + 1;
        a0 = alphaQ / b0;
        a2 = -a0;
        b1 = 2*cos_w / b0;
        b2 = (alphaQ - 1) / b0;

        *out++ = yn = a0 * xn + a2 * xnm2 + b1 * ynm1 + b2 * ynm2;
        
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

static void bandpass_dsp(t_bandpass *x, t_signal **sp)
{
    x->x_nyq = sp[0]->s_sr / 2;
    dsp_add(bandpass_perform, 6, x, sp[0]->s_n, sp[0]->s_vec,sp[1]->s_vec, sp[2]->s_vec,
            sp[3]->s_vec);
}

static void *bandpass_new(t_floatarg f1, t_floatarg f2)
{
    t_bandpass *x = (t_bandpass *)pd_new(bandpass_class);
    x->x_inlet_freq = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_freq, f1);
    x->x_inlet_q = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_q, f2);
    x->x_out = outlet_new((t_object *)x, &s_signal);
    return (x);
}

void bandpass_tilde_setup(void)
{
    bandpass_class = class_new(gensym("bandpass~"), (t_newmethod)bandpass_new, 0,
        sizeof(t_bandpass), CLASS_DEFAULT, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(bandpass_class, (t_method)bandpass_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(bandpass_class, nullfn, gensym("signal"), 0);
}
