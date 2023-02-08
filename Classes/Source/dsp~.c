
#include "m_pd.h"
#include "g_canvas.h"

static t_class *dsp_class;

typedef struct dsp_tilde{
    t_object x_obj;
}t_dsp_tilde;

static void dsp_bang(t_dsp_tilde *x){
    outlet_float(x->x_obj.ob_outlet, pd_getdspstate());
}

static void dsp_float(t_dsp_tilde *x, t_floatarg f){
    x = NULL;
    t_atom at[1];
    SETFLOAT(at, f != 0);
    typedmess(gensym("pd")->s_thing, gensym("dsp"), 1, at);
}

static void dsp_loadbang(t_dsp_tilde *x, t_floatarg action){
    if(action == LB_LOAD)
        dsp_bang(x);
}

static void *dsp_tilde_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_dsp_tilde *x = (t_dsp_tilde *)pd_new(dsp_class);
    if(ac)
        dsp_float(x, atom_getfloat(av));
    outlet_new(&x->x_obj, &s_);
    return(void *)x;
}

void dsp_tilde_setup(void){
    dsp_class = class_new(gensym("dsp~"), (t_newmethod)dsp_tilde_new,
        0, sizeof(t_dsp_tilde), 0, A_GIMME, 0);
    class_addmethod(dsp_class, (t_method)dsp_loadbang, gensym("loadbang"), A_DEFFLOAT, 0);
    class_addbang(dsp_class, dsp_bang);
    class_addfloat(dsp_class, dsp_float);
}
