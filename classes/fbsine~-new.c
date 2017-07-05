// Porres 2017

#include "m_pd.h"
#include "math.h"

#define TWOPI (M_PI * 2)

static t_class *fbsine_class;

typedef struct _fbsine
{
    t_object x_obj;
    double  x_phase;
    double  x_last_out;
    t_float  x_freq;
    t_inlet  *x_inlet_fb;
    t_outlet *x_outlet;
    t_float x_sr;
} t_fbsine;

static t_int *fbsine_perform(t_int *w)
{
    t_fbsine *x = (t_fbsine *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]); // freq
    t_float *in2 = (t_float *)(w[4]); // fb
    t_float *out = (t_float *)(w[5]);
    float last_out = x->x_last_out;
    double phase = x->x_phase;
    float sr = x->x_sr;
    while (nblock--){
        double hz = *in1++;
        float fback = *in2++;
        float radians = (phase + last_out * fback) * TWOPI;
        *out++ = last_out = sin(radians);
        phase += (double)(hz / sr); // next phase
    }
    x->x_last_out = last_out; // last out
    x->x_phase = fmod(phase, 1); // next wrapped phase
    return (w + 6);
}

static void fbsine_dsp(t_fbsine *x, t_signal **sp)
{
    x->x_sr = sp[0]->s_sr;
    dsp_add(fbsine_perform, 5, x, sp[0]->s_n, sp[0]->s_vec,
            sp[1]->s_vec, sp[2]->s_vec);
}

static void *fbsine_free(t_fbsine *x)
{
    inlet_free(x->x_inlet_fb);
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *fbsine_new(t_symbol *s, int ac, t_atom *av)
{
    t_fbsine *x = (t_fbsine *)pd_new(fbsine_class);
    
    x->x_inlet_fb = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_fb, 0);
    
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return (x);
}

void fbsine_tilde_setup(void)
{
    fbsine_class = class_new(gensym("fbsine~"),
        (t_newmethod)fbsine_new, (t_method)fbsine_free,
        sizeof(t_fbsine), CLASS_DEFAULT, A_GIMME, 0);
    CLASS_MAINSIGNALIN(fbsine_class, t_fbsine, x_freq);
    class_addmethod(fbsine_class, (t_method)fbsine_dsp, gensym("dsp"), A_CANT, 0);
}
