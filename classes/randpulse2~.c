// Porres 2018

#include "m_pd.h"

static t_class *randpulse2_class;

typedef struct _randpulse2
{
    t_object   x_obj;
    t_float    x_freq;
    int        x_val;
    double     x_phase;
    t_float    x_ynp1;
    t_float    x_yn;
    t_outlet  *x_outlet;
    float      x_sr;
} t_randpulse2;

static void randpulse2_seed(t_randpulse2 *x, t_floatarg f)
{
    int val = (int)f * 1319;
    x->x_ynp1 = ((float)((val & 0x7fffffff) - 0x40000000)) * (float)(1.0 / 0x40000000);
    val = val * 435898247 + 382842987;
    x->x_val = val;
}

static t_int *randpulse2_perform(t_int *w)
{
    t_randpulse2 *x = (t_randpulse2 *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *out = (t_sample *)(w[4]);
    int val = x->x_val;
    double phase = x->x_phase;
    t_float ynp1 = x->x_ynp1;
    t_float yn = x->x_yn;
    double sr = x->x_sr;
    while (nblock--)
        {
        float hz = *in1++;
        double phase_step = hz / sr;
// clipped phase_step
        phase_step = phase_step > 1 ? 1. : phase_step < -1 ? -1 : phase_step;
        t_float random;
        if (hz >= 0)
            {
            if (phase >= 1.) // update
                {
                random = ((float)((val & 0x7fffffff) - 0x40000000)) * (float)(1.0 / 0x40000000);
                val = val * 435898247 + 382842987;
                phase = phase - 1;
                yn = ynp1;
                ynp1 = random; // next random value
                }
            *out++ = (yn + (ynp1 - yn) * (phase)) > 0;
            }
        else 
            {
            if (phase <= 0.) // update
                {
                random = ((float)((val & 0x7fffffff) - 0x40000000)) * (float)(1.0 / 0x40000000);
                val = val * 435898247 + 382842987;
                phase = phase + 1;
                yn = ynp1;
                ynp1 = random; // next random value
                }
            *out++ = (yn + (ynp1 - yn) * (1 - phase)) > 0;
            }
        phase += phase_step;
        }
    x->x_val = val;
    x->x_phase = phase;
    x->x_ynp1 = ynp1; // next random value
    x->x_yn = yn; // current output
    return (w + 5);
}

static void randpulse2_dsp(t_randpulse2 *x, t_signal **sp)
{
    x->x_sr = sp[0]->s_sr;
    dsp_add(randpulse2_perform, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
}

static void *randpulse2_free(t_randpulse2 *x)
{
    outlet_free(x->x_outlet);
    return (void *)x;
}


static void *randpulse2_new(t_floatarg f)
{
    t_randpulse2 *x = (t_randpulse2 *)pd_new(randpulse2_class);
// default seed
    static int seed = 234599;
    seed *= 1319;
    if (f >= 0)
        x->x_phase = 1.;
    x->x_freq = f;
// get 1st output
    x->x_ynp1 = (((float)((seed & 0x7fffffff) - 0x40000000)) * (float)(1.0 / 0x40000000));
    x->x_val = seed * 435898247 + 382842987;
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return (x);
}

void randpulse2_tilde_setup(void)
{
    randpulse2_class = class_new(gensym("randpulse2~"), (t_newmethod)randpulse2_new,
        (t_method)randpulse2_free, sizeof(t_randpulse2), CLASS_DEFAULT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(randpulse2_class, t_randpulse2, x_freq);
    class_addmethod(randpulse2_class, (t_method)randpulse2_dsp, gensym("dsp"), A_CANT, 0);
}
