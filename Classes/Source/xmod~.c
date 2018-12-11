// Porres 2017

#include "m_pd.h"
#include "math.h"

#define TWOPI (M_PI * 2)

static t_class *xmod_class;

typedef struct _xmod
{
    t_object    x_obj;
    double      x_phase_1;
    double      x_phase_2;
    t_float     x_yn;
    t_float     x_freq;
    t_inlet    *x_inlet_fb;
    t_inlet    *x_inlet_freq2;
    t_inlet    *x_inlet_fb2;
    t_outlet   *x_outlet1;
    t_outlet   *x_outlet2;
    t_float     x_sr;
} t_xmod;

static t_int *xmod_perform(t_int *w)
{
    t_xmod *x = (t_xmod *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]); // freq1
    t_float *in2 = (t_float *)(w[4]); // index1
    t_float *in3 = (t_float *)(w[5]); // freq2
    t_float *in4 = (t_float *)(w[6]); // index2
    t_float *out1 = (t_float *)(w[7]);
    t_float *out2 = (t_float *)(w[8]);
    float yn = x->x_yn;
    double phase1 = x->x_phase_1;
    double phase2 = x->x_phase_2;
    float sr = x->x_sr;
    while (nblock--){
        float freq1 = *in1++;
        float index1 = *in2++;
        float freq2 = *in3++;
        float index2 = *in4++;
        float radians1 = (phase1 + (yn * index2)) * TWOPI;
        float output1 = sin(radians1);
        float radians2 = (phase2 + (output1 * index1)) * TWOPI;
        *out1++ = output1;
        *out2++  = yn = sin(radians2);
        phase1 += (double)(freq1 / sr);
        phase2 += (double)(freq2 / sr);
    }
    x->x_yn = yn; // 1 sample feedback
    x->x_phase_1 = fmod(phase1, 1);
    x->x_phase_2 = fmod(phase2, 1);
    return (w + 9);
}

static void xmod_dsp(t_xmod *x, t_signal **sp)
{
    x->x_sr = sp[0]->s_sr;
    dsp_add(xmod_perform, 8, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec,
            sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec);
}

static void *xmod_free(t_xmod *x)
{
    inlet_free(x->x_inlet_fb);
    inlet_free(x->x_inlet_freq2);
    inlet_free(x->x_inlet_fb2);
    outlet_free(x->x_outlet1);
    outlet_free(x->x_outlet2);
    return (void *)x;
}

static void *xmod_new(t_symbol *s, int ac, t_atom *av)
{
    t_xmod *x = (t_xmod *)pd_new(xmod_class);
   
    t_float init_freq1 = 0;
    t_float init_fb1 = 0;
    t_float init_freq2 = 0;
    t_float init_fb2 = 0;
    if (ac && av->a_type == A_FLOAT){
        init_freq1 = av->a_w.w_float;
        ac--; av++;
        if (ac && av->a_type == A_FLOAT){
            init_fb1 = av->a_w.w_float;
            ac--; av++;
            if (ac && av->a_type == A_FLOAT){
                init_freq2 = av->a_w.w_float;
                ac--; av++;
                if (ac && av->a_type == A_FLOAT){
                    init_fb2 = av->a_w.w_float;
                    ac--; av++;
                }
            }
        }
    }
    x->x_freq = init_freq1;
    x->x_inlet_fb = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_fb, init_fb1);
    x->x_inlet_freq2 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_freq2, init_freq2);
    x->x_inlet_fb2 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_fb2, init_fb2);
    x->x_outlet1 = outlet_new(&x->x_obj, &s_signal);
    x->x_outlet2 = outlet_new(&x->x_obj, &s_signal);
    return (x);
}

void xmod_tilde_setup(void)
{
    xmod_class = class_new(gensym("xmod~"),
        (t_newmethod)xmod_new, (t_method)xmod_free,
        sizeof(t_xmod), CLASS_DEFAULT, A_GIMME, 0);
    CLASS_MAINSIGNALIN(xmod_class, t_xmod, x_freq);
    class_addmethod(xmod_class, (t_method)xmod_dsp, gensym("dsp"), A_CANT, 0);
}
