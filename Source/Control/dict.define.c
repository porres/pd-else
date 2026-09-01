// Porres 2026

#include <m_pd.h>
#include <g_canvas.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "dict.h"
#include "cJSON_Utils.h"

static t_class *dict_class;
static t_class *dictdefine_class;

static void dict_is_opened(t_dict *x, t_floatarg f){
    x->x_is_opened = (int)f;
    dict_do_open(x);
}

static void dict_do_read(t_dict *x, t_symbol *fn){
    if(!fn || fn == &s_)
        return;
    char buf[MAXPDSTRING];
    char *bufptr;
    int fd = canvas_open(x->x_canvas, fn->s_name, "", buf, &bufptr, MAXPDSTRING, 1);
    if(fd > 0){
        buf[strlen(buf)] = '/';
        sys_close(fd);
    }
    else{
        post("[dict.define] file '%s' not found", fn->s_name);
        return;
    }
    FILE *f = sys_fopen(buf, "r");
    if(!f){
        post("[dict.define] can't read file '%s'", fn->s_name);
        return;
    }
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);
    if(size <= 0){
        fclose(f);
        return;
    }
    char *text = (char *)getbytes(size + 1);
    fread(text, 1, size, f);
    text[size] = 0;
    fclose(f);
    cJSON *root = cJSON_Parse(text);
    freebytes(text, size + 1);
    if(!root){
        post("[dict.define] JSON parse error in '%s'", fn->s_name);
        return;
    }
    if(x->x_root)
        cJSON_Delete(x->x_root);
    x->x_root = root;
    dict_dirty(x);
    if(x->x_is_opened)
        dict_do_update(x);
    x->x_readfile = fn;
}

static void dict_readhook(t_pd *z, t_symbol *fn, int ac, t_atom *av){
    (void)ac, (void)av;
    t_dict *x = (t_dict *)z;
    if(fn && fn != &s_)
        dict_do_read(x, fn);
}

static void dict_do_write(t_dict *x, t_symbol *fn){
    if(!fn || fn == &s_)
        return;
    if(!x->x_root){
        post("[dict.define] nothing to write");
        return;
    }
    char buf[MAXPDSTRING];
    if(x->x_canvas)
        canvas_makefilename(x->x_canvas, fn->s_name, buf, MAXPDSTRING);
    else{
        strncpy(buf, fn->s_name, MAXPDSTRING);
        buf[MAXPDSTRING-1] = 0;
    }
    char *printed = cJSON_Print(x->x_root);
    if(!printed)
        return;
    FILE *f = sys_fopen(buf, "w");
    if(!f){
        post("[dict.define] error writing text file '%s'", fn->s_name);
        free(printed);
        return;
    }
    fputs(printed, f);
    fclose(f);
    free(printed);
    x->x_readfile = fn;
}

static void dict_writehook(t_pd *z, t_symbol *fn, int ac, t_atom *av){
    (void)ac, (void)av;
    t_dict *x = (t_dict *)z;
    if(fn && fn != &s_)
        dict_do_write(x, fn);
}

static void dict_keephook(t_pd *z, t_binbuf *bb, t_symbol *bindsym){
    t_dict *x = (t_dict *)z;
    if(x->x_keep && x->x_root){
        char *printed = cJSON_PrintUnformatted(x->x_root);
        if(printed){
            t_atom head[2];
            SETSYMBOL(head, bindsym);
            SETSYMBOL(head + 1, gensym("list"));
            binbuf_add(bb, 2, head);
            t_binbuf *tmp = binbuf_new();
            binbuf_text(tmp, printed, strlen(printed));
            binbuf_addbinbuf(bb, tmp);
            binbuf_free(tmp);
            binbuf_addsemi(bb);
            binbuf_addv(bb, "ssi;", bindsym, gensym("keep"), 1);
            free(printed);
        }
    }
    obj_saveformat((t_object *)x, bb);
}

static void dict_editorhook(t_pd *z, t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_dict *x = (t_dict *)z;
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
        else if(av[i].a_type == A_COMMA)
            snprintf(tmp, sizeof(tmp), ",");
        else if(av[i].a_type == A_SEMI)
            snprintf(tmp, sizeof(tmp), ";");
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
    cJSON *root = cJSON_Parse(buf);
    if(!root){
        const char *err = cJSON_GetErrorPtr();
        post("[dict.define] parse error at: %s", err ? err : "(null)");
        post("[dict.define] input: %s", buf);
        return;
    }
    if(x->x_root)
        cJSON_Delete(x->x_root);
    x->x_root = root;
    dict_dirty(x);
}

// ------------------- METHODS!!! ----------------------------------------
static void dict_click(t_dict *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos,(void)ypos, (void)ctrl;
    if(shift)
        elsefile_panel_click_open(x->x_filehandle);
    else if(alt)
        elsefile_panel_save(x->x_filehandle, 0, 0);
    else
        dict_open(x);
}

static void dict_show(t_dict *x){
    dict_open(x);
}

static void dict_hide(t_dict *x){
    else_editor_close(x->x_filehandle, 1);
}

static void dict_clear(t_dict *x){
    if(!x->x_root)
        return;
    cJSON_Delete(x->x_root);
    x->x_root = NULL;
    if(x->x_is_opened)
        dict_do_update(x);
    dict_dirty(x);
}

