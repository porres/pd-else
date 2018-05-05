// Porres 2018

// from music-dsp (Paul Kellet's) - most popular one used in jMax/Csound/etc.

#include "m_pd.h"

#define PINK_GAIN  .105

static t_class *pinknoise_class;

typedef struct _pinknoise{
    t_object  x_obj;
    int       x_val;
    float     x_b0;
    float     x_b1;
    float     x_b2;
    float     x_b3;
    float     x_b4;
    float     x_b5;
    float     x_b6;
    t_outlet *x_outlet;
} t_pinknoise;

static void pinknoise_float(t_pinknoise *x, t_floatarg f){
    x->x_val = f;
}

static t_int *pinknoise_perform(t_int *w){
    t_pinknoise *x = (t_pinknoise *)(w[1]);
    int n = (t_int)(w[2]);
    int *vp = (int *)(w[3]);
    t_sample *out = (t_sample *)(w[4]);
    int val = *vp;
    float b0 = x->x_b0;
    float b1 = x->x_b1;
    float b2 = x->x_b2;
    float b3 = x->x_b3;
    float b4 = x->x_b4;
    float b5 = x->x_b5;
    float b6 = x->x_b6;
    while(n--){
    // white noise (from noise~)
        t_float white = ((float)((val & 0x7fffffff) - 0x40000000)) * (float)(1.0 / 0x40000000);
        val = val * 435898247 + 382842987;
        
        b0 = 0.99886 * b0 + white * 0.0555179;
        b1 = 0.99332 * b1 + white * 0.0750759;
        b2 = 0.96900 * b2 + white * 0.1538520;
        b3 = 0.86650 * b3 + white * 0.3104856;
        b4 = 0.55000 * b4 + white * 0.5329522;
        b5 = -0.7616 * b5 - white * 0.0168980;
        
        *out++ = PINK_GAIN*(b0 + b1 + b2 + b3 + b4 + b5 + b6 + white * 0.5362);
        b6 = white * 0.115926;
    }
     *vp = val;
    x->x_b0 = b0;
    x->x_b1 = b1;
    x->x_b2 = b2;
    x->x_b3 = b3;
    x->x_b4 = b4;
    x->x_b5 = b5;
    x->x_b6 = b6;
    return (w + 5);
}

static void pinknoise_dsp(t_pinknoise *x, t_signal **sp){
    dsp_add(pinknoise_perform, 4, x, sp[0]->s_n, &x->x_val, sp[0]->s_vec);
}

static void *pinknoise_new(t_symbol *s, int ac, t_atom *av){
    t_pinknoise *x = (t_pinknoise *)pd_new(pinknoise_class);
    static int init = 307;
    x->x_val = (init *= 1319);
    if(ac && av->a_type == A_FLOAT)
        x->x_val = atom_getfloatarg(0, ac, av) * 1319;
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void pinknoise_tilde_setup(void){
    pinknoise_class = class_new(gensym("pinknoise~"), (t_newmethod)pinknoise_new,
                            0, sizeof(t_pinknoise), 0, A_GIMME, 0);
    class_addmethod(pinknoise_class, (t_method)pinknoise_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(pinknoise_class, pinknoise_float);
}
