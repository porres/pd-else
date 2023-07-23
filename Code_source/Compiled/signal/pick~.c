// porres

#include "m_pd.h"

static t_class *pick_class;

typedef struct _pick{
    t_object x_obj;
    int      x_ch;
}t_pick;

static void pick_float(t_pick *x, t_floatarg f){
    x->x_ch = f;
    canvas_update_dsp();
}
    
static void pick_dsp(t_pick *x, t_signal **sp){
    signal_setmultiout(&sp[1], 1);
    int ch = x->x_ch;
    if(ch == 0){
        dsp_add_zero(sp[1]->s_vec, sp[0]->s_length);
        return;
    }
    else if(ch > 0){
        if(ch > x->x_ch){
            dsp_add_zero(sp[1]->s_vec, sp[0]->s_length);
            return;
        }
        ch--;
    }
    else{ // negative
        ch += sp[0]->s_nchans;
        if(ch < 0){
            dsp_add_zero(sp[1]->s_vec, sp[0]->s_length);
            return;
        }
    }
    dsp_add_copy(sp[0]->s_vec + ch * sp[0]->s_length, sp[1]->s_vec, sp[0]->s_length);
}

static void *pick_new(t_floatarg f){
    t_pick *x = (t_pick *)pd_new(pick_class);
    x->x_ch = f;
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void pick_tilde_setup(void){
    pick_class = class_new(gensym("pick~"), (t_newmethod)pick_new,
        0, sizeof(t_pick), CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
    class_addfloat(pick_class, pick_float);
    class_addmethod(pick_class, nullfn, gensym("signal"), 0);
    class_addmethod(pick_class, (t_method)pick_dsp, gensym("dsp"), 0);
}
