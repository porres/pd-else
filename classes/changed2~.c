// Porres 2017

#include "m_pd.h"
#include "math.h"

static t_class *changed2_class;

typedef struct _changed2{
    t_object x_obj;
    t_float  x_xm1;
    t_float  x_last_dir;
}t_changed2;

static t_int *changed2_perform(t_int *w){
    t_changed2 *x = (t_changed2 *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    t_float xm1 = x->x_xm1;
    t_float last_dir = x->x_last_dir;
    while (nblock--){
        float x = *in++;
        int dir = (x > xm1 ? 1. : (x < xm1 ? -1. : 0.)); // output of cyclone/change~
        *out++ = dir != last_dir;
        xm1 = x; // update for next
        last_dir = dir; // update for next
        }
    x->x_xm1 = xm1;
    x->x_last_dir = last_dir;
    return (w + 5);
}

static void changed2_dsp(t_changed2 *x, t_signal **sp){
    dsp_add(changed2_perform, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
}

static void *changed2_new(void){
    t_changed2 *x = (t_changed2 *)pd_new(changed2_class);
    outlet_new((t_object *)x, &s_signal);
    return (x);
}

void changed2_tilde_setup(void){
    changed2_class = class_new(gensym("changed2~"), (t_newmethod)changed2_new,
        0, sizeof(t_changed2), CLASS_DEFAULT, 0);
    class_addmethod(changed2_class, nullfn, gensym("signal"), 0);
    class_addmethod(changed2_class, (t_method)changed2_dsp, gensym("dsp"), A_CANT, 0);
}
