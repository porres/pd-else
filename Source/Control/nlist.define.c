// Porres 2026

#include <m_pd.h>
#include <g_canvas.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "nlist.h"

static t_class *nlist_class;
static t_class *nlistdefine_class;

static void nlist_is_opened(t_nlist *x, t_floatarg f){ // Callback
    x->x_is_opened = (int)f;
    nlist_do_open(x);
}

static void nlist_do_read(t_nlist *x, t_symbol *fn){
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
        post("[nlist.define] file '%s' not found", fn->s_name);
        return;
    }
    t_binbuf *bb = binbuf_new();
    if(binbuf_read(bb, buf, "", 0)){
        post("[nlist.define] can't read file '%s'", fn->s_name);
        binbuf_free(bb);
        return;
    }
    int natoms = binbuf_getnatom(bb);
    t_atom *ap = binbuf_getvec(bb);
    // Clear current data
    if(x->x_root){
        nlist_clear_nodes(x->x_root);
        x->x_root = NULL;
        x->x_len = 0;
        x->x_depth = 0;
    }
    if(natoms > 0)
        x->x_root = nlist_parse_all(x, natoms, ap);
    nlist_dirty(x);
    if(x->x_is_opened)
        nlist_do_update(x);
    binbuf_free(bb);
    x->x_readfile = fn;  // Keep the filename for future reference
}

static void nlist_readhook(t_pd *z, t_symbol *fn, int ac, t_atom *av){
    (void)ac, (void)av;
    t_nlist *x = (t_nlist *)z;
    if(fn && fn != &s_)
        nlist_do_read(x, fn);
}

static void nlist_do_write(t_nlist *x, t_symbol *fn){
    if(!fn || fn == &s_)
        return;
    if(!x->x_root){
        post("[nlist.define] nothing to write");
        return;
    }
    char buf[MAXPDSTRING];
    if(x->x_canvas)
        canvas_makefilename(x->x_canvas, fn->s_name, buf, MAXPDSTRING);
    else{
        strncpy(buf, fn->s_name, MAXPDSTRING);
        buf[MAXPDSTRING-1] = 0;
    }
    t_binbuf *bb = binbuf_new();
    nlist_tree_to_binbuf(x->x_root, bb);
    if(binbuf_write(bb, buf, "", 0))
        post("[nlist.define]: error writing text file '%s'", fn->s_name);
    else
        x->x_readfile = fn;
    binbuf_free(bb);
}

static void nlist_writehook(t_pd *z, t_symbol *fn, int ac, t_atom *av){
    (void)ac, (void)av;
    t_nlist *x = (t_nlist *)z;
    // The hook is called from elsefile's save panel with the selected filename
    if(fn && fn != &s_)
        nlist_do_write(x, fn);
}

// Called via f_embedfn when saved, this binds the object to "#C", so
// messages are restored. We re-flatten the tree with nlist_tree_to_binbuf()
// and hand it back via the "list" method.
static void nlist_keephook(t_pd *z, t_binbuf *bb, t_symbol *bindsym){
    t_nlist *x = (t_nlist *)z;
    if(x->x_keep){
        t_atom head[2];
        SETSYMBOL(head, bindsym);
        SETSYMBOL(head + 1, gensym("list"));
        binbuf_add(bb, 2, head);
        if(x->x_root)
            nlist_tree_to_binbuf(x->x_root, bb);
        binbuf_addsemi(bb);
        // restores the keep flag itself, same as coll_keephook's "keep 1"
        binbuf_addv(bb, "ssi;", bindsym, gensym("keep"), 1);
    }
    obj_saveformat((t_object *)x, bb);
}

static void nlist_editorhook(t_pd *z, t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nlist *x = (t_nlist *)z;
    if(x->x_root){
        nlist_clear_nodes(x->x_root);
        x->x_root = NULL;
        x->x_len = 0;
        x->x_depth = 0;
    }
    if(ac)
        x->x_root = nlist_parse_all(x, ac, av);
    nlist_dirty(x);
    if(x->x_is_opened)
        nlist_do_update(x);
}

// ------------------- METHODS!!! ----------------------------------------
static void nlist_click(t_nlist *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos,(void)ypos, (void)ctrl;
    if(shift)
        elsefile_panel_click_open(x->x_filehandle);
    else if(alt)
        elsefile_panel_save(x->x_filehandle, 0, 0);
    else
        nlist_open(x);
}

static void nlist_show(t_nlist *x){
    nlist_open(x);
}

static void nlist_hide(t_nlist *x){
    else_editor_close(x->x_filehandle, 1);
}

static void nlist_clear(t_nlist *x){
    if(x->x_len == 0)
        return;
    x->x_len = 0;
    x->x_depth = 0;
    if(x->x_root){
        nlist_clear_nodes(x->x_root);
        x->x_root = NULL;
    }
    if(x->x_is_opened)
        nlist_do_update(x);
    nlist_dirty(x);
}

