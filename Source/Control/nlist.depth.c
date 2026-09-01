// Porres 2026

#include <m_pd.h>
#include "nlist.h"

typedef struct _nlistdepth{
    t_object    x_obj;
    t_symbol   *x_sym;
}t_nlistdepth;

static t_class *nlistdepth_class;

static void nlistdepth_name(t_nlistdepth *x, t_symbol *name){
    x->x_sym = (name == &s_) ? NULL : name;
}

static void nlistdepth_click(t_nlistdepth *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
    t_nlist *nlist = nlist_get(x->x_sym, gensym("depth"));
    if(nlist)
        nlist_open(nlist);
}

static void nlistdepth_bang(t_nlistdepth *x){
    t_nlist *nlist = nlist_get(x->x_sym, gensym("depth"));
    if(nlist)
        outlet_float(x->x_obj.ob_outlet, nlist->x_depth);
}

static void *nlistdepth_new(t_symbol *s, int ac, t_atom* av){
    (void)s;
    t_nlistdepth *x = (t_nlistdepth *)pd_new(nlistdepth_class);
    x->x_sym = NULL;
    if(ac && av->a_type == A_SYMBOL)
        x->x_sym = atom_getsymbol(av);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("name"));
    outlet_new(&x->x_obj, &s_float);
    return(x);
}

void setup_nlist0x2edepth(void){
    nlistdepth_class = class_new(gensym("nlist.depth"),
        (t_newmethod)(void *)nlistdepth_new, 0, sizeof(t_nlistdepth), 0, A_GIMME, 0);
    class_addbang(nlistdepth_class, nlistdepth_bang);
    class_addmethod(nlistdepth_class, (t_method)nlistdepth_name, gensym("name"), A_SYMBOL, 0);
    class_addmethod(nlistdepth_class, (t_method)nlistdepth_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
}
