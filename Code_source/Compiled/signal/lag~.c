// Porres 2019

#include "m_pd.h"
#include <math.h>

#define LOG001 log(0.001)

typedef struct _lag{
    t_object    x_obj;
    t_float     x_in;
    t_inlet    *x_inlet_ms;
    t_float     x_sr_khz;
    double     *x_last_out;
    int         x_reset;
    int         x_nchans;
    float    x_f;
}t_lag;

static t_class *lag_class;

static t_int *lag_perform(t_int *w){
    t_lag *x = (t_lag *)(w[1]);
    int n = (int)(w[2]);
    int ch2 = (int)(w[3]);
    t_float *in1 = (t_float *)(w[4]);
    t_float *in2 = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);
    t_float sr_khz = x->x_sr_khz;
    double *last_out = x->x_last_out;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            double in = in1[j*n + i];
            double ms = ch2 == 1 ? in2[i] : in2[j*n + i];
            if(x->x_reset){ // reset
                out[j*n + i] = last_out[j] = in;
                if(j == (x->x_nchans - 1))
                    x->x_reset = 0;
            }
            else{
                if(ms <= 0)
                    out[j*n + i] = last_out[j] = in;
                else{
                    double a = exp(LOG001 / (ms * sr_khz));
                    out[j*n + i] = last_out[j] = in + a*(last_out[j] - in);
                }
            }
        }
    }
    x->x_last_out = last_out;
    return(w+7);
}

static void lag_dsp(t_lag *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    int chs = sp[0]->s_nchans, ch2 = sp[1]->s_nchans, n = sp[0]->s_n;
    signal_setmultiout(&sp[2], chs);
    if(x->x_nchans != chs){
        x->x_last_out = (double *)resizebytes(x->x_last_out,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_nchans = chs;
    }
    if(ch2 > 1 && ch2 != x->x_nchans){
        dsp_add_zero(sp[2]->s_vec, chs*n);
        pd_error(x, "[lag~]: channel sizes mismatch");
    }
    else
        dsp_add(lag_perform, 6, x, n, ch2, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void lag_reset(t_lag *x){
    x->x_reset = 1;
}

static void *lag_free(t_lag *x){
    inlet_free(x->x_inlet_ms);
    freebytes(x->x_last_out, x->x_nchans * sizeof(*x->x_last_out));
    return(void *)x;
}

static void *lag_new(t_floatarg f){
    t_lag *x = (t_lag *)pd_new(lag_class);
    x->x_last_out = (double *)getbytes(sizeof(*x->x_last_out));
    x->x_reset = 0;
    x->x_inlet_ms = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
         pd_float((t_pd *)x->x_inlet_ms, f);
    outlet_new((t_object *)x, &s_signal);
    return(x);
}

void lag_tilde_setup(void){
    lag_class = class_new(gensym("lag~"), (t_newmethod)lag_new, (t_method)lag_free,
        sizeof(t_lag), CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(lag_class, t_lag, x_f);
    class_addmethod(lag_class, (t_method)lag_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(lag_class, (t_method)lag_reset, gensym("reset"), 0);
}
