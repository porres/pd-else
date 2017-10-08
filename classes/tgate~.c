// Porres 2016

#include "m_pd.h"
#include "math.h"

static t_class *tgate_class;

typedef struct _tgate{
    t_object  x_obj;
    float     x_sr;
    float     x_sr_khz;
    float     x_sum;
    float     x_lastin;
    float     x_gate_value;
    int       x_retrigger;
    t_inlet   *x_inlet_ms;
    t_outlet  *x_outlet;
} t_tgate;

static void tgate_float(t_tgate *x, t_float f){
    x->x_sum = 0;
    x->x_gate_value = f;
}

static void tgate_retrigger(t_tgate *x, t_float f){
    x->x_retrigger = f != 0;
}

static void tgate_bang(t_tgate *x){
    x->x_sum = 0;
}

static t_int *tgate_perform(t_int *w){
    t_tgate *x = (t_tgate *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_float *ms_in = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    t_float lastin = x->x_lastin;
    t_float sr_khz = x->x_sr_khz;
    t_float sum = x->x_sum;
    while (nblock--){
        t_float trig = *in++;
        t_float ms = *ms_in++;
        t_int samps = (int)roundf(ms * sr_khz);
        t_float gate = ((sum += 1) <= samps);
        t_float gate_value;
        if (x->x_retrigger){
            if(trig != 0 && lastin == 0){
                sum = 0;
                x->x_gate_value = trig;
            }
        }
        else{
            if(trig != 0 && lastin == 0 && !gate){
                sum = 0;
                x->x_gate_value = trig;
            }
        }
        *out++ =  gate ? gate * x->x_gate_value : 0;
        lastin = trig;
    }
    x->x_sum = sum; // next
    x->x_lastin = lastin;
    return (w + 6);
}

static void tgate_dsp(t_tgate *x, t_signal **sp){
    int sr = sp[0]->s_sr;
    if(sr != x->x_sr){
        x->x_sr = sr;
        x->x_sr_khz = sr * 0.001;
        }
    dsp_add(tgate_perform, 5, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *tgate_free(t_tgate *x){
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *tgate_new(t_floatarg f1, t_floatarg f2, t_floatarg f3){
    t_tgate *x = (t_tgate *)pd_new(tgate_class);
    x->x_sr = sys_getsr();
    x->x_sr_khz = x->x_sr * 0.001;
    float ms = f1 < 0 ? 0 : f1;
    int time_in_samps;
    time_in_samps = (int)roundf(ms * x->x_sr_khz);
    x->x_sum = time_in_samps + 1;
    x->x_gate_value = f2 == 0 ? 1 : f2; // default gate value is 1
    x->x_retrigger = f3 != 0;
    x->x_inlet_ms = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_ms, ms);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return (x);
}

void tgate_tilde_setup(void){
    tgate_class = class_new(gensym("tgate~"),
        (t_newmethod)tgate_new, (t_method)tgate_free,
        sizeof(t_tgate), CLASS_DEFAULT, A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(tgate_class, nullfn, gensym("signal"), 0);
    class_addmethod(tgate_class, (t_method) tgate_dsp, gensym("dsp"), 0);
    class_addmethod(tgate_class, (t_method)tgate_retrigger, gensym("retrigger"), A_DEFFLOAT, 0);
    class_addfloat(tgate_class, (t_method)tgate_float);
    class_addbang(tgate_class, (t_method)tgate_bang);
}
