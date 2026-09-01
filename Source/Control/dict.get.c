// Porres 2026

#include <m_pd.h>
#include "dict.h"
#include "cJSON_Utils.h"

typedef struct _dictget{
    t_object    x_obj;
    t_symbol   *x_sym;
    t_outlet   *x_typeout;
}t_dictget;

static t_class *dictget_class;

static void dictget_name(t_dictget *x, t_symbol *name){
    x->x_sym = (name == &s_) ? NULL : name;
}

static void dictget_click(t_dictget *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
    t_dict *dict = dict_get(x->x_sym, gensym("get"));
    if(dict)
        dict_open(dict);
}

static int dictget_write_item(cJSON *item, t_atom *out, int pos);

static int dictget_write_object(cJSON *item, t_atom *out, int pos){
    t_atom a;
    SETSYMBOL(&a, gensym("{"));
    out[pos++] = a;
    cJSON *child = item->child;
    int first = 1;
    while(child){
        if(!first){
            SETSYMBOL(&a, gensym(","));
            out[pos++] = a;
        }
        char key[512];
        snprintf(key, sizeof(key), "\"%s\":", child->string);
        SETSYMBOL(&a, gensym(key));
        out[pos++] = a;
        pos = dictget_write_item(child, out, pos);
        first = 0;
        child = child->next;
    }
    SETSYMBOL(&a, gensym("}"));
    out[pos++] = a;
    return(pos);
}

static int dictget_write_array(cJSON *item, t_atom *out, int pos, int bracket){
    t_atom a;
    if(bracket){
        SETSYMBOL(&a, gensym("["));
        out[pos++] = a;
    }
    cJSON *child = item->child;
    while(child){
        pos = dictget_write_item(child, out, pos);
        child = child->next;
    }
    if(bracket){
        SETSYMBOL(&a, gensym("]"));
        out[pos++] = a;
    }
    return(pos);
}

static int dictget_write_item(cJSON *item, t_atom *out, int pos){
    t_atom a;
    if(cJSON_IsNumber(item)){
        SETFLOAT(&a, item->valuedouble);
        out[pos++] = a;
    }
    else if(cJSON_IsString(item)){
        SETSYMBOL(&a, gensym(item->valuestring));
        out[pos++] = a;
    }
    else if(cJSON_IsArray(item))
        pos = dictget_write_array(item, out, pos, 1);
    else if(cJSON_IsObject(item))
        pos = dictget_write_object(item, out, pos);
    return(pos);
}

static int dictget_count(cJSON *item){
    int n = 0;
    if(cJSON_IsNumber(item) || cJSON_IsString(item))
        return(1);
    if(cJSON_IsArray(item)){
        n += 2;
        cJSON *child = item->child;
        while(child){
            n += dictget_count(child);
            child = child->next;
        }
    }
    else if(cJSON_IsObject(item)){
        n += 2;
        cJSON *child = item->child;
        int first = 1;
        while(child){
            if(!first)
                n++;
            n++;
            n += dictget_count(child);
            first = 0;
            child = child->next;
        }
    }
    return(n);
}

static void dictget_symbol(t_dictget *x, t_symbol *s){
    t_dict *dict = dict_get(x->x_sym, gensym("get"));
    if(!dict || !dict->x_root)
        return;
    char path[512];
    snprintf(path, sizeof(path), "/%s", s->s_name);
    cJSON *item = cJSONUtils_GetPointerCaseSensitive(dict->x_root, path);
    if(!item){
        post("[dict.get] key \"%s\" not found", s->s_name);
        return;
    }
    if(cJSON_IsNumber(item)){
        outlet_float(x->x_typeout, 0);
        outlet_float(x->x_obj.ob_outlet, item->valuedouble);
    }
    else if(cJSON_IsString(item)){
        outlet_float(x->x_typeout, 1);
        outlet_symbol(x->x_obj.ob_outlet, gensym(item->valuestring));
    }
    else if(cJSON_IsArray(item)){
        int n = dictget_count(item);
        t_atom *out = (t_atom *)getbytes(sizeof(t_atom) * (n + 4));
        int outn = dictget_write_array(item, out, 0, 0);
        outlet_float(x->x_typeout, 2);
        outlet_list(x->x_obj.ob_outlet, &s_list, outn, out);
        freebytes(out, sizeof(t_atom) * (n + 4));
    }
    else if(cJSON_IsObject(item)){
        int n = dictget_count(item);
        t_atom *out = (t_atom *)getbytes(sizeof(t_atom) * (n + 4));
        int outn = dictget_write_object(item, out, 0);
        outlet_float(x->x_typeout, 3);
        outlet_list(x->x_obj.ob_outlet, &s_list, outn, out);
        freebytes(out, sizeof(t_atom) * (n + 4));
    }
}

static void *dictget_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_dictget *x = (t_dictget *)pd_new(dictget_class);
    x->x_sym = NULL;
    if(ac && av->a_type == A_SYMBOL)
        x->x_sym = atom_getsymbol(av);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("name"));
    outlet_new(&x->x_obj, &s_list);
    x->x_typeout = outlet_new(&x->x_obj, &s_float);
    return(x);
}

void setup_dict0x2eget(void){
    dictget_class = class_new(gensym("dict.get"), (t_newmethod)(void *)dictget_new,
        0, sizeof(t_dictget), 0, A_GIMME, 0);
    class_addsymbol(dictget_class, dictget_symbol);
    class_addmethod(dictget_class, (t_method)dictget_name, gensym("name"), A_SYMBOL, 0);
    class_addmethod(dictget_class, (t_method)dictget_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
}
