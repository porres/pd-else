// porres

#include "m_pd.h"

static t_class *select_class;

typedef struct _select{
    t_object x_obj;
    int      x_n;
    int      x_sel;
}t_select;

static void select_float(t_select *x, t_floatarg f){
    x->x_sel = f;
    canvas_update_dsp();
}
    
static void select_dsp(t_select *x, t_signal **sp){
    int n = x->x_n, i = (x->x_sel > n ? n : x->x_sel) - 1;
    if(i >= 0){
        signal_setmultiout(&sp[n], sp[i]->s_nchans);
        dsp_add_copy(sp[i]->s_vec, sp[n]->s_vec, sp[i]->s_nchans*sp[0]->s_n);
    }
    else{
        signal_setmultiout(&sp[n], 1);
        dsp_add_zero(sp[n]->s_vec, sp[0]->s_n);
    }
}

static void *select_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_select *x = (t_select *)pd_new(select_class);
    x->x_n = 2;
    x->x_sel = 0;
    if(ac){
        x->x_n = atom_getfloat(av);
        ac--, av++;
    }
    if(ac){
        x->x_sel = atom_getfloat(av);
        ac--, av++;
    }
    if(x->x_n < 1)
        x->x_n = 1;
    if(x->x_sel < 0)
        x->x_n = 0;
    for(int i = 1; i < x->x_n; i++)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void select_tilde_setup(void){
    select_class = class_new(gensym("select~"), (t_newmethod)select_new,
        0, sizeof(t_select), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addfloat(select_class, select_float);
    class_addmethod(select_class, nullfn, gensym("signal"), 0);
    class_addmethod(select_class, (t_method)select_dsp, gensym("dsp"), 0);
}
