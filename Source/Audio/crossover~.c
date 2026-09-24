// Porres 2016-2025

#define _USE_MATH_DEFINES

#include "m_pd.h"
#include <math.h>

#define TWOPI (3.14159265358979323846 * 2.)
#define SQRT_HALF sqrt(0.5)

typedef struct _cross{
    t_object    x_obj;
    t_inlet    *x_inlet_freq;
    t_outlet   *x_out1;
    t_outlet   *x_out2;
    t_float     x_nyq;
    t_float     x_coeff;
    t_float    *x_L1x1;
    t_float    *x_L1x2;
    t_float    *x_L1y1;
    t_float    *x_L1y2;
    t_float    *x_L2x1;
    t_float    *x_L2x2;
    t_float    *x_L2y1;
    t_float    *x_L2y2;
    t_float    *x_H1x1;
    t_float    *x_H1x2;
    t_float    *x_H1y1;
    t_float    *x_H1y2;
    t_float    *x_H2x1;
    t_float    *x_H2x2;
    t_float    *x_H2y1;
    t_float    *x_H2y2;
    int         x_n;
    int         x_nchans;
    int         x_ch2;
}t_cross;

static t_class *cross_class;

static void cross_clear(t_cross *x){
    for(int j = 0; j < x->x_nchans; j++){
        x->x_L1x1[j] = x->x_L1x2[j] = x->x_L1y1[j] = x->x_L1y2[j] = 0.;
        x->x_L2x1[j] = x->x_L2x2[j] = x->x_L2y1[j] = x->x_L2y2[j] = 0.;
        x->x_H1x1[j] = x->x_H1x2[j] = x->x_H1y1[j] = x->x_H1y2[j] = 0.;
        x->x_H2x1[j] = x->x_H2x2[j] = x->x_H2y1[j] = x->x_H2y2[j] = 0.;
    }
}

