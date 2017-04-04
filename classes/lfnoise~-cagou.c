// Porres 2017

#include "m_pd.h"
#include "math.h"

static t_class *lfnoise_class;

typedef struct _lfnoise
{
    t_object x_obj;
    int x_val;
    t_float  x_freq;
    double  x_phase;
    t_float  x_yn;
    t_float  x_ynm1;
    t_float  x_interp;
    t_outlet *x_outlet;
    float    x_sr;
} t_lfnoise;

static void lfnoise_interp(t_lfnoise *x, t_floatarg f)
{
    x->x_interp = f != 0;
}

static t_int *lfnoise_perform(t_int *w)
{
    t_lfnoise *x = (t_lfnoise *)(w[1]);
    int nblock = (t_int)(w[2]);
    int *vp = (int *)(w[3]);
    t_float *in = (t_float *)(w[4]);
    t_sample *out = (t_sample *)(w[5]);
    int val = *vp;
    double phase = x->x_phase;
    t_float yn = x->x_yn;
    t_float ynm1 = x->x_ynm1;
    t_float interp = x->x_interp;
    double sr = x->x_sr;
    while (nblock--)
        {
        float hz = *in++;
        double phase_step = hz / sr;
        phase_step = phase_step > 1 ? 1. : phase_step < -1 ? -1 : phase_step; // clipped step
        t_float noise = ((float)((val & 0x7fffffff) - 0x40000000)) * (float)(1.0 / 0x40000000);
        if (hz >= 0)
            {
            if (phase >= 1.) // update
                {
                phase = phase - 1;
                ynm1 = yn;
                yn = noise;
                }
            }
        else 
            {
            if (phase <= 0.) // update
                {
                phase = phase + 1;
                ynm1 = yn;
                yn = noise;
                }
            }
            if (interp)
                {
                if (hz >= 0)
                    *out++ = ynm1 + (yn - ynm1) * (phase);
                else
                    *out++ = ynm1 + (yn - ynm1) * (1 - phase);
                }
        else
            *out++ = yn;
            
        phase += phase_step;
        }
    x->x_phase = phase;
    x->x_yn = yn;
    x->x_ynm1 = ynm1;
    return (w + 6);
}

static void lfnoise_dsp(t_lfnoise *x, t_signal **sp)
{
    x->x_sr = sp[0]->s_sr;
    dsp_add(lfnoise_perform, 5, x, sp[0]->s_n, &x->x_val, sp[0]->s_vec, sp[1]->s_vec);
}

static void *lfnoise_free(t_lfnoise *x)
{
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *lfnoise_new(t_floatarg f1, t_floatarg f2)
{
    t_lfnoise *x = (t_lfnoise *)pd_new(lfnoise_class);
    if (f1 >= 0) x->x_phase = 1;
    x->x_freq  = f1;
    x->x_interp = (f2 != 0);
    static int init_val = 307;
    init_val *= 1319;
    x->x_yn = (((float)((init_val & 0x7fffffff) - 0x40000000)) * (float)(1.0 / 0x40000000));
    x->x_val = init_val * 435898247 + 382842987;
    outlet_new((t_object *)x, &s_signal);
    return (x);
}

void lfnoise_tilde_setup(void)
{
    lfnoise_class = class_new(gensym("lfnoise~"),
        (t_newmethod)lfnoise_new,
        (t_method)lfnoise_free,
        sizeof(t_lfnoise),
        CLASS_DEFAULT,
        A_DEFFLOAT, A_DEFFLOAT,
        0);
    CLASS_MAINSIGNALIN(lfnoise_class, t_lfnoise, x_freq);
    class_addmethod(lfnoise_class, (t_method)lfnoise_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(lfnoise_class, (t_method)lfnoise_interp, gensym("interp"), A_DEFFLOAT, 0);
}
