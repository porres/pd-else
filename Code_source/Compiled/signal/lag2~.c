// Porres 2017

#include "m_pd.h"
#include <math.h>

#define LOG001 log(0.001)

typedef struct _lag2{
    t_object    x_obj;
    t_float     x_in;
    t_inlet    *x_inlet_ms_up;
    t_inlet    *x_inlet_ms_down;
    t_outlet   *x_out;
    t_float     x_sr_khz;
    double     *x_last_out;
    int         x_reset;
    int         x_nchans;
}t_lag2;

static t_class *lag2_class;

static t_int *lag2_perform(t_int *w){
    t_lag2 *x = (t_lag2 *)(w[1]);
    int n = (int)(w[2]);
    int ch2 = (int)(w[3]);
    int ch3 = (int)(w[4]);
    t_float *in1 = (t_float *)(w[5]);
    t_float *in2 = (t_float *)(w[6]);
    t_float *in3 = (t_float *)(w[7]);
    t_float *out = (t_float *)(w[8]);
    double *last_out = x->x_last_out;
    t_float sr_khz = x->x_sr_khz;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            double xn = in1[j*n + i];
            double ms_up = ch2 == 1 ? in2[i] : in2[j*n + i];
            double ms_down = ch3 == 1 ? in3[i] : in3[j*n + i];
            double a, yn;
            if(x->x_reset){ // reset
                out[j*n + i] = last_out[j] = xn;
                if(j == (x->x_nchans - 1))
                    x->x_reset = 0;
            }
            else{
                if(xn >= last_out[j])
                    a = ms_up > 0 ? exp(LOG001 / (ms_up * sr_khz)) : 0;
                else
                    a = ms_down > 0 ? exp(LOG001 / (ms_down * sr_khz)) : 0;
                if(a == 0)
                    out[j*n + i] = yn = xn;
                else
                    out[j*n + i] = yn = xn + a*(last_out[j] - xn);
                last_out[j] = yn;
            }
        }
    }
    x->x_last_out = last_out;
    return(w+9);
}

static void lag2_dsp(t_lag2 *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    int chs = sp[0]->s_nchans, ch2 = sp[1]->s_nchans, ch3 = sp[2]->s_nchans, n = sp[0]->s_n;
    signal_setmultiout(&sp[3], chs);
    if(x->x_nchans != chs){
        x->x_last_out = (double *)resizebytes(x->x_last_out,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_nchans = chs;
    }
    if((ch2 > 1 && ch2 != x->x_nchans) || (ch3 > 1 && ch3 != x->x_nchans)){
        dsp_add_zero(sp[3]->s_vec, chs*n);
        pd_error(x, "[lag2~]: channel sizes mismatch");
    }
    else
        dsp_add(lag2_perform, 8, x, n, ch2, ch3,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void lag2_reset(t_lag2 *x){
    x->x_reset = 1;
}
                        
static void *lag2_free(t_lag2 *x){
    inlet_free(x->x_inlet_ms_up);
    inlet_free(x->x_inlet_ms_down);
    freebytes(x->x_last_out, x->x_nchans * sizeof(*x->x_last_out));
    return(void *)x;
}

static void *lag2_new(t_floatarg f1, t_floatarg f2){
    t_lag2 *x = (t_lag2 *)pd_new(lag2_class);
    x->x_last_out = (double *)getbytes(sizeof(*x->x_last_out));
    x->x_reset = 0;
    x->x_inlet_ms_up = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_ms_up, f1);
    x->x_inlet_ms_down = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_ms_down, f2);
    x->x_out = outlet_new((t_object *)x, &s_signal);
    return(x);
}

void lag2_tilde_setup(void){
    lag2_class = class_new(gensym("lag2~"), (t_newmethod)lag2_new, (t_method)lag2_free,
        sizeof(t_lag2), CLASS_MULTICHANNEL, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(lag2_class, nullfn, gensym("signal"), 0);
    class_addmethod(lag2_class, (t_method)lag2_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(lag2_class, (t_method)lag2_reset, gensym("reset"), 0);
}
