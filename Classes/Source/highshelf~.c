// Porres 2017

#include "m_pd.h"
#include <math.h>

#define PI 3.14159265358979323846

typedef struct _highshelf {
    t_object    x_obj;
    t_int       x_n;
    t_inlet    *x_inlet_freq;
    t_inlet    *x_inlet_q;
    t_inlet    *x_inlet_amp;
    t_outlet   *x_out;
    t_float     x_nyq;
    int     x_bypass;
    double  x_xnm1;
    double  x_xnm2;
    double  x_ynm1;
    double  x_ynm2;
    double  f;
    double  slope;
    double  db;
    double  a0;
    double  a1;
    double  a2;
    double  b0;
    double  b1;
    double  b2;
    } t_highshelf;

static t_class *highshelf_class;

static void update_vals(t_highshelf *x, double f, double slope, double db){
    
    x->f = f;
    x->slope = slope;
    x->db = db;

    double amp = pow(10, db / 40);
    double omega = f * PI/x->x_nyq;
    double alphaS = sin(omega) * sqrt((amp*amp + 1) * (1/slope - 1) + 2*amp);
    double cos_w = cos(omega);

    x->b0 = (amp+1) - (amp-1)*cos_w + alphaS;
    x->a0 = amp*(amp+1 + (amp-1)*cos_w + alphaS) / x->b0;
    x->a1 = -2*amp*(amp-1 + (amp+1)*cos_w) / x->b0;
    x->a2 = amp*(amp+1 + (amp-1)*cos_w - alphaS) / x->b0;
    x->b1 = -2*(amp-1 - (amp+1)*cos_w) / x->b0;
    x->b2 = -(amp+1 - (amp-1)*cos_w - alphaS) / x->b0;
    
}

static t_int *highshelf_perform(t_int *w)
{
    t_highshelf *x = (t_highshelf *)(w[1]);
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
        double xn = *in1++, f = *in2++, slope = *in3++, db = *in4++;
        double yn;

        if (f < 0.1)
            f = 0.1;
        if (f > nyq - 0.1)
            f = nyq - 0.1;
        
        if (slope < 0.000001)
            slope = 0.000001;
        if (slope > 1)
            slope = 1;

        if(f != x->f || slope != x->slope || db != x->db){
            update_vals(x, f, slope, db);
        }

        yn = x->a0 * xn + x->a1 * xnm1 + x->a2 * xnm2 + x->b1 * ynm1 + x->b2 * ynm2;
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

static void highshelf_dsp(t_highshelf *x, t_signal **sp)
{
    x->x_nyq = sp[0]->s_sr / 2;
    dsp_add(highshelf_perform, 7, x, sp[0]->s_n, sp[0]->s_vec,
            sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
}

static void highshelf_clear(t_highshelf *x)
{
    x->x_xnm1 = x->x_xnm2 = x->x_ynm1 = x->x_ynm2 = 0.;
}

static void highshelf_bypass(t_highshelf *x, t_floatarg f)
{
    x->x_bypass = (int)(f != 0);
}

static void *highshelf_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
    t_highshelf *x = (t_highshelf *)pd_new(highshelf_class);
    return (x);
}

static void *highshelf_new(t_symbol *s, int argc, t_atom *argv)
{
    t_highshelf *x = (t_highshelf *)pd_new(highshelf_class);
    float freq = 0;
    float slope = 0;
    float db = 0;
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
                    slope = argval;
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
            goto errstate;
        }
    };
/////////////////////////////////////////////////////////////////////////////////////
    x->x_inlet_freq = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_freq, freq);
    x->x_inlet_q = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_q, slope);
    x->x_inlet_amp = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_amp, db);
    x->x_out = outlet_new((t_object *)x, &s_signal);
    return (x);
    errstate:
        pd_error(x, "highshelf~: improper args");
        return NULL;
}

void highshelf_tilde_setup(void)
{
    highshelf_class = class_new(gensym("highshelf~"), (t_newmethod)highshelf_new, 0,
        sizeof(t_highshelf), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(highshelf_class, (t_method)highshelf_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(highshelf_class, nullfn, gensym("signal"), 0);
    class_addmethod(highshelf_class, (t_method)highshelf_clear, gensym("clear"), 0);
    class_addmethod(highshelf_class, (t_method)highshelf_bypass, gensym("bypass"), A_DEFFLOAT, 0);
}