static void nlist_list(t_nlist *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(!ac)
        return;
    nlist_clear(x);
    x->x_root = nlist_parse_all(x, ac, av);
    if(x->x_is_opened)
        nlist_do_update(x);
}

static void nlist_keep(t_nlist *x, t_float f){
    if(x->x_filearg){
        post("[nlist.define]: file arg is given, so keep message is ignored");
        return;
    }
    x->x_keep = (int)(f != 0);
}

static void nlist_read(t_nlist *x, t_symbol *s){
    if(s && s != &s_)
        nlist_do_read(x, s);
    else if(x->x_filehandle)
        elsefile_panel_click_open(x->x_filehandle);
}

static void nlist_write(t_nlist *x, t_symbol *s){
    if(s && s != &s_)
        nlist_do_write(x, s);
    else if(x->x_filehandle){
        t_symbol *inifile = x->x_readfile;
        elsefile_panel_save(x->x_filehandle, NULL, inifile);
    }
}

// ----------- FREE / NEW / SETUP ---------------
static void nlist_free(t_nlist *x){
    if(x->x_filehandle){
        nlist_hide(x);
        elsefile_free(x->x_filehandle);
        x->x_filehandle = NULL;
    }
    if(x->x_root){
        nlist_clear_nodes(x->x_root);
        x->x_root = NULL;
    }
    pd_unbind(&x->x_obj.ob_pd, x->x_bindsym);
    if(x->x_name)
        pd_unbind(&x->x_obj.ob_pd, x->x_name);
}

static void *nlist_do_new(t_class *c, int ac, t_atom *av){
    t_nlist *x = (t_nlist *)pd_new(c);
    x->x_canvas = canvas_getcurrent();
    x->x_keep = 0;
    x->x_is_opened = 0;
    x->x_filehandle = NULL;
    x->x_root = NULL;
    x->x_len = 0;
    x->x_depth = 0;
    x->x_name = NULL;
    x->x_next = NULL;
    x->x_readfile = NULL;
    char buf[MAXPDSTRING];
    sprintf(buf, "#%lx", (long)x);
    pd_bind(&x->x_obj.ob_pd, x->x_bindsym = gensym(buf));
    t_symbol *name = NULL;
    t_symbol *file = NULL;
    x->x_root = NULL;
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
                    post("[nlist.define]: file arg is given, so keep flag is ignored");
                }
            }
            else
                goto errstate;
            ac--; av++;
        }
        else
            goto errstate;
    }
    x->x_filehandle = elsefile_new((t_pd *)x, nlist_keephook,
        nlist_readhook, nlist_writehook, nlist_editorhook);
    if(name)
        pd_bind(&x->x_obj.ob_pd, x->x_name = name);
    if(file)
        nlist_read(x, file);
    return(x);
    errstate:
        pd_error(x, "[nlist.define]: improper args");
        return(NULL);
}

static void *nlist_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    return(nlist_do_new(nlist_class, ac, av));
}

static void *nlistdefine_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    return(nlist_do_new(nlistdefine_class, ac, av));
}

static void nlist_class_setup(t_class *c){
    class_addlist(c, nlist_list);
    class_addmethod(c, (t_method)nlist_show, gensym("show"), 0);
    class_addmethod(c, (t_method)nlist_hide, gensym("hide"), 0);
    class_addmethod(c, (t_method)nlist_clear, gensym("clear"), 0);
    class_addmethod(c, (t_method)nlist_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(c, (t_method)nlist_is_opened, gensym("_is_opened"), A_FLOAT, 0);
    class_addmethod(c, (t_method)nlist_keep, gensym("keep"), A_FLOAT, 0);
    class_addmethod(c, (t_method)nlist_read, gensym("read"), A_DEFSYM, 0);
    class_addmethod(c, (t_method)nlist_write, gensym("write"), A_DEFSYM, 0);
    elsefile_setup(c, 1);
}

void setup_nlist0x2edefine(void){
    nlistdefine_class = class_new(gensym("nlist.define"), (t_newmethod)(void *)nlistdefine_new,
        (t_method)nlist_free, sizeof(t_nlist), CLASS_DEFAULT, A_GIMME, 0);
    nlist_class_setup(nlistdefine_class);
}

void nlist_setup(void){
    nlist_class = class_new(gensym("nlist"), (t_newmethod)(void *)nlist_new,
        (t_method)nlist_free, sizeof(t_nlist), CLASS_DEFAULT, A_GIMME, 0);
    nlist_class_setup(nlist_class);
    class_sethelpsymbol(nlist_class, gensym("nlist.define"));
}
