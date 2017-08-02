// Porres 2017

#include "m_pd.h"
#include <math.h>

#define LOG001 log(0.001)

typedef struct _glide {
    t_object    x_obj;
    t_inlet    *x_inlet_ms;
    t_outlet   *x_out;
    t_float     x_sr_khz;
    double  x_ynm1;
    } t_glide;

static t_class *glide_class;

static t_int *glide_perform(t_int *w)
{
    t_glide *x = (t_glide *)(w[1]);
    int nblock = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    double ynm1 = x->x_ynm1;
    t_float sr_khz = x->x_sr_khz;
    while (nblock--)
    {
        double xn = *in1++, ms = *in2++, a, yn;
        
        if (ms <= 0)
            *out++ = xn;
        
        else {
            a = exp(LOG001 / (ms * sr_khz));
            yn = xn + a*(ynm1 - xn);
            
            *out++ = yn;
            ynm1 = yn;
            }
    }
    x->x_ynm1 = ynm1;
    return (w + 6);
}

static void glide_dsp(t_glide *x, t_signal **sp)
{
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    dsp_add(glide_perform, 5, x, sp[0]->s_n, sp[0]->s_vec,
            sp[1]->s_vec, sp[2]->s_vec);
}

static void glide_clear(t_glide *x)
{
    x->x_ynm1 = 0.;
}

static void *glide_new(t_symbol *s, int argc, t_atom *argv)
{
    t_glide *x = (t_glide *)pd_new(glide_class);
    float ms = 1000;
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
                    ms = argval;

                default:
                    break;
            };
            argnum++;
            argc--;
            argv++;
        }
        else
            {
                goto errstate;
            };
    };
/////////////////////////////////////////////////////////////////////////////////////

    
    x->x_inlet_ms = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_ms, ms);
    x->x_out = outlet_new((t_object *)x, &s_signal);

    return (x);
    errstate:
        pd_error(x, "glide~: improper args");
        return NULL;
}

void glide_tilde_setup(void)
{
    glide_class = class_new(gensym("glide~"), (t_newmethod)glide_new, 0,
        sizeof(t_glide), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(glide_class, (t_method)glide_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(glide_class, nullfn, gensym("signal"), 0);
    class_addmethod(glide_class, (t_method)glide_clear, gensym("clear"), 0);
}
