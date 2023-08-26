// porres 2017-2020

#include "m_pd.h"
#include <stdlib.h>
#include <math.h>

static t_class *xgatemc_class;

#define MAXOUTS 512
#define HALF_PI (3.14159265358979323846 * 0.5)

typedef struct _xgatemc{
    t_object    x_obj;
    int       x_lastout;
    int       x_n_outs;
    double    x_n_fade; // fade length in samples
    float     x_sr_khz;
    int       x_active_out[MAXOUTS];
    int       x_count[MAXOUTS];
}t_xgatemc;

void xgatemc_float(t_xgatemc *x, t_floatarg f){
    int out = f < 0 ? 0 : f > x->x_n_outs ? x->x_n_outs : (int)f;
    if(x->x_lastout != out){
        if(out)
            x->x_active_out[out - 1] = 1;
        if(x->x_lastout)
            x->x_active_out[x->x_lastout - 1] = 0;
        x->x_lastout = out;
    }
}

static void xgatemc_time(t_xgatemc *x, t_floatarg f){
    double last_fade_n = x->x_n_fade;
    x->x_n_fade = (x->x_sr_khz * (f < 0 ? 0 : f)) + 1;
    for(int n = 0; n < x->x_n_outs; n++)
        if(x->x_count[n]) // adjust counters
            x->x_count[n] = (x->x_count[n] / last_fade_n) * x->x_n_fade;
}

static t_int *xgatemc_perform(t_int *w){
    t_xgatemc *x = (t_xgatemc *)(w[1]);
    int n = (int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    for(int i = 0; i < n; i++){
        float input = in[i];
        for(int j = 0; j < x->x_n_outs; j++){
            if(x->x_active_out[j] && x->x_count[j] < x->x_n_fade)
                x->x_count[j]++;
            else if(!x->x_active_out[j] && x->x_count[j] > 0)
                x->x_count[j]--;
            out[j*n + i] = input * sin((x->x_count[j] / x->x_n_fade) * HALF_PI);
        }
    }
    return(w+5);
}

static void xgatemc_dsp(t_xgatemc *x, t_signal **sp){
    signal_setmultiout(&sp[1], x->x_n_outs);
    dsp_add(xgatemc_perform, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
}

static void *xgatemc_new(t_floatarg f1, t_floatarg f2, t_floatarg f3){
    t_xgatemc *x = (t_xgatemc *)pd_new(xgatemc_class);
    for(int n = 0; n < MAXOUTS; n++){
        x->x_active_out[n] = 0;
        x->x_count[n] = 0;
    }
    t_float out = f1, ms = f2, init_out = f3;
    x->x_n_outs = out < 1 ? 1 : out > MAXOUTS ? MAXOUTS : (int)out;
    x->x_sr_khz = sys_getsr() * 0.001;
    x->x_n_fade = x->x_sr_khz * (ms > 0 ? ms : 0) + 1;
    x->x_lastout = 0;
    outlet_new(&x->x_obj, gensym("signal"));
    xgatemc_float(x, init_out);
    return(x);
}

void setup_xgate0x2emc_tilde(void) {
    xgatemc_class = class_new(gensym("xgate.mc~"), (t_newmethod)xgatemc_new, 0,
        sizeof(t_xgatemc), CLASS_MULTICHANNEL, A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addfloat(xgatemc_class, (t_method)xgatemc_float);
    class_addmethod(xgatemc_class, nullfn, gensym("signal"), 0);
    class_addmethod(xgatemc_class, (t_method)xgatemc_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(xgatemc_class, (t_method)xgatemc_time, gensym("time"), A_FLOAT, 0);
}
