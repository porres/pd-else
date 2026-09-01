// Porres 2026

#include "dict.h"
#include <m_pd.h>
#include <g_canvas.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void dict_dirty(t_dict *x){
    if(x->x_keep && x->x_canvas)
        canvas_dirty(x->x_canvas, 1);
}

void dict_do_update(t_dict *x){
    if(x->x_is_opened && x->x_filehandle){
        char buf[256];
        snprintf(buf, sizeof(buf),
            "if {[winfo exists .%lx]} { .%lx.text delete 1.0 end }",
            (unsigned long)x->x_filehandle,
            (unsigned long)x->x_filehandle);
        pdgui_vmess(buf, NULL);
        if(x->x_root){
            char *printed = cJSON_Print(x->x_root);
            if(printed){
                else_editor_append(x->x_filehandle, printed);
                free(printed);
            }
        }
        else_editor_setdirty(x->x_filehandle, 0);
    }
}

void dict_open_window(unsigned long handle){
    char buf[256];
    snprintf(buf, sizeof(buf), "wm deiconify .%lx; raise .%lx; focus .%lx.text",
        handle, handle, handle);
    pdgui_vmess(buf, NULL);
}

void dict_do_open(t_dict *x){
    if(x->x_is_opened)
        dict_open_window((unsigned long)x->x_filehandle);
    else{
        const char *title = (x->x_name ? x->x_name->s_name : "Untitled");
        elsefile_editor_open(x->x_filehandle, (char *)title, "dict");
        if(x->x_root){
            char *printed = cJSON_Print(x->x_root);
            if(printed){
                else_editor_append(x->x_filehandle, printed);
                free(printed);
            }
        }
        else_editor_setdirty(x->x_filehandle, 0);
        x->x_is_opened = 1;
    }
}

void dict_open(t_dict *x){
    char buf[512];
    snprintf(buf, sizeof(buf),
        "if {[winfo exists .%lx]} {"
        "pdsend \"%s _is_opened 1\""
        "} else {"
        "pdsend \"%s _is_opened 0\""
        "}",
        (unsigned long)x->x_filehandle,
        x->x_bindsym->s_name,
        x->x_bindsym->s_name);
    pdgui_vmess(buf, NULL);
}

t_dict *dict_get(t_symbol *name, t_symbol *obj){
    if(name == NULL){
        post("[dict.%s] no name given", obj->s_name);
        return(NULL);
    }
    t_dict *dict = (t_dict *)pd_findbyclassname(name, gensym("dict"));
    t_dict *d2 = (t_dict *)pd_findbyclassname(name, gensym("dict.define"));
    if(dict && d2)
        post("warning %s multiply defined", name->s_name);
    if(!dict && d2)
        dict = d2;
    if(!dict)
        post("[dict.%s] \"%s\" name not found", obj->s_name, name->s_name);
    return(dict);
}
