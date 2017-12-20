// matt barber and porres (2017)
// based on SuperCollider's GrayNoise UGen

#include "m_pd.h"

static t_class *gray_class;

typedef struct _gray{
    t_object  x_obj;
    int x_val;
    int x_base;
    t_outlet *x_outlet;
} t_gray;

static void gray_float(t_gray *x, t_floatarg f){
    x->x_val = x->x_base = f;
}

static t_int *gray_perform(t_int *w){
    t_gray *x = (t_gray *)(w[1]);
    int nblock = (t_int)(w[2]);
    int *vp = (int *)(w[3]);
    t_sample *out = (t_sample *)(w[4]);
    int val = *vp;
    int base = x->x_base;
    while(nblock--){
        t_float noise = ((float)((val & 0x7fffffff) - 0x40000000)) * (float)(16.0 / 0x40000000);
        int shift = (int)(noise + 16.0);
        shift = (shift == 32 ? 31 : shift);
        base ^= 1L << shift;
        *out++ = base * 4.65661287308e-10f; // That's 1/(2^31), so normalizes the int to 1.0
        val = val * 435898247 + 382842987;
    }
    *vp = val;
    x->x_base = base;
    return (w + 5);
}

static void gray_dsp(t_gray *x, t_signal **sp){
    dsp_add(gray_perform, 4, x, sp[0]->s_n, &x->x_val, sp[0]->s_vec);
}

static void *gray_new(t_symbol *s, int ac, t_atom *av){
    t_gray *x = (t_gray *)pd_new(gray_class);
    static int init = 307;
    x->x_val = x->x_base = (init *= 1319);
    if(ac)
        if(av->a_type == A_FLOAT)
            x->x_val = x->x_base = atom_getfloatarg(0, ac, av) * 1319;
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void gray_tilde_setup(void){
    gray_class = class_new(gensym("gray~"), (t_newmethod)gray_new,
                            0, sizeof(t_gray), 0, A_GIMME, 0);
    class_addmethod(gray_class, (t_method)gray_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(gray_class, gray_float);
}
