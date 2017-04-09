// Porres 2017

#include "m_pd.h"
#include <math.h>

#define PI M_PI
#define HALF_LOG2 log(2)/2

typedef struct _phaseshifter {
    t_object    x_obj;
    t_int       x_n;
    t_inlet    *x_inlet_freq;
    t_inlet    *x_inlet_q;
    t_outlet   *x_out;
    t_float     x_nyq;
    int     x_bypass;
    int     x_bw;
    double  x_xnm1;
    double  x_xnm2;
    double  x_ynm1;
    double  x_ynm2;
    } t_phaseshifter;

static t_class *phaseshifter_class;

static t_int *phaseshifter_perform(t_int *w)
{
    t_phaseshifter *x = (t_phaseshifter *)(w[1]);
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
        double q, omega, alphaQ, cos_w, a0, a1, a2, b0, b1, b2, yn;
        
        if (f < 0.000001)
            f = 0.000001;
        if (f > nyq - 0.000001)
            f = nyq - 0.000001;
        
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
            q = 0.000001;

        alphaQ = sin(omega) / (2*q);
        cos_w = cos(omega);
        b0 = alphaQ + 1;
        a0 = (1 - alphaQ) / b0;
        a1 = -2*cos_w / b0;
        a2 = 1;
        b1 = -a1;
        b2 = (alphaQ - 1) / b0;
        
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
    return (w + 7);
}

static void phaseshifter_dsp(t_phaseshifter *x, t_signal **sp)
{
    x->x_nyq = sp[0]->s_sr / 2;
    dsp_add(phaseshifter_perform, 6, x, sp[0]->s_n, sp[0]->s_vec,
            sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void phaseshifter_clear(t_phaseshifter *x)
{
    x->x_xnm1 = x->x_xnm2 = x->x_ynm1 = x->x_ynm2 = 0.;
}

static void phaseshifter_bypass(t_phaseshifter *x, t_floatarg f)
{
    x->x_bypass = (int)(f != 0);
}

static void phaseshifter_bw(t_phaseshifter *x)
{
    x->x_bw = 1;
}

static void phaseshifter_q(t_phaseshifter *x)
{
    x->x_bw = 0;
}

static void *phaseshifter_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
    t_phaseshifter *x = (t_phaseshifter *)pd_new(phaseshifter_class);
    return (x);
}


static void *phaseshifter_new(t_symbol *s, int argc, t_atom *argv)
{
    t_phaseshifter *x = (t_phaseshifter *)pd_new(phaseshifter_class);
    float freq = 0;
    float reson = 1;
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
    x->x_out = outlet_new((t_object *)x, &s_signal);

    return (x);
    errstate:
        pd_error(x, "phaseshifter~: improper args");
        return NULL;
}

void phaseshifter_tilde_setup(void)
{
    phaseshifter_class = class_new(gensym("phaseshifter~"), (t_newmethod)phaseshifter_new, 0,
        sizeof(t_phaseshifter), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(phaseshifter_class, (t_method)phaseshifter_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(phaseshifter_class, nullfn, gensym("signal"), 0);
    class_addmethod(phaseshifter_class, (t_method)phaseshifter_clear, gensym("clear"), 0);
    class_addmethod(phaseshifter_class, (t_method)phaseshifter_bypass, gensym("bypass"), A_DEFFLOAT, 0);
    class_addmethod(phaseshifter_class, (t_method)phaseshifter_bw, gensym("bw"), 0);
    class_addmethod(phaseshifter_class, (t_method)phaseshifter_q, gensym("q"), 0);
}