static t_int *cross_perform(t_int *w){
    t_cross *x = (t_cross *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *out1 = (t_float *)(w[4]);
    t_float *out2 = (t_float *)(w[5]);
    for(int j = 0; j < x->x_nchans; j++){
        t_float L1x1 = x->x_L1x1[j];
        t_float L1x2 = x->x_L1x2[j];
        t_float L1y1 = x->x_L1y1[j];
        t_float L1y2 = x->x_L1y2[j];
        t_float L2x1 = x->x_L2x1[j];
        t_float L2x2 = x->x_L2x2[j];
        t_float L2y1 = x->x_L2y1[j];
        t_float L2y2 = x->x_L2y2[j];
        t_float H1x1 = x->x_H1x1[j];
        t_float H1x2 = x->x_H1x2[j];
        t_float H1y1 = x->x_H1y1[j];
        t_float H1y2 = x->x_H1y2[j];
        t_float H2x1 = x->x_H2x1[j];
        t_float H2x2 = x->x_H2x2[j];
        t_float H2y1 = x->x_H2y1[j];
        t_float H2y2 = x->x_H2y2[j];
        for(int i = 0; i < x->x_n; i++){
            float L1xn, H1xn;
            L1xn = H1xn = in1[j*x->x_n+i];
            float f = x->x_ch2 == 1 ? in2[i] : in2[j*x->x_n+i];
            if(f < 1)
                f = 1;
            if(f > x->x_nyq - 1)
                f = x->x_nyq - 1;
            float rad = f * x->x_coeff;
            float cosF = cos(rad);
            float sinF = sin(rad) * SQRT_HALF;
            float sinFp1 = sinF + 1;
            float sinFm1 = sinF - 1;
// coefs LOWPASS:
            float ff1 = (1-cosF) / (2*sinFp1);
            float ff2 = (1-cosF) / sinFp1;
            float ff3 = ff1;
            float fb1 = (2*cosF) / sinFp1;
            float fb2 = sinFm1 / sinFp1;
// Lowpass 2nd order
            float L1yn = ff1*L1xn + ff2*L1x1 + ff3*L1x2 + fb1*L1y1 + fb2*L1y2;
            L1x2 = L1x1;
            L1x1 = L1xn;
            L1y2 = L1y1;
            L1y1 = L1yn;
// Lowpass 4th order
            float L2xn = L1yn;
            float L2yn = ff1*L2xn + ff2*L2x1 + ff3*L2x2 + fb1*L2y1 + fb2*L2y2;
            L2x2 = L2x1;
            L2x1 = L2xn;
            L2y2 = L2y1;
            L2y1 = L2yn;
            float lowpass = L2yn;
// coefs HIGHPASS:
            ff1 = (1+cosF) / (2*sinFp1);
            ff2 = (-cosF-1) / sinFp1;
            ff3 = ff1;
// Highpass 2nd order
            float H1yn = ff1*H1xn + ff2*H1x1 + ff3*H1x2 + fb1*H1y1 + fb2*H1y2;
            H1x2 = H1x1;
            H1x1 = H1xn;
            H1y2 = H1y1;
            H1y1 = H1yn;
// Highpass 4th order
            float H2xn = H1yn;
            float H2yn = ff1*H2xn + ff2*H2x1 + ff3*H2x2 + fb1*H2y1 + fb2*H2y2;
            H2x2 = H2x1;
            H2x1 = H2xn;
            H2y2 = H2y1;
            H2y1 = H2yn;
            float highpass = H2yn;
            out1[j*x->x_n+i] = lowpass;
            out2[j*x->x_n+i] = highpass;
        }
        x->x_L1x1[j] = L1x1;
        x->x_L1x2[j] = L1x2;
        x->x_L1y1[j] = L1y1;
        x->x_L1y2[j] = L1y2;
        x->x_L2x1[j] = L2x1;
        x->x_L2x2[j] = L2x2;
        x->x_L2y1[j] = L2y1;
        x->x_L2y2[j] = L2y2;
        x->x_H1x1[j] = H1x1;
        x->x_H1x2[j] = H1x2;
        x->x_H1y1[j] = H1y1;
        x->x_H1y2[j] = H1y2;
        x->x_H2x1[j] = H2x1;
        x->x_H2x2[j] = H2x2;
        x->x_H2y1[j] = H2y1;
        x->x_H2y2[j] = H2y2;
    }
    return(w+6);
}

static void cross_dsp(t_cross *x, t_signal **sp){
    x->x_nyq = sp[0]->s_sr * 0.5;
    x->x_coeff = TWOPI / sp[0]->s_sr;
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    x->x_ch2 = sp[1]->s_nchans;
    if(x->x_nchans != chs){
        x->x_L1x1 = (t_float *)resizebytes(x->x_L1x1, x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_L1x2 = (t_float *)resizebytes(x->x_L1x2, x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_L1y1 = (t_float *)resizebytes(x->x_L1y1, x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_L1y2 = (t_float *)resizebytes(x->x_L1y2, x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_L2x1 = (t_float *)resizebytes(x->x_L2x1, x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_L2x2 = (t_float *)resizebytes(x->x_L2x2, x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_L2y1 = (t_float *)resizebytes(x->x_L2y1, x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_L2y2 = (t_float *)resizebytes(x->x_L2y2, x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_H1x1 = (t_float *)resizebytes(x->x_H1x1, x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_H1x2 = (t_float *)resizebytes(x->x_H1x2, x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_H1y1 = (t_float *)resizebytes(x->x_H1y1, x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_H1y2 = (t_float *)resizebytes(x->x_H1y2, x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_H2x1 = (t_float *)resizebytes(x->x_H2x1, x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_H2x2 = (t_float *)resizebytes(x->x_H2x2, x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_H2y1 = (t_float *)resizebytes(x->x_H2y1, x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_H2y2 = (t_float *)resizebytes(x->x_H2y2, x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        for(int j = x->x_nchans; j < chs; j++){
            x->x_L1x1[j] = x->x_L1x2[j] = x->x_L1y1[j] = x->x_L1y2[j] = 0.;
            x->x_L2x1[j] = x->x_L2x2[j] = x->x_L2y1[j] = x->x_L2y2[j] = 0.;
            x->x_H1x1[j] = x->x_H1x2[j] = x->x_H1y1[j] = x->x_H1y2[j] = 0.;
            x->x_H2x1[j] = x->x_H2x2[j] = x->x_H2y1[j] = x->x_H2y2[j] = 0.;
        }
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[2], x->x_nchans);
    signal_setmultiout(&sp[3], x->x_nchans);
    if(x->x_ch2 > 1 && x->x_ch2 != x->x_nchans){
        dsp_add_zero(sp[2]->s_vec, x->x_nchans*x->x_n);
        dsp_add_zero(sp[3]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[crossover~]: channel sizes mismatch");
        return;
    }
    dsp_add(cross_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec);
}

static void *cross_free(t_cross *x){
    freebytes(x->x_L1x1, x->x_nchans * sizeof(t_float));
    freebytes(x->x_L1x2, x->x_nchans * sizeof(t_float));
    freebytes(x->x_L1y1, x->x_nchans * sizeof(t_float));
    freebytes(x->x_L1y2, x->x_nchans * sizeof(t_float));
    freebytes(x->x_L2x1, x->x_nchans * sizeof(t_float));
    freebytes(x->x_L2x2, x->x_nchans * sizeof(t_float));
    freebytes(x->x_L2y1, x->x_nchans * sizeof(t_float));
    freebytes(x->x_L2y2, x->x_nchans * sizeof(t_float));
    freebytes(x->x_H1x1, x->x_nchans * sizeof(t_float));
    freebytes(x->x_H1x2, x->x_nchans * sizeof(t_float));
    freebytes(x->x_H1y1, x->x_nchans * sizeof(t_float));
    freebytes(x->x_H1y2, x->x_nchans * sizeof(t_float));
    freebytes(x->x_H2x1, x->x_nchans * sizeof(t_float));
    freebytes(x->x_H2x2, x->x_nchans * sizeof(t_float));
    freebytes(x->x_H2y1, x->x_nchans * sizeof(t_float));
    freebytes(x->x_H2y2, x->x_nchans * sizeof(t_float));
    inlet_free(x->x_inlet_freq);
    outlet_free(x->x_out1);
    outlet_free(x->x_out2);
    return(void *)x;
}

static void *cross_new(t_floatarg f){
    t_cross *x = (t_cross *)pd_new(cross_class);
    x->x_nchans = 1;
    x->x_L1x1 = (t_float *)getbytes(sizeof(t_float));
    x->x_L1x2 = (t_float *)getbytes(sizeof(t_float));
    x->x_L1y1 = (t_float *)getbytes(sizeof(t_float));
    x->x_L1y2 = (t_float *)getbytes(sizeof(t_float));
    x->x_L2x1 = (t_float *)getbytes(sizeof(t_float));
    x->x_L2x2 = (t_float *)getbytes(sizeof(t_float));
    x->x_L2y1 = (t_float *)getbytes(sizeof(t_float));
    x->x_L2y2 = (t_float *)getbytes(sizeof(t_float));
    x->x_H1x1 = (t_float *)getbytes(sizeof(t_float));
    x->x_H1x2 = (t_float *)getbytes(sizeof(t_float));
    x->x_H1y1 = (t_float *)getbytes(sizeof(t_float));
    x->x_H1y2 = (t_float *)getbytes(sizeof(t_float));
    x->x_H2x1 = (t_float *)getbytes(sizeof(t_float));
    x->x_H2x2 = (t_float *)getbytes(sizeof(t_float));
    x->x_H2y1 = (t_float *)getbytes(sizeof(t_float));
    x->x_H2y2 = (t_float *)getbytes(sizeof(t_float));
    x->x_inlet_freq = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_freq, f);
    x->x_out1 = outlet_new((t_object *)x, &s_signal);
    x->x_out2 = outlet_new((t_object *)x, &s_signal);
    cross_clear(x);
    return(x);
}

void crossover_tilde_setup(void){
    cross_class = class_new(gensym("crossover~"), (t_newmethod)cross_new,
        (t_method)cross_free, sizeof(t_cross), CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
    class_addmethod(cross_class, (t_method)cross_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(cross_class, nullfn, gensym("signal"), 0);
    class_addmethod(cross_class, (t_method)cross_clear, gensym("clear"), 0);
}