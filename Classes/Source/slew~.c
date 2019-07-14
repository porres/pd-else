// Porres 2017

#include "m_pd.h"
#include <math.h>

#define LOG001 log(0.001)

typedef struct _slew {
    t_object    x_obj;
    t_float     x_in;
    t_inlet    *x_inlet_ms;
    t_outlet   *x_out;
    t_float     x_sr_khz;
    double      x_ynm1;
    int         x_reset;
}t_slew;

static t_class *slew_class;

static t_int *slew_perform(t_int *w){
    t_slew *x = (t_slew *)(w[1]);
    int n = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    double ynm1 = x->x_ynm1;
    t_float sr_khz = x->x_sr_khz;
    while(n--){
        double xn = *in1++, ms = *in2++, a, yn;
        if(x->x_reset){ // reset
            x->x_reset = 0;
            *out++ = yn = xn;
        }
        else{
            if(ms <= 0)
                *out++ = xn;
            else{
                a = exp(LOG001 / (ms * sr_khz));
                yn = xn + a*(ynm1 - xn);
                *out++ = yn;
                ynm1 = yn;
            }
        }
    }
    x->x_ynm1 = ynm1;
    return(w+6);
}

static void slew_dsp(t_slew *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    dsp_add(slew_perform, 5, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void slew_reset(t_slew *x){
    x->x_reset = 1;
}

static void *slew_new(t_symbol *s, int argc, t_atom *argv){
    s = NULL;
    t_slew *x = (t_slew *)pd_new(slew_class);
    float ms = 0;
    x->x_reset = 0;
/////////////////////////////////////////////////////////////////////////////////////
    int argnum = 0;
    while(argc > 0){
        if(argv -> a_type == A_FLOAT){ //if current argument is a float
            t_float argval = atom_getfloatarg(0, argc, argv);
            switch(argnum){
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
                goto errstate;
    };
/////////////////////////////////////////////////////////////////////////////////////
    x->x_inlet_ms = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_ms, ms);
    x->x_out = outlet_new((t_object *)x, &s_signal);
    return(x);
errstate:
    pd_error(x, "[slew~]: improper args");
    return NULL;
}

void slew_tilde_setup(void){
    slew_class = class_new(gensym("slew~"), (t_newmethod)slew_new, 0,
            sizeof(t_slew), 0, A_GIMME, 0);
    CLASS_MAINSIGNALIN(slew_class, t_slew, x_in);
    class_addmethod(slew_class, (t_method)slew_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(slew_class, (t_method)slew_reset, gensym("reset"), 0);
}
