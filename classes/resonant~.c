// Porres 2017

#include "m_pd.h"
#include <math.h>

#define PI M_PI

typedef struct _resonant {
    t_object    x_obj;
    t_int       x_n;
    t_inlet    *x_inlet_freq;
    t_inlet    *x_inlet_q;
    t_outlet   *x_out;
    t_float     x_nyq;
    t_float     x_bypass;
    t_float     x_t60;
    double  x_xnm1;
    double  x_xnm2;
    double  x_ynm1;
    double  x_ynm2;
    } t_resonant;

static t_class *resonant_class;

static t_int *resonant_perform(t_int *w)
{
    t_resonant *x = (t_resonant *)(w[1]);
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
        double xn = *in1++, f = *in2++, reson = *in3++;
        double q, omega, alphaQ, cos_w, a0, a2, b0, b1, b2, yn;
        if (x->x_t60) // reson is t60 in ms
            q = f * (PI * reson/1000) / log(1000);
        else
            q = reson;
        int q_bypass;
        if (f < 0.000001)
            f = 0.000001;
        if (f > nyq - 0.000001)
            f = nyq - 0.000001;
        if (q < 0.000001)
            {
            q = 0.000001;
            q_bypass = 1;
            }
        else
            q_bypass = 0;
        omega = f * PI/nyq;
        alphaQ = sin(omega) / (2*q);
        cos_w = cos(omega);
        b0 = alphaQ + 1;
        a0 = alphaQ*q / b0;
        a2 = -a0;
        b1 = 2*cos_w / b0;
        b2 = (alphaQ - 1) / b0;
        yn = a0 * xn + a2 * xnm2 + b1 * ynm1 + b2 * ynm2;
        if(x->x_bypass || q_bypass)
            *out++ = xn;
        else
            *out++ = yn;
        
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

static void resonant_dsp(t_resonant *x, t_signal **sp)
{
    x->x_nyq = sp[0]->s_sr / 2;
    dsp_add(resonant_perform, 6, x, sp[0]->s_n, sp[0]->s_vec,sp[1]->s_vec, sp[2]->s_vec,
            sp[3]->s_vec);
}

static void resonant_clear(t_resonant *x)
{
    x->x_xnm1 = x->x_xnm2 = x->x_ynm1 = x->x_ynm2 = 0.;
}

static void resonant_bypass(t_resonant *x, t_floatarg f)
{
    x->x_bypass = (int)(f != 0);
}

static void resonant_t60(t_resonant *x, t_floatarg f)
{
    x->x_t60 = (int)(f != 0);
}

static void *resonant_new(t_floatarg f1, t_floatarg f2)
{
    t_resonant *x = (t_resonant *)pd_new(resonant_class);
    x->x_inlet_freq = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_freq, f1);
    x->x_inlet_q = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_q, f2);
    x->x_out = outlet_new((t_object *)x, &s_signal);
    return (x);
}

void resonant_tilde_setup(void)
{
    resonant_class = class_new(gensym("resonant~"), (t_newmethod)resonant_new, 0,
        sizeof(t_resonant), CLASS_DEFAULT, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(resonant_class, (t_method)resonant_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(resonant_class, nullfn, gensym("signal"), 0);
    class_addmethod(resonant_class, (t_method)resonant_clear, gensym("clear"), 0);
    class_addmethod(resonant_class, (t_method)resonant_bypass, gensym("bypass"), A_DEFFLOAT, 0);
    class_addmethod(resonant_class, (t_method)resonant_t60, gensym("t60"), A_DEFFLOAT, 0);
}
