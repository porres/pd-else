// Porres 2026

#include <m_pd.h>
#include <string.h>
#include "dict.h"
#include "cJSON_Utils.h"

typedef struct _dictdelete{
    t_object    x_obj;
    t_symbol   *x_sym;
}t_dictdelete;

static t_class *dictdelete_class;

static void dictdelete_name(t_dictdelete *x, t_symbol *name){
    x->x_sym = (name == &s_) ? NULL : name;
}

static void dictdelete_click(t_dictdelete *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
    t_dict *dict = dict_get(x->x_sym, gensym("delete"));
    if(dict)
        dict_open(dict);
}

static void dictdelete_symbol(t_dictdelete *x, t_symbol *s){
    t_dict *dict = dict_get(x->x_sym, gensym("delete"));
    if(!dict || !dict->x_root)
        return;
    char path[512];
    snprintf(path, sizeof(path), "/%s", s->s_name);
    cJSON *item = cJSONUtils_GetPointerCaseSensitive(dict->x_root, path);
    if(!item){
        post("[dict.delete] key \"%s\" not found", s->s_name);
        return;
    }
    char *slash = strrchr(s->s_name, '/');
    if(!slash)
        cJSON_DeleteItemFromObjectCaseSensitive(dict->x_root, s->s_name);
    else{
        char parentpath[512];
        int len = slash - s->s_name;
        memcpy(parentpath, s->s_name, len);
        parentpath[len] = 0;
        char fullparent[520];
        snprintf(fullparent, sizeof(fullparent), "/%s", parentpath);
        cJSON *parent = cJSONUtils_GetPointerCaseSensitive(dict->x_root, fullparent);
        if(parent)
            cJSON_DeleteItemFromObjectCaseSensitive(parent, slash + 1);
    }
    if(dict->x_is_opened)
        dict_do_update(dict);
    dict_dirty(dict);
}

static void *dictdelete_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_dictdelete *x = (t_dictdelete *)pd_new(dictdelete_class);
    x->x_sym = NULL;
    if(ac && av->a_type == A_SYMBOL)
        x->x_sym = atom_getsymbol(av);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("name"));
    return(x);
}

void setup_dict0x2edelete(void){
    dictdelete_class = class_new(gensym("dict.delete"), (t_newmethod)(void *)dictdelete_new,
        0, sizeof(t_dictdelete), 0, A_GIMME, 0);
    class_addsymbol(dictdelete_class, dictdelete_symbol);
    class_addmethod(dictdelete_class, (t_method)dictdelete_name, gensym("name"), A_SYMBOL, 0);
    class_addmethod(dictdelete_class, (t_method)dictdelete_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
}