static void dict_list(t_dict *x, t_symbol *s, int ac, t_atom *av){
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
    cJSON *root = cJSON_Parse(buf);
    if(!root){
        const char *err = cJSON_GetErrorPtr();
        post("[dict.define] parse error at: %s", err ? err : "(null)");
        post("[dict.define] input: %s", buf);
        return;
    }
    if(x->x_root)
        cJSON_Delete(x->x_root);
    x->x_root = root;
    if(x->x_is_opened)
        dict_do_update(x);
    dict_dirty(x);
}

static void dict_keep(t_dict *x, t_float f){
    if(x->x_filearg){
        post("[dict.define]: file arg is given, so keep message is ignored");
        return;
    }
    x->x_keep = (int)(f != 0);
}

static void dict_read(t_dict *x, t_symbol *s){
    if(s && s != &s_)
        dict_do_read(x, s);
    else if(x->x_filehandle)
        elsefile_panel_click_open(x->x_filehandle);
}

static void dict_write(t_dict *x, t_symbol *s){
    if(s && s != &s_)
        dict_do_write(x, s);
    else if(x->x_filehandle){
        t_symbol *inifile = x->x_readfile;
        elsefile_panel_save(x->x_filehandle, NULL, inifile);
    }
}

static void dict_sort(t_dict *x){
    if(!x->x_root)
        return;
    cJSONUtils_SortObjectCaseSensitive(x->x_root);
    if(x->x_is_opened)
        dict_do_update(x);
    dict_dirty(x);
}

// ----------- FREE / NEW / SETUP ---------------
static void dict_free(t_dict *x){
    if(x->x_filehandle){
        dict_hide(x);
        elsefile_free(x->x_filehandle);
        x->x_filehandle = NULL;
    }
    if(x->x_root){
        cJSON_Delete(x->x_root);
        x->x_root = NULL;
    }
    pd_unbind(&x->x_obj.ob_pd, x->x_bindsym);
    if(x->x_name)
        pd_unbind(&x->x_obj.ob_pd, x->x_name);
}

static void *dict_do_new(t_class *c, int ac, t_atom *av){
    t_dict *x = (t_dict *)pd_new(c);
    x->x_canvas = canvas_getcurrent();
    x->x_keep = 0;
    x->x_is_opened = 0;
    x->x_filehandle = NULL;
    x->x_root = NULL;
    x->x_name = NULL;
    x->x_next = NULL;
    x->x_readfile = NULL;
    char buf[MAXPDSTRING];
    sprintf(buf, "#%lx", (long)x);
    pd_bind(&x->x_obj.ob_pd, x->x_bindsym = gensym(buf));
    t_symbol *name = NULL;
    t_symbol *file = NULL;
    int arg = 0;
    while(ac){
        if(av->a_type == A_SYMBOL){
            if(atom_getsymbol(av) == gensym("-k") && arg == 0)
                x->x_keep = 1;
            else if(name == NULL && arg == 0){
                name = atom_getsymbol(av);
                arg = 1;
            }
            else if(file == NULL && arg == 1){
                arg = 2;
                file = atom_getsymbol(av);
                x->x_filearg = 1;
                if(x->x_keep){
                    x->x_keep = 0;
                    post("[dict.define]: file arg is given, so keep flag is ignored");
                }
            }
            else
                goto errstate;
            ac--; av++;
        }
        else
            goto errstate;
    }
    x->x_filehandle = elsefile_new((t_pd *)x, dict_keephook,
        dict_readhook, dict_writehook, dict_editorhook);
    if(name)
        pd_bind(&x->x_obj.ob_pd, x->x_name = name);
    if(file)
        dict_read(x, file);
    return(x);
    errstate:
        pd_error(x, "[dict.define]: improper args");
        return(NULL);
}

static void *dict_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    return(dict_do_new(dict_class, ac, av));
}

static void *dictdefine_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    return(dict_do_new(dictdefine_class, ac, av));
}

static void dict_class_setup(t_class *c){
    class_addlist(c, dict_list);
    class_addmethod(c, (t_method)dict_show, gensym("show"), 0);
    class_addmethod(c, (t_method)dict_hide, gensym("hide"), 0);
    class_addmethod(c, (t_method)dict_clear, gensym("clear"), 0);
    class_addmethod(c, (t_method)dict_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(c, (t_method)dict_is_opened, gensym("_is_opened"), A_FLOAT, 0);
    class_addmethod(c, (t_method)dict_keep, gensym("keep"), A_FLOAT, 0);
    class_addmethod(c, (t_method)dict_read, gensym("read"), A_DEFSYM, 0);
    class_addmethod(c, (t_method)dict_write, gensym("write"), A_DEFSYM, 0);
    class_addmethod(c, (t_method)dict_sort, gensym("sort"), 0);
    elsefile_setup(c, 1);
}

void setup_dict0x2edefine(void){
    dictdefine_class = class_new(gensym("dict.define"), (t_newmethod)(void *)dictdefine_new,
        (t_method)dict_free, sizeof(t_dict), CLASS_DEFAULT, A_GIMME, 0);
    dict_class_setup(dictdefine_class);
}

void dict_setup(void){
    dict_class = class_new(gensym("dict"), (t_newmethod)(void *)dict_new,
        (t_method)dict_free, sizeof(t_dict), CLASS_DEFAULT, A_GIMME, 0);
    dict_class_setup(dict_class);
    class_sethelpsymbol(dict_class, gensym("dict.define"));
}
