// porres 2023

#include <math.h>
#include "m_pd.h"

#define HALF_PI (3.14159265358979323846 * 0.5)

static t_class *xfademc_class;

typedef struct _xfademc{
    t_object    x_obj;
    t_inlet    *x_inlet_mix;
    int         x_block;
    int         x_chs;
}t_xfademc;

static t_int *xfademc_perform(t_int *w){
    t_xfademc *x = (t_xfademc *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *pos_in = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    int n =  x->x_block, chs =  x->x_chs;
    for(int i = 0; i < x->x_block; i++){
        float pos = pos_in[i]; // xfade value [-1: left (A) input, 1: right (B) input]
        if(pos > 1)
            pos = 1;
        if(pos < -1)
            pos = -1;
        pos = (pos + 1) * 0.5; // xfade from 0 to 1
        for(int j = 0; j < chs; j++){
            double amp1 = cos((double)pos * HALF_PI);
            double amp2 = sin((double)pos * HALF_PI);
            out[j*n + i] = (in1[j*n + i] * amp1) + (in2[j*n + i] * amp2);
        }
    }
    return(w+6);
}

static void xfademc_dsp(t_xfademc *x, t_signal **sp){
    int n = sp[0]->s_n, chs1 = sp[0]->s_nchans, chs2 = sp[1]->s_nchans;
    signal_setmultiout(&sp[3], chs1);
    x->x_block = n, x->x_chs = chs1;
    if(chs1 != chs2){
        dsp_add_zero(sp[3]->s_vec, chs1*n);
        pd_error(x, "[xfade.mc~]: channel sizes mismatch");
    }
    else
        dsp_add(xfademc_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void xfademc_free(t_xfademc *x){
     inlet_free(x->x_inlet_mix);
}

static void *xfademc_new(t_floatarg f){
    t_xfademc *x = (t_xfademc *)pd_new(xfademc_class);
    float init_mix = f < -1 ? -1 : f >= 1 ? 1 : f; // clipping mix input
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    x->x_inlet_mix = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_mix, init_mix);
    outlet_new(&x->x_obj, &s_signal);
    return(void *)x;
}

void setup_xfade0x2emc_tilde(void){
    xfademc_class = class_new(gensym("xfade.mc~"), (t_newmethod)xfademc_new,
        (t_method)xfademc_free, sizeof(t_xfademc), CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
    class_addmethod(xfademc_class, nullfn, gensym("signal"), 0);
    class_addmethod(xfademc_class, (t_method)xfademc_dsp, gensym("dsp"), A_CANT, 0);
}
