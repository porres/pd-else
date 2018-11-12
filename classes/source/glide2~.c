// Porres 2017

#include "m_pd.h"
#include <math.h>

#define LOG001 log(0.001)

typedef struct _glide2 {
    t_object    x_obj;
    t_float     x_in;
    t_inlet    *x_inlet_ms;
    t_outlet   *x_out;
    t_float     x_sr_khz;
    double      x_ynm1;
}t_glide2;

static t_class *glide2_class;

static t_int *glide2_perform(t_int *w)
{
    t_glide2 *x = (t_glide2 *)(w[1]);
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

static void glide2_dsp(t_glide2 *x, t_signal **sp)
{
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    dsp_add(glide2_perform, 5, x, sp[0]->s_n, sp[0]->s_vec,
            sp[1]->s_vec, sp[2]->s_vec);
}

static void glide2_reset(t_glide2 *x){
    x->x_ynm1 = 0.;
}

static void *glide2_new(t_symbol *s, int argc, t_atom *argv)
{
    t_glide2 *x = (t_glide2 *)pd_new(glide2_class);
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
        pd_error(x, "glide2~: improper args");
        return NULL;
}

void glide2_tilde_setup(void){
    glide2_class = class_new(gensym("glide2~"), (t_newmethod)glide2_new, 0,
            sizeof(t_glide2), 0, A_GIMME, 0);
    CLASS_MAINSIGNALIN(glide2_class, t_glide2, x_in);
    class_addmethod(glide2_class, (t_method)glide2_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(glide2_class, (t_method)glide2_reset, gensym("reset"), 0);
}
