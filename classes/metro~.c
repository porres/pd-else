// Porres 2017

#include "m_pd.h"
#include "math.h"

static t_class *metro_class;

typedef struct _metro{
    t_object x_obj;
    double  x_phase;
    t_inlet  *x_inlet_bpm;
    t_outlet *x_outlet_dsp_0;
    t_float x_sr;
    t_float x_gate;
    t_float x_last_in;
    t_float x_mul;
} t_metro;

static void metro_mul(t_metro *x, t_floatarg f){
    x->x_mul = f;
}

static t_int *metro_perform(t_int *w){
    t_metro *x = (t_metro *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]); // gate
    t_float *in2 = (t_float *)(w[4]); // bpm
    t_float *out = (t_float *)(w[5]);
    float lastin = x->x_last_in;
    double phase = x->x_phase;
    double sr = x->x_sr;
    while (nblock--){
        float gate = *in1++;
        double bpm = *in2++;
        double hz;
        if (x->x_mul <= 0)
            hz = sr;
        else{
            if (bpm < 0)
                bpm = 0;
            hz = bpm / (60 * x->x_mul);
        }
        double phase_step = hz / sr; // phase_step
        if (phase_step > 1)
            phase_step = 1;
        if (gate != 0){
            if (lastin == 0)
                phase = 1;
            *out++ = phase >= 1.;
            if (phase >= 1.)
                phase = phase - 1; // wrapped phase
            phase += phase_step; // next phase
            }
        else
            *out++ = 0;
        lastin = gate;
    }
    x->x_phase = phase;
    x->x_last_in = lastin;
    return (w + 6);
}

static void metro_dsp(t_metro *x, t_signal **sp){
    x->x_sr = sp[0]->s_sr;
    dsp_add(metro_perform, 5, x, sp[0]->s_n,
        sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *metro_free(t_metro *x){
    inlet_free(x->x_inlet_bpm);
    outlet_free(x->x_outlet_dsp_0);
    return (void *)x;
}

static void *metro_new(t_floatarg f1, t_floatarg f2){
    t_metro *x = (t_metro *)pd_new(metro_class);
    t_float init_bpm = f1;
    if (init_bpm < 0)
        init_bpm = 0;
    t_float init_mul = f2;
    if (init_mul <= 0)
        init_mul = 1;
    x->x_mul = init_mul;
    x->x_sr = sys_getsr(); // sample rate
    x->x_phase = 1;
    x->x_inlet_bpm = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_bpm, init_bpm);
    x->x_outlet_dsp_0 = outlet_new(&x->x_obj, &s_signal);
    return (x);
}

void metro_tilde_setup(void){
    metro_class = class_new(gensym("metro~"),(t_newmethod)metro_new, (t_method)metro_free,
            sizeof(t_metro), CLASS_DEFAULT, A_DEFFLOAT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(metro_class, t_metro, x_gate);
    class_addmethod(metro_class, (t_method)metro_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(metro_class, (t_method)metro_mul, gensym("mul"), A_DEFFLOAT, 0);
}
