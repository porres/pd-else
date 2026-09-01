// Porres 2026

#include <m_pd.h>
#include "dict.h"

typedef struct _dictsize{
    t_object    x_obj;
    t_symbol   *x_sym;
}t_dictsize;

static t_class *dictsize_class;

static void dictsize_name(t_dictsize *x, t_symbol *name){
    x->x_sym = (name == &s_) ? NULL : name;
}

static void dictsize_click(t_dictsize *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
    t_dict *dict = dict_get(x->x_sym, gensym("size"));
    if(dict)
        dict_open(dict);
}

static void dictsize_bang(t_dictsize *x){
    t_dict *dict = dict_get(x->x_sym, gensym("size"));
    if(!dict)
        return;
    if(!dict->x_root){
        outlet_float(x->x_obj.ob_outlet, 0);
        return;
    }
    int n = cJSON_GetArraySize(dict->x_root);
    outlet_float(x->x_obj.ob_outlet, n);
}

static void *dictsize_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_dictsize *x = (t_dictsize *)pd_new(dictsize_class);
    x->x_sym = NULL;
    if(ac && av->a_type == A_SYMBOL)
        x->x_sym = atom_getsymbol(av);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("name"));
    outlet_new(&x->x_obj, &s_float);
    return(x);
}

void setup_dict0x2esize(void){
    dictsize_class = class_new(gensym("dict.size"), (t_newmethod)(void *)dictsize_new,
        0, sizeof(t_dictsize), 0, A_GIMME, 0);
    class_addbang(dictsize_class, dictsize_bang);
    class_addmethod(dictsize_class, (t_method)dictsize_name, gensym("name"), A_SYMBOL, 0);
    class_addmethod(dictsize_class, (t_method)dictsize_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
}
