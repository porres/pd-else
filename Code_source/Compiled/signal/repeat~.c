// porres

#include "m_pd.h"

static t_class *repeat_tilde_class;

typedef struct _repeat_tilde{
    t_object x_obj;
    t_sample x_f;
    int      x_n;
}t_repeat_tilde;

static void repeat_tilde_float(t_repeat_tilde *x, t_floatarg f){
    x->x_n = f < 1 ? 1 : (int)f;
    canvas_update_dsp();
}

static void repeat_tilde_dsp(t_repeat_tilde *x, t_signal **sp){
    int n = sp[0]->s_n, ch = sp[0]->s_nchans;
    signal_setmultiout(&sp[1], x->x_n*ch);
    for(int i = 0; i < x->x_n; i++)
         dsp_add_copy(sp[0]->s_vec, sp[1]->s_vec + i*n*ch, n*ch);
}

static void *repeat_tilde_new(t_floatarg fnchans){
    t_repeat_tilde *x = (t_repeat_tilde *)pd_new(repeat_tilde_class);
    if((x->x_n = fnchans) <= 0)
        x->x_n = 1;
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void repeat_tilde_setup(void){
    repeat_tilde_class = class_new(gensym("repeat~"), (t_newmethod)repeat_tilde_new,
        0, sizeof(t_repeat_tilde), CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(repeat_tilde_class, t_repeat_tilde, x_f);
    class_addmethod(repeat_tilde_class, (t_method)repeat_tilde_dsp, gensym("dsp"), 0);
    class_addfloat(repeat_tilde_class, repeat_tilde_float);
}
