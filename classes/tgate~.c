// Porres 2016

#include "m_pd.h"
#include "math.h"

static t_class *tgate_class;

typedef struct _tgate
{
    t_object  x_obj;
    float     x_sr;
    float     x_sr_khz;
    float     x_sum;
    float     x_lastin;
    t_inlet   *x_inlet_ms;
    t_outlet  *x_outlet;
} t_tgate;

static void tgate_bang(t_tgate *x){
    x->x_sum = 0;
}

static t_int *tgate_perform(t_int *w)
{
    t_tgate *x = (t_tgate *)(w[1]);
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
        if (trig > 0 && lastin <= 0)
            sum = 0;
        *out++ = (sum += 1) <= samps;
        lastin = trig;
    }
    x->x_sum = sum; // next
    x->x_lastin = lastin;
    return (w + 6);
}

static void tgate_dsp(t_tgate *x, t_signal **sp)
{
    int sr = sp[0]->s_sr;
    if(sr != x->x_sr){
        x->x_sr = sr;
        x->x_sr_khz = sr * 0.001;
        }
    dsp_add(tgate_perform, 5, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *tgate_free(t_tgate *x)
{
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *tgate_new(t_floatarg f)
{
    t_tgate *x = (t_tgate *)pd_new(tgate_class);
    x->x_sr = sys_getsr();
    x->x_sr_khz = x->x_sr * 0.001;
    float ms = f < 0 ? 0 : f;
    x->x_sum = 0;
    x->x_inlet_ms = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_ms, ms);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return (x);
}

void tgate_tilde_setup(void)
{
    tgate_class = class_new(gensym("tgate~"),
        (t_newmethod)tgate_new, (t_method)tgate_free,
        sizeof(t_tgate), CLASS_DEFAULT, A_DEFFLOAT, 0);
    class_addmethod(tgate_class, nullfn, gensym("signal"), 0);
    class_addmethod(tgate_class, (t_method) tgate_dsp, gensym("dsp"), 0);
    class_addbang(tgate_class,(t_method)tgate_bang);
}
