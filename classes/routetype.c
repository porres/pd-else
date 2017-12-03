#include "m_pd.h"

static t_class *routetype_class;

typedef struct _routetype{
    t_object    x_obj;
    t_outlet    *x_out_bang;
    t_outlet    *x_out_float;
    t_outlet    *x_out_symbol;
    t_outlet    *x_out_list;
    t_outlet    *x_out_anything;
} t_routetype;

static void routetype_bang(t_routetype *x){
    outlet_bang(x->x_out_bang);
}

static void routetype_float(t_routetype *x, t_floatarg f){
    outlet_float(x->x_out_float, f);
}

static void routetype_symbol(t_routetype *x, t_symbol *s){
    outlet_symbol(x->x_out_symbol, s);
}

static void routetype_list(t_routetype *x, t_symbol *sel, int argc, t_atom *argv){
  outlet_list(x->x_out_list, gensym("list"), argc, argv);
}

static void routetype_anything(t_routetype *x, t_symbol *sel, int argc, t_atom *argv){
    outlet_anything(x->x_out_anything, sel, argc, argv);
}

static void *routetype_new(t_symbol *s, int argc, t_atom *argv){
    t_routetype *x = (t_routetype *)pd_new(routetype_class);
    x->x_out_bang = outlet_new(&x->x_obj, &s_bang);
    x->x_out_float = outlet_new(&x->x_obj, &s_float);
    x->x_out_symbol = outlet_new(&x->x_obj, &s_symbol);
    x->x_out_list = outlet_new(&x->x_obj, &s_list);
    x->x_out_anything = outlet_new(&x->x_obj, &s_anything);
    return (x);
}

void routetype_setup(void){
    routetype_class = class_new(gensym("routetype"), (t_newmethod)routetype_new,
                                0, sizeof(t_routetype), 0, 0);
    class_addbang(routetype_class, routetype_bang);
    class_addfloat(routetype_class, routetype_float);
    class_addsymbol(routetype_class, routetype_symbol);
    class_addlist(routetype_class, routetype_list);
    class_addanything(routetype_class, routetype_anything);
}
