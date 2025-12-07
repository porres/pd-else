// Porres 2017 - 2025

#include "m_pd.h"
#include <stdlib.h>

static t_class *above_class;

typedef struct _above{
    t_object x_obj;
    t_inlet  *x_thresh_inlet;
    t_float  x_in;
    t_float  *x_lastin;
    int       x_nchs;
    t_int     x_n;
    t_int     x_ch1;
    t_int     x_ch2;
}t_above;

static t_int *above_perform(t_int *w){
    t_above *x = (t_above *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *out1 = (t_float *)(w[4]);
    t_float *out2 = (t_float *)(w[5]);
    int n = x->x_n;
    t_float *lastin = x->x_lastin;
    for(int j = 0; j < x->x_nchs; j++){
        for(int i = 0; i < n; i++){
            float input;
            if(x->x_ch1 == 1)
                input = in1[i];
            else
                input = in1[j*n+i];
            float threshold;
            if(x->x_ch2 == 1)
                threshold = in2[i];
            else
                threshold = in2[j*n+i];
            out1[j*n+i] = input > threshold && lastin[j] <= threshold;
            out2[j*n+i] = input <= threshold && lastin[j] > threshold;
            lastin[j] = input;
        }
    }
    x->x_lastin = lastin;
    return(w+6);
}

static void above_dsp(t_above *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    int chs = x->x_ch1 = sp[0]->s_nchans;
    x->x_ch2 = sp[1]->s_nchans;
    if(x->x_ch2 > chs)
        chs = x->x_ch2;
    if(x->x_nchs != chs){
        x->x_lastin = (t_float *)resizebytes(x->x_lastin,
            x->x_nchs * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchs = chs;
    }
    signal_setmultiout(&sp[2], x->x_nchs);
    signal_setmultiout(&sp[3], x->x_nchs);
    if((x->x_ch1 > 1 && x->x_ch1 != x->x_nchs)
    || (x->x_ch2 > 1 && x->x_ch2 != x->x_nchs)){
        dsp_add_zero(sp[4]->s_vec, x->x_nchs*x->x_n);
        dsp_add_zero(sp[5]->s_vec, x->x_nchs*x->x_n);
        pd_error(x, "[above~]: channel sizes mismatch");
        return;
    }
    dsp_add(above_perform, 5, x, sp[0]->s_vec,
        sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *above_free(t_above *x){
    inlet_free(x->x_thresh_inlet);
    freebytes(x->x_lastin, x->x_nchs * sizeof(*x->x_lastin));
    return(void*)x;
}

static void *above_new(t_floatarg f){
    t_above *x = (t_above *)pd_new(above_class);
    x->x_lastin = (t_float*)malloc(sizeof(t_float));
    x->x_lastin[0] = 0;
    x->x_thresh_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_thresh_inlet, f);
    outlet_new((t_object *)x, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    return(x);
}

void above_tilde_setup(void){
    above_class = class_new(gensym("above~"), (t_newmethod)(void*)above_new, (t_method)above_free, sizeof(t_above), CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(above_class, t_above, x_in);
    class_addmethod(above_class, (t_method)above_dsp, gensym("dsp"), A_CANT, 0);
}
