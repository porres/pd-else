// Porres 2017

#include "m_pd.h"

static t_class *brown_class;

typedef struct _brown{
    t_object  x_obj;
    int x_val;
    t_float  x_lastout;
    t_outlet *x_outlet;
} t_brown;

static void brown_float(t_brown *x, t_floatarg f){
    x->x_val = f;
}

static t_int *brown_perform(t_int *w){
    t_brown *x = (t_brown *)(w[1]);
    int nblock = (t_int)(w[2]);
    int *vp = (int *)(w[3]);
    t_sample *out = (t_sample *)(w[4]);
    int val = *vp;
    t_float lastout = x->x_lastout;
    while(nblock--){
        t_float noise = ((float)((val & 0x7fffffff) - 0x40000000)) * (float)(1.0 / 0x40000000);
        t_float input = noise * 0.125;
        t_float output = input + lastout;
        if (output > 1)
            output = 2 - output;
        if (output < -1)
            output = -2 - output;
        *out++ = output;
        val = val * 435898247 + 382842987;
        lastout = output;
    }
     *vp = val;
    x->x_lastout = lastout;
    return (w + 5);
}

static void brown_dsp(t_brown *x, t_signal **sp){
    dsp_add(brown_perform, 4, x, sp[0]->s_n, &x->x_val, sp[0]->s_vec);
}

static void *brown_new(t_symbol *s, int ac, t_atom *av){
    t_brown *x = (t_brown *)pd_new(brown_class);
    x->x_lastout = 0;
    static int init = 307;
    x->x_val = (init *= 1319);
    if(ac)
        if(av->a_type == A_FLOAT)
            x->x_val = atom_getfloatarg(0, ac, av) * 1319;
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void brown_tilde_setup(void){
    brown_class = class_new(gensym("brown~"), (t_newmethod)brown_new,
                            0, sizeof(t_brown), 0, A_GIMME, 0);
    class_addmethod(brown_class, (t_method)brown_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(brown_class, brown_float);
}
