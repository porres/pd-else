// Porres 2026

#include <m_pd.h>
#include "dict.h"

typedef struct _dictkeys{
    t_object    x_obj;
    t_symbol   *x_sym;
}t_dictkeys;

static t_class *dictkeys_class;

static void dictkeys_name(t_dictkeys *x, t_symbol *name){
    x->x_sym = (name == &s_) ? NULL : name;
}

static void dictkeys_click(t_dictkeys *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
    t_dict *dict = dict_get(x->x_sym, gensym("keys"));
    if(dict)
        dict_open(dict);
}

static void dictkeys_bang(t_dictkeys *x){
    t_dict *dict = dict_get(x->x_sym, gensym("keys"));
    if(!dict)
        return;
    if(!dict->x_root || cJSON_GetArraySize(dict->x_root) == 0){
        outlet_bang(x->x_obj.ob_outlet);
        return;
    }
    int n = cJSON_GetArraySize(dict->x_root);
    t_atom *out = (t_atom *)getbytes(sizeof(t_atom) * n);
    int i = 0;
    cJSON *child = dict->x_root->child;
    while(child && i < n){
        SETSYMBOL(out + i, gensym(child->string));
        i++;
        child = child->next;
    }
    outlet_list(x->x_obj.ob_outlet, &s_list, i, out);
    freebytes(out, sizeof(t_atom) * n);
}

static void *dictkeys_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_dictkeys *x = (t_dictkeys *)pd_new(dictkeys_class);
    x->x_sym = NULL;
    if(ac && av->a_type == A_SYMBOL)
        x->x_sym = atom_getsymbol(av);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("name"));
    outlet_new(&x->x_obj, &s_list);
    return(x);
}

void setup_dict0x2ekeys(void){
    dictkeys_class = class_new(gensym("dict.keys"), (t_newmethod)(void *)dictkeys_new,
        0, sizeof(t_dictkeys), 0, A_GIMME, 0);
    class_addbang(dictkeys_class, dictkeys_bang);
    class_addmethod(dictkeys_class, (t_method)dictkeys_name, gensym("name"), A_SYMBOL, 0);
    class_addmethod(dictkeys_class, (t_method)dictkeys_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
}
