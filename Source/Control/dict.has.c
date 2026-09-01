// Porres 2026

#include <m_pd.h>
#include "dict.h"
#include "cJSON_Utils.h"

typedef struct _dicthas{
    t_object    x_obj;
    t_symbol   *x_sym;
}t_dicthas;

static t_class *dicthas_class;

static void dicthas_name(t_dicthas *x, t_symbol *name){
    x->x_sym = (name == &s_) ? NULL : name;
}

static void dicthas_click(t_dicthas *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
    t_dict *dict = dict_get(x->x_sym, gensym("has"));
    if(dict)
        dict_open(dict);
}

static void dicthas_symbol(t_dicthas *x, t_symbol *s){
    t_dict *dict = dict_get(x->x_sym, gensym("has"));
    if(!dict)
        return;
    if(!dict->x_root){
        outlet_float(x->x_obj.ob_outlet, 0);
        return;
    }
    char path[512];
    snprintf(path, sizeof(path), "/%s", s->s_name);
    cJSON *item = cJSONUtils_GetPointerCaseSensitive(dict->x_root, path);
    outlet_float(x->x_obj.ob_outlet, item ? 1 : 0);
}

static void *dicthas_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_dicthas *x = (t_dicthas *)pd_new(dicthas_class);
    x->x_sym = NULL;
    if(ac && av->a_type == A_SYMBOL)
        x->x_sym = atom_getsymbol(av);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("name"));
    outlet_new(&x->x_obj, &s_float);
    return(x);
}

void setup_dict0x2ehas(void){
    dicthas_class = class_new(gensym("dict.has"), (t_newmethod)(void *)dicthas_new,
        0, sizeof(t_dicthas), 0, A_GIMME, 0);
    class_addsymbol(dicthas_class, dicthas_symbol);
    class_addmethod(dicthas_class, (t_method)dicthas_name, gensym("name"), A_SYMBOL, 0);
    class_addmethod(dicthas_class, (t_method)dicthas_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
}
