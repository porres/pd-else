// Porres 2016

#include "m_pd.h"
#include "math.h"

static t_class *pluck_class;

typedef struct _pluck
{
    t_object  x_obj;
    float     x_sr;
    float     x_sum;
    float     x_last_trig;
    t_inlet   *x_inlet_hz;
    t_outlet  *x_outlet;
} t_pluck;

static t_int *pluck_perform(t_int *w)
{
    t_pluck *x = (t_pluck *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_float *triglet = (t_float *)(w[4]);
    t_float *hz_in = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);
    t_float last_trig = x->x_last_trig;
    t_float sr = x->x_sr;
    t_float sum = x->x_sum;
    while (nblock--){
        t_float input = *in++;
        t_float trig = *triglet++;
        t_float hz = *hz_in++;
        t_int samps = (int)roundf(sr/hz);
        if (trig > 0 && last_trig <= 0)
            sum = 0;
        *out++ = input * ((sum += 1) <= samps);
        last_trig = trig;
    }
    x->x_sum = sum; // next
    x->x_last_trig = last_trig;
    return (w + 7);
}

static void pluck_dsp(t_pluck *x, t_signal **sp)
{
    int sr = sp[0]->s_sr;
    if(sr != x->x_sr){
        x->x_sr = sr;
        }
    dsp_add(pluck_perform, 6, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *pluck_free(t_pluck *x)
{
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *pluck_new(t_floatarg f)
{
    t_pluck *x = (t_pluck *)pd_new(pluck_class);
    x->x_sr = sys_getsr();
    float hz = f < 0 ? 0 : f;
    x->x_sum = 0;
    inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_inlet_hz = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_hz, hz);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return (x);
}

void pluck_tilde_setup(void)
{
    pluck_class = class_new(gensym("pluck~"),
        (t_newmethod)pluck_new, (t_method)pluck_free,
        sizeof(t_pluck), CLASS_DEFAULT, A_DEFFLOAT, 0);
    class_addmethod(pluck_class, nullfn, gensym("signal"), 0);
    class_addmethod(pluck_class, (t_method) pluck_dsp, gensym("dsp"), 0);
}
