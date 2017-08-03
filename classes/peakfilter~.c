// Porres 2017

#include "m_pd.h"
#include <math.h>

#define PI M_PI
#define HALF_LOG2 log(2) * 0.5

typedef struct _peakfilter {
    t_object    x_obj;
    t_int       x_n;
    t_inlet    *x_inlet_freq;
    t_inlet    *x_inlet_q;
    t_inlet    *x_inlet_amp;
    t_outlet   *x_out;
    t_float     x_nyq;
    int     x_bw;
    int     x_bypass;
    double  x_xnm1;
    double  x_xnm2;
    double  x_ynm1;
    double  x_ynm2;
    } t_peakfilter;

static t_class *peakfilter_class;

static t_int *peakfilter_perform(t_int *w)
{
    t_peakfilter *x = (t_peakfilter *)(w[1]);
    int nblock = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *in3 = (t_float *)(w[5]);
    t_float *in4 = (t_float *)(w[6]);
    t_float *out = (t_float *)(w[7]);
    double xnm1 = x->x_xnm1;
    double xnm2 = x->x_xnm2;
    double ynm1 = x->x_ynm1;
    double ynm2 = x->x_ynm2;
    t_float nyq = x->x_nyq;
    while (nblock--)
    {
        double xn = *in1++, f = *in2++, reson = *in3++, db = *in4++;
        double q, amp, omega, alphaQ, cos_w, a0, a1, a2, b0, b1, b2, yn;
        int q_bypass;

        if (f < 0.1)
            f = 0.1;
        if (f > nyq - 0.1)
            f = nyq - 0.1;
        
        omega = f * PI/nyq; // hz2rad
        
        if (x->x_bw) // reson is bw in octaves
            {
            if (reson < 0.000001)
                reson = 0.000001;
            q = 1 / (2 * sinh(HALF_LOG2 * reson * omega/sin(omega)));
            }
        else
            q = reson;
        
        if (q < 0.000001)
            q = 0.000001; // prevent blow-up
        
        amp = pow(10, db / 40);
        alphaQ = sin(omega) / (2*q);
        cos_w = cos(omega);
        b0 = alphaQ/amp + 1;
        a0 = (1 + alphaQ*amp) / b0;
        a1 = -2*cos_w / b0;
        a2 = (1 - alphaQ*amp) / b0;
        b1 = 2*cos_w / b0;
        b2 = (alphaQ/amp - 1) / b0;
        
        yn = a0 * xn + a1 * xnm1 + a2 * xnm2 + b1 * ynm1 + b2 * ynm2;
        
        if(x->x_bypass)
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
    return (w + 8);
}

static void peakfilter_dsp(t_peakfilter *x, t_signal **sp)
{
    x->x_nyq = sp[0]->s_sr / 2;
    dsp_add(peakfilter_perform, 7, x, sp[0]->s_n, sp[0]->s_vec,
            sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
}

static void peakfilter_clear(t_peakfilter *x)
{
    x->x_xnm1 = x->x_xnm2 = x->x_ynm1 = x->x_ynm2 = 0.;
}

static void peakfilter_bypass(t_peakfilter *x, t_floatarg f)
{
    x->x_bypass = (int)(f != 0);
}

static void *peakfilter_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
    t_peakfilter *x = (t_peakfilter *)pd_new(peakfilter_class);
    return (x);
}

static void peakfilter_bw(t_peakfilter *x)
{
    x->x_bw = 1;
}

static void peakfilter_q(t_peakfilter *x)
{
    x->x_bw = 0;
}

static void *peakfilter_new(t_symbol *s, int argc, t_atom *argv)
{
    t_peakfilter *x = (t_peakfilter *)pd_new(peakfilter_class);
    float freq = 0;
    float reson = 0;
    float db = 0;
    int bw = 0;
/////////////////////////////////////////////////////////////////////////////////////
    int argnum = 0;
    while(argc > 0)
    {
        if(argv -> a_type == A_FLOAT)
        { //if current argument is a float
            t_float argval = atom_getfloatarg(0, argc, argv);
            switch(argnum)
            {
                case 0:
                    freq = argval;
                    break;
                case 1:
                    reson = argval;
                    break;
                case 2:
                    db = argval;
                    break;
                default:
                    break;
            };
            argnum++;
            argc--;
            argv++;
        }
        else if (argv -> a_type == A_SYMBOL)
        {
            t_symbol *curarg = atom_getsymbolarg(0, argc, argv);
            if(strcmp(curarg->s_name, "-bw")==0)
            {
                bw = 1;
                argc -= 1;
                argv += 1;
            }
            else
            {
                goto errstate;
            };
        }
    };
/////////////////////////////////////////////////////////////////////////////////////
    x->x_bw = bw;
    x->x_inlet_freq = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_freq, freq);
    x->x_inlet_q = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_q, reson);
    x->x_inlet_amp = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_amp, db);
    x->x_out = outlet_new((t_object *)x, &s_signal);
    return (x);
    errstate:
        pd_error(x, "peakfilter~: improper args");
        return NULL;
}

void peakfilter_tilde_setup(void)
{
    peakfilter_class = class_new(gensym("peakfilter~"), (t_newmethod)peakfilter_new, 0,
        sizeof(t_peakfilter), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(peakfilter_class, (t_method)peakfilter_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(peakfilter_class, nullfn, gensym("signal"), 0);
    class_addmethod(peakfilter_class, (t_method)peakfilter_clear, gensym("clear"), 0);
    class_addmethod(peakfilter_class, (t_method)peakfilter_bypass, gensym("bypass"), A_DEFFLOAT, 0);
    class_addmethod(peakfilter_class, (t_method)peakfilter_bw, gensym("bw"), 0);
    class_addmethod(peakfilter_class, (t_method)peakfilter_q, gensym("q"), 0);
}
