// Porres 2026

#include <m_pd.h>
#include <g_canvas.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include "nlist.h"

static t_class *nlist_class;

// ------------ Open Editor helpers ----------------------------
static void nlist_open_window(unsigned long handle){
    char buf[256];
    snprintf(buf, sizeof(buf), "wm deiconify .%lx; raise .%lx; focus .%lx.text",
        handle, handle, handle);
    pdgui_vmess(buf, NULL);
}

static void nlist_do_open(t_nlist *x){
    t_nlist_node *root = x->x_root;
    if(x->x_is_opened)
        nlist_open_window((unsigned long)x->x_filehandle);
    else{
        const char *title = (x->x_name ? x->x_name->s_name : "Untitled");
        elsefile_editor_open(x->x_filehandle, (char *)title, "nlist");
        if(root)
            nlist_editor_show_nodes(x->x_filehandle, root, 0);
        else_editor_setdirty(x->x_filehandle, 0);
        x->x_is_opened = 1;
    }
}

static void nlist_is_opened(t_nlist *x, t_floatarg f){
    x->x_is_opened = (int)f;
    nlist_do_open(x);
}

// ------------------- METHODS!!! -------------------------

static void nlist_click(t_nlist *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
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
    if(x->x_keep && x->x_canvas)
        canvas_dirty(x->x_canvas, 1);
    if(x->x_is_opened)
        nlist_do_update(x);
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
    x->x_keep = f != 0;
}

// ========== HOOKS ==========
// dummy hooks
static void nlist_readhook(t_pd *z, t_symbol *fn, int ac, t_atom *av){
    (void)z, (void)fn, (void)ac, (void)av; // Dummy read hook
}
static void nlist_writehook(t_pd *z, t_symbol *fn, int ac, t_atom *av){
    (void)z, (void)fn, (void)ac, (void)av; // Dummy write hook
}
/*// For anonymous objects, this stores the keep flag in the object
// For named objects, it stores it in the common
static void nlist_keephook(t_pd *z, t_binbuf *bb, t_symbol *bindsym){
//    t_nlist *x = (t_nlist *)z;
//    t_nlistcommon *cc = x->x_common;
    (void)z, (void)bb, (void)bindsym; // DUMMY!!!!!
    // The keep flag is already set in the object or common
    // This hook is only needed for the embed mechanism
    // This is still dummy
}*/

// For anonymous (non-named) objects: same job as nlistcommon_editorhook,
// but operating directly on the object's own x_root instead of a common's c_root.
// Without a real hook here, elsefile has no way to reconcile the editor's
// contents against our data, so it can't clear the dirty state properly and
// ends up asking to save even when nothing was touched.
static void nlist_editorhook(t_pd *z, t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nlist *x = (t_nlist *)z;
    if(x->x_root){
        nlist_clear_nodes(x->x_root);
        x->x_root = NULL;
    }
    if(ac)
        x->x_root = nlist_parse_all(x, ac, av);
    if(x->x_keep && x->x_canvas)
        canvas_dirty(x->x_canvas, 1);
    if(x->x_is_opened)
        nlist_do_update(x);
}

// ----------- INIT / FREE / NEW / SETUP

static void nlist_init(t_nlist *x, t_symbol *name){
    x->x_name = name;
    x->x_root = NULL;
    if(!x->x_filehandle){
        x->x_filehandle = elsefile_new((t_pd *)x, NULL,
            nlist_readhook, nlist_writehook, nlist_editorhook);
    }
}

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

static void *nlist_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nlist *x = (t_nlist *)pd_new(nlist_class);
    x->x_canvas = canvas_getcurrent();
    x->x_keep = 0;
    x->x_is_opened = 0;
    x->x_filehandle = NULL;
    x->x_root = NULL;
    x->x_len = 0;
    x->x_depth = 0;
    x->x_name = NULL;
    x->x_next = NULL;
    char buf[MAXPDSTRING];
    sprintf(buf, "#%lx", (long)x);
    pd_bind(&x->x_obj.ob_pd, x->x_bindsym = gensym(buf));
    t_symbol *name = NULL;
    while(ac > 0 && av->a_type == A_SYMBOL){
        t_symbol *cursym = atom_getsymbol(av);
        if(cursym == gensym("-k")){
            x->x_keep = 1;
            ac--; av++;
        }
        else if(!name){
            name = cursym;
            ac--; av++;
        }
        else
            break;
    }
    nlist_init(x, name);
    if(name)
        pd_bind(&x->x_obj.ob_pd, name);
    return(x);
}

void setup_nlist0x2edefine(void){
    nlist_class = class_new(gensym("nlist.define"), (t_newmethod)(void*)nlist_new,
        (t_method)nlist_free, sizeof(t_nlist), CLASS_DEFAULT, A_GIMME, 0);
    class_addlist(nlist_class, nlist_list);
    class_addmethod(nlist_class, (t_method)nlist_show, gensym("show"), 0);
    class_addmethod(nlist_class, (t_method)nlist_hide, gensym("hide"), 0);
    class_addmethod(nlist_class, (t_method)nlist_clear, gensym("clear"), 0);
    class_addmethod(nlist_class, (t_method)nlist_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(nlist_class, (t_method)nlist_is_opened, gensym("_is_opened"),
        A_FLOAT, 0);
    class_addmethod(nlist_class, (t_method)nlist_keep, gensym("keep"), A_FLOAT, 0);
    elsefile_setup(nlist_class, 0);
}

void nlist_setup(void){
    nlist_class = class_new(gensym("nlist"), (t_newmethod)(void*)nlist_new,
        (t_method)nlist_free, sizeof(t_nlist), CLASS_DEFAULT, A_GIMME, 0);
    class_addlist(nlist_class, nlist_list);
    class_addmethod(nlist_class, (t_method)nlist_show, gensym("show"), 0);
    class_addmethod(nlist_class, (t_method)nlist_hide, gensym("hide"), 0);
    class_addmethod(nlist_class, (t_method)nlist_clear, gensym("clear"), 0);
    class_addmethod(nlist_class, (t_method)nlist_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(nlist_class, (t_method)nlist_is_opened, gensym("_is_opened"),
        A_FLOAT, 0);
    class_addmethod(nlist_class, (t_method)nlist_keep, gensym("keep"), A_FLOAT, 0);
    elsefile_setup(nlist_class, 0);
    class_sethelpsymbol(nlist_class, gensym("nlist.define"));
}
