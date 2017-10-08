// Porres 2016

#include "m_pd.h"
#include "math.h"

static t_class *tgate2_class;

typedef struct _tgate2
{
    t_object  x_obj;
    float     x_sr;
    float     x_sr_khz;
    float     x_sum;
    float     x_lastin;
    float     x_gate_value;
    t_inlet   *x_inlet_ms;
    t_outlet  *x_outlet;
} t_tgate2;

static void tgate2_float(t_tgate2 *x, t_float f)
{
    x->x_sum = 0;
    x->x_gate_value = f;
}

static t_int *tgate2_perform(t_int *w)
{
    t_tgate2 *x = (t_tgate2 *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_float *ms_in = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    t_float lastin = x->x_lastin;
    t_float sr_khz = x->x_sr_khz;
    t_float sum = x->x_sum;
    while (nblock--)
    {
        t_float trig = *in++;
        t_float ms = *ms_in++;
        t_int samps = (int)roundf(ms * sr_khz);
        t_float gate_value;
        if (trig != 0 && lastin == 0){
            sum = 0;
            x->x_gate_value = trig;
        }
        t_float gate = ((sum += 1) <= samps);
        *out++ =  gate ? gate * x->x_gate_value : 0;
        lastin = trig;
    }
    x->x_sum = sum; // next
    x->x_lastin = lastin;
    return (w + 6);
}

static void tgate2_dsp(t_tgate2 *x, t_signal **sp)
{
    int sr = sp[0]->s_sr;
    if(sr != x->x_sr){
        x->x_sr = sr;
        x->x_sr_khz = sr * 0.001;
        }
    dsp_add(tgate2_perform, 5, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *tgate2_free(t_tgate2 *x)
{
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *tgate2_new(t_floatarg f)
{
    t_tgate2 *x = (t_tgate2 *)pd_new(tgate2_class);
    x->x_sr = sys_getsr();
    x->x_sr_khz = x->x_sr * 0.001;
    float ms = f < 0 ? 0 : f;
    x->x_sum = 0;
    x->x_inlet_ms = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_ms, ms);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return (x);
}

void tgate2_tilde_setup(void)
{
    tgate2_class = class_new(gensym("tgate2~"),
        (t_newmethod)tgate2_new, (t_method)tgate2_free,
        sizeof(t_tgate2), CLASS_DEFAULT, A_DEFFLOAT, 0);
    class_addmethod(tgate2_class, nullfn, gensym("signal"), 0);
    class_addmethod(tgate2_class, (t_method) tgate2_dsp, gensym("dsp"), 0);
    class_addfloat(tgate2_class, (t_method)tgate2_float);
}
