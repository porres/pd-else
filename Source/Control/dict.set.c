// Porres 2026

#include <m_pd.h>
#include "dict.h"
#include "cJSON_Utils.h"
#include <string.h>

typedef struct _dictset{
    t_object    x_obj;
    t_symbol   *x_sym;
    t_symbol   *x_key;
}t_dictset;

static t_class *dictset_class;

static void dictset_name(t_dictset *x, t_symbol *name){
    x->x_sym = (name == &s_) ? NULL : name;
}

static void dictset_click(t_dictset *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
    t_dict *dict = dict_get(x->x_sym, gensym("set"));
    if(dict)
        dict_open(dict);
}

static void dictset_set(t_dictset *x, t_symbol *s){
    if(s == &s_){
        x->x_key = NULL;
        return;
    }
    t_dict *dict = dict_get(x->x_sym, gensym("set"));
    if(dict && dict->x_root){
        const char *path = s->s_name;
        char buf[512];
        strncpy(buf, path, sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = 0;
        char *slash = strrchr(buf, '/');
        cJSON *parent = dict->x_root;
        char *leaf = buf;
        if(slash){
            *slash = 0;
            leaf = slash + 1;
            char parentpath[520];
            snprintf(parentpath, sizeof(parentpath), "/%s", buf);
            parent = cJSONUtils_GetPointerCaseSensitive(dict->x_root, parentpath);
        }
        if(!parent || !cJSON_IsObject(parent) ||
           !cJSON_GetObjectItemCaseSensitive(parent, leaf)){
            post("[dict.set] key \"%s\" not found", s->s_name);
            x->x_key = NULL;
            return;
        }
    }
    x->x_key = s;
}

static void dictset_store(t_dictset *x, cJSON *value){
    if(!x->x_key){
        post("[dict.set] no key set");
        cJSON_Delete(value);
        return;
    }
    t_dict *dict = dict_get(x->x_sym, gensym("set"));
    if(!dict){
        cJSON_Delete(value);
        return;
    }
    if(!dict->x_root){
        post("[dict.set] key \"%s\" not found", x->x_key->s_name);
        cJSON_Delete(value);
        return;
    }
    const char *path = x->x_key->s_name;
    char buf[512];
    strncpy(buf, path, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;
    char *slash = strrchr(buf, '/');
    cJSON *parent = dict->x_root;
    char *leaf = buf;
    if(slash){
        *slash = 0;
        leaf = slash + 1;
        char parentpath[520];
        snprintf(parentpath, sizeof(parentpath), "/%s", buf);
        parent = cJSONUtils_GetPointerCaseSensitive(dict->x_root, parentpath);
        if(!parent || !cJSON_IsObject(parent)){
            post("[dict.set] parent path \"%s\" not found", buf);
            cJSON_Delete(value);
            return;
        }
    }
    if(!cJSON_GetObjectItemCaseSensitive(parent, leaf)){
        post("[dict.set] key \"%s\" not found", x->x_key->s_name);
        cJSON_Delete(value);
        return;
    }
    cJSON_DeleteItemFromObjectCaseSensitive(parent, leaf);
    cJSON_AddItemToObject(parent, leaf, value);
    if(dict->x_is_opened)
        dict_do_update(dict);
    dict_dirty(dict);
}

static void dictset_float(t_dictset *x, t_float f){
    dictset_store(x, cJSON_CreateNumber(f));
}

static void dictset_symbol(t_dictset *x, t_symbol *s){
    dictset_store(x, cJSON_CreateString(s->s_name));
}

static void dictset_list(t_dictset *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(!ac)
        return;
    char buf[MAXPDSTRING * 16];
    size_t pos = 0;
    buf[0] = 0;
    for(int i = 0; i < ac; i++){
        char tmp[MAXPDSTRING];
        if(av[i].a_type == A_FLOAT)
            snprintf(tmp, sizeof(tmp), "%g", atom_getfloat(av + i));
        else if(av[i].a_type == A_SYMBOL)
            snprintf(tmp, sizeof(tmp), "%s", atom_getsymbol(av + i)->s_name);
        else
            continue;
        size_t len = strlen(tmp);
        if(pos + len + 1 >= sizeof(buf))
            break;
        memcpy(buf + pos, tmp, len);
        pos += len;
        buf[pos++] = ' ';
        buf[pos] = 0;
    }
    cJSON *value = cJSON_Parse(buf);
    if(!value){
        post("[dict.set] JSON parse error in value");
        return;
    }
    dictset_store(x, value);
}

static void *dictset_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_dictset *x = (t_dictset *)pd_new(dictset_class);
    x->x_sym = NULL;
    x->x_key = NULL;
    if(ac && av->a_type == A_SYMBOL){
        x->x_sym = atom_getsymbol(av);
        ac--; av++;
    }
    if(ac && av->a_type == A_SYMBOL)
        x->x_key = atom_getsymbol(av);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("set"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("name"));
    return(x);
}

void setup_dict0x2eset(void){
    dictset_class = class_new(gensym("dict.set"), (t_newmethod)(void *)dictset_new,
        0, sizeof(t_dictset), 0, A_GIMME, 0);
    class_addfloat(dictset_class, dictset_float);
    class_addsymbol(dictset_class, dictset_symbol);
    class_addlist(dictset_class, dictset_list);
    class_addmethod(dictset_class, (t_method)dictset_set, gensym("set"), A_SYMBOL, 0);
    class_addmethod(dictset_class, (t_method)dictset_name, gensym("name"), A_SYMBOL, 0);
    class_addmethod(dictset_class, (t_method)dictset_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
}
