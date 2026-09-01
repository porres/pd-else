// Porres 2026

#include <m_pd.h>
#include "dict.h"
#include "cJSON_Utils.h"
#include <string.h>

typedef struct _dictadd{
    t_object    x_obj;
    t_symbol   *x_sym;
    t_symbol   *x_key;
}t_dictadd;

static t_class *dictadd_class;

static void dictadd_name(t_dictadd *x, t_symbol *name){
    x->x_sym = (name == &s_) ? NULL : name;
}

static void dictadd_click(t_dictadd *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
    t_dict *dict = dict_get(x->x_sym, gensym("add"));
    if(dict)
        dict_open(dict);
}

static void dictadd_set(t_dictadd *x, t_symbol *s){
    if(s == &s_){
        x->x_key = NULL;
        return;
    }
    t_dict *dict = dict_get(x->x_sym, gensym("add"));
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
            if(!parent || !cJSON_IsObject(parent)){
                post("[dict.add] parent path \"%s\" not found", buf);
                x->x_key = NULL;
                return;
            }
        }
        if(cJSON_GetObjectItemCaseSensitive(parent, leaf)){
            post("[dict.add] key \"%s\" already exists", s->s_name);
            x->x_key = NULL;
            return;
        }
    }
    x->x_key = s;
}

static void dictadd_store(t_dictadd *x, cJSON *value){
    if(!x->x_key){
        post("[dict.add] no key set");
        cJSON_Delete(value);
        return;
    }
    t_dict *dict = dict_get(x->x_sym, gensym("add"));
    if(!dict){
        cJSON_Delete(value);
        return;
    }
    if(!dict->x_root){
        post("[dict.add] key \"%s\" not found", x->x_key->s_name);
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
            post("[dict.add] parent path \"%s\" not found", buf);
            cJSON_Delete(value);
            return;
        }
    }
    if(cJSON_GetObjectItemCaseSensitive(parent, leaf)){
        post("[dict.add] key \"%s\" already exists", x->x_key->s_name);
        cJSON_Delete(value);
        return;
    }
    cJSON_AddItemToObject(parent, leaf, value);
    if(dict->x_is_opened)
        dict_do_update(dict);
    dict_dirty(dict);
}

static void dictadd_float(t_dictadd *x, t_float f){
    dictadd_store(x, cJSON_CreateNumber(f));
}

static void dictadd_symbol(t_dictadd *x, t_symbol *s){
    dictadd_store(x, cJSON_CreateString(s->s_name));
}

static void dictadd_list(t_dictadd *x, t_symbol *s, int ac, t_atom *av){
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
        post("[dict.add] JSON parse error in value");
        return;
    }
    dictadd_store(x, value);
}

static void *dictadd_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_dictadd *x = (t_dictadd *)pd_new(dictadd_class);
    x->x_sym = NULL;
    x->x_key = NULL;
    if(ac && av->a_type == A_SYMBOL)
        x->x_sym = atom_getsymbol(av);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("set"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("name"));
    return(x);
}

void setup_dict0x2eadd(void){
    dictadd_class = class_new(gensym("dict.add"), (t_newmethod)(void *)dictadd_new,
        0, sizeof(t_dictadd), 0, A_GIMME, 0);
    class_addfloat(dictadd_class, dictadd_float);
    class_addsymbol(dictadd_class, dictadd_symbol);
    class_addlist(dictadd_class, dictadd_list);
    class_addmethod(dictadd_class, (t_method)dictadd_set, gensym("set"), A_SYMBOL, 0);
    class_addmethod(dictadd_class, (t_method)dictadd_name, gensym("name"), A_SYMBOL, 0);
    class_addmethod(dictadd_class, (t_method)dictadd_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
}
