// Porres 2021-2025

#include <m_pd.h>
#include "math.h"

typedef struct _pimpmul{
    t_object    x_obj;
    t_inlet    *x_inlet_rate;
    t_int       x_n;
    t_int       x_ch1;
    t_int       x_ch2;
    int         x_nchs;
    double     *x_last_in;
    double     *x_last_out;
    double     *x_last_rate;
}t_pimpmul;

static t_class *pimpmul_class;

static t_int *pimpmul_perform(t_int *w){
    t_pimpmul *x = (t_pimpmul *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *out1 = (t_float *)(w[4]);
    t_float *out2 = (t_float *)(w[5]);
    double *last_in = x->x_last_in;
    double *last_rate = x->x_last_rate;
    double *last_out = x->x_last_out;
    double output;
    for(int j = 0; j < x->x_nchs; j++){
        for(int i = 0, n = x->x_n; i < n; i++){
            double input = x->x_ch1 == 1 ? in1[i] : in1[j*n + i];
            double rate = x->x_ch2 == 1 ? in2[i] : in2[j*n + i];
            double delta = (input - last_in[j]);
            if(fabs(delta) >= 0.5){
                if(delta < 0)
                    delta += 1;
                else
                    delta -= 1;
            }
            if(rate != last_rate[j]){
                out1[j*n + i] = output = fmod(input * rate, 1);
                out2[j*n + i] = 0;
            }
            else{
                output = fmod(delta * rate + last_out[j], 1);
                if(output < 0)
                    output += 1;
                out1[j*n + i] = output;
                out2[j*n + i] = fabs(output - last_out[j]) > 0.5;
            }
            last_in[j] = input;
            last_out[j] = output;
            last_rate[j] = rate;
        }
    }
    x->x_last_in = last_in;
    x->x_last_out = last_out;
    x->x_last_rate = last_rate;
    return(w+6);
}

static void pimpmul_dsp(t_pimpmul *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    int chs = x->x_ch1 = sp[0]->s_nchans;
    x->x_ch2 = sp[1]->s_nchans;
    if(x->x_ch2 > chs)
        chs = x->x_ch2;
    if(x->x_nchs != chs){
        x->x_last_in = (double *)resizebytes(x->x_last_in,
            x->x_nchs * sizeof(double), chs * sizeof(double));
        x->x_last_out = (double *)resizebytes(x->x_last_out,
            x->x_nchs * sizeof(double), chs * sizeof(double));
        x->x_last_rate = (double *)resizebytes(x->x_last_rate,
            x->x_nchs * sizeof(double), chs * sizeof(double));
        x->x_nchs = chs;
    }
    signal_setmultiout(&sp[2], x->x_nchs);
    signal_setmultiout(&sp[3], x->x_nchs);
    if((x->x_ch1 > 1 && x->x_ch1 != x->x_nchs)
    || (x->x_ch2 > 1 && x->x_ch2 != x->x_nchs)){
        dsp_add_zero(sp[2]->s_vec, x->x_nchs*x->x_n);
        dsp_add_zero(sp[3]->s_vec, x->x_nchs*x->x_n);
        pd_error(x, "[pimpmul~]: channel sizes mismatch");
        return;
    }
    dsp_add(pimpmul_perform, 5, x, sp[0]->s_vec,
        sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *pimpmul_free(t_pimpmul *x){
    inlet_free(x->x_inlet_rate);
    freebytes(x->x_last_in, x->x_nchs * sizeof(*x->x_last_in));
    freebytes(x->x_last_out, x->x_nchs * sizeof(*x->x_last_out));
    freebytes(x->x_last_rate, x->x_nchs * sizeof(*x->x_last_rate));
    return(void *)x;
}

static void *pimpmul_new(t_floatarg f){
    t_pimpmul *x = (t_pimpmul *)pd_new(pimpmul_class);
    x->x_nchs = 1;
    x->x_last_in = (double *)getbytes(sizeof(*x->x_last_in));
    x->x_last_out = (double *)getbytes(sizeof(*x->x_last_out));
    x->x_last_rate = (double *)getbytes(sizeof(*x->x_last_rate));
    x->x_last_in[0] = x->x_last_out[0] = x->x_last_rate[0] = 0;
    x->x_inlet_rate = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_rate, f);
    outlet_new((t_object *)x, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    return(x);
}

void pimpmul_tilde_setup(void){
    pimpmul_class = class_new(gensym("pimpmul~"), (t_newmethod)(void *)pimpmul_new,
        (t_method)pimpmul_free, sizeof(t_pimpmul), CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
    class_addmethod(pimpmul_class, nullfn, gensym("signal"), 0);
    class_addmethod(pimpmul_class, (t_method)pimpmul_dsp, gensym("dsp"), A_CANT, 0);
}
