// based on cyclone's active

#include "m_pd.h"
#include "g_canvas.h"
#include <stdio.h>
#include <string.h>

typedef struct _window_gui{
    t_pd       g_pd;
    t_symbol  *g_psgui;
    t_symbol  *g_psfocus;
}t_window_gui;

// static t_class *window_gui_class = 0;
static t_class *window_gui_class;
static t_window_gui *window_gui_sink = 0;
static t_symbol *ps_hashwindow_gui;
static t_symbol *ps__window_gui;
static t_symbol *ps__focus;

static void window_gui_anything(void){
} // Dummy

static void window_gui__focus(t_window_gui *snk, t_symbol *s, t_floatarg f){
    if(!snk->g_psfocus) // bug("window_gui__focus");
        return;
    if(snk->g_psfocus->s_thing){
        t_atom at[2];
        SETSYMBOL(&at[0], s);
        SETFLOAT(&at[1], f);
        pd_typedmess(snk->g_psfocus->s_thing, ps__focus, 2, at);
    }
}

static void window_gui_dobindfocus(t_window_gui *snk){
    sys_vgui("bind Canvas <<window_focusin>> \
             {if {[window_gui_ispatcher %%W]} \
             {pdsend {%s _focus %%W 1}}}\n", snk->g_psgui->s_name);
    sys_vgui("bind Canvas <<window_focusout>> \
             {if {[window_gui_ispatcher %%W]} \
             {pdsend {%s _focus %%W 0}}}\n", snk->g_psgui->s_name);
}

static void window_gui__refocus(t_window_gui *snk){
    if(!snk->g_psfocus) // bug("window_gui__refocus");
        return;
    if(snk->g_psfocus->s_thing) // if new master bound in gray period, restore gui bindings
        window_gui_dobindfocus(snk);
}

static int window_gui_setup(void){
    ps_hashwindow_gui = gensym("#window_gui");
    ps__window_gui = gensym("_window_gui");
    ps__focus = gensym("_focus");
    if (ps_hashwindow_gui->s_thing){
        if(strcmp(class_getname(*ps_hashwindow_gui->s_thing), ps__window_gui->s_name))
            // bug("window_gui_setup"); // avoid something (e.g. receive) bind to #window_gui
            return (0);
        else{ // FIXME compatibility test
            window_gui_class = *ps_hashwindow_gui->s_thing;
            return (1);
        }
    }
    window_gui_class = class_new(ps__window_gui, 0, 0,
                    sizeof(t_window_gui), CLASS_PD | CLASS_NOINLET, 0);
    class_addanything(window_gui_class, window_gui_anything);
    class_addmethod(window_gui_class, (t_method)window_gui__refocus,
                    gensym("_refocus"), 0);
    class_addmethod(window_gui_class, (t_method)window_gui__focus,
                    ps__focus, A_SYMBOL, A_FLOAT, 0);
    
    /* Protect against pdCmd being called (via "Canvas <Destroy>" binding)
     during Tcl_Finalize().  FIXME this should be a standard exit handler. */
    sys_gui("proc window_gui_exithook {cmd op} {proc ::pdsend {} {}}\n");
    sys_gui("if {[info tclversion] >= 8.4} {\n\
            trace add execution exit enter window_gui_exithook}\n");
    
    sys_gui("proc window_gui_ispatcher {cv} {\n");
    sys_gui(" if {[string range $cv 0 1] == \".x\"");
    sys_gui("  && [string range $cv end-1 end] == \".c\"} {\n");
    sys_gui("  return 1} else {return 0}\n");
    sys_gui("}\n");
    
    sys_gui("proc window_gui_getscreen {} {\n");
    sys_gui(" set px [winfo pointerx .]\n");
    sys_gui(" set py [winfo pointery .]\n");
    sys_gui(" pdsend \"#window_mouse _getscreen $px $py\"\n");
    sys_gui("}\n");
    
    sys_gui("proc window_gui_getscreenfocused {} {\n");
    sys_gui(" set px [winfo pointerx .]\n");
    sys_gui(" set py [winfo pointery . ]\n");
    sys_gui(" set wx [winfo x $::focused_window]\n");
    sys_gui(" set wy [winfo y $::focused_window]\n");
    sys_gui(" pdsend \"#window_mouse _getscreenfocused ");
    sys_gui("$px $py $wx $wy\"\n");
    sys_gui("}\n");
    
    sys_gui("proc window_gui_refocus {} {\n");
    sys_gui(" bind Canvas <<window_focusin>> {}\n");
    sys_gui(" bind Canvas <<window_focusout>> {}\n");
    sys_gui(" pdsend {#window_gui _refocus}\n");
    sys_gui("}\n");
    
    return(1);
}

static int window_gui_validate(int dosetup){
    if(dosetup && !window_gui_sink && (window_gui_class || window_gui_setup())){
        if(ps_hashwindow_gui->s_thing)
            window_gui_sink = (t_window_gui *)ps_hashwindow_gui->s_thing;
        else{
            window_gui_sink = (t_window_gui *)pd_new(window_gui_class);
            window_gui_sink->g_psgui = ps_hashwindow_gui;
            pd_bind((t_pd *)window_gui_sink, ps_hashwindow_gui); // never unbound
        }
    }
    if(window_gui_class && window_gui_sink)
        return(1);
    else // bug("window_gui_validate");
        return(0);
}

static int window_gui_focusvalidate(int dosetup){
    if(dosetup && !window_gui_sink->g_psfocus){
        window_gui_sink->g_psfocus = gensym("#window_focus");
        sys_gui("event add <<window_focusin>> <FocusIn>\n");
        sys_gui("event add <<window_focusout>> <FocusOut>\n");
    }
    if(window_gui_sink->g_psfocus)
        return(1);
    else // bug("window_gui_focusvalidate");
        return(0);
}

void window_gui_getscreenfocused(void){
    if(window_gui_validate(0))
        sys_gui("window_gui_getscreenfocused\n");
}

void window_gui_getscreen(void){
    if(window_gui_validate(0))
        sys_gui("window_gui_getscreen\n");
}

///////////// WINDOW CLASS!!!!!!! ////////////////

typedef struct _window{
    t_object   x_ob;
    t_symbol  *x_cnvame;
    int        x_on;
}t_window;

static t_class *window_class;

static void window_dofocus(t_window *x, t_symbol *s, t_floatarg f){
    if((int)f){
        int on = (s == x->x_cnvame);
        if(on != x->x_on) // ???
            outlet_float(((t_object *)x)->ob_outlet, x->x_on = on);
    }
    else
        if(x->x_on && s == x->x_cnvame) // ???
            outlet_float(((t_object *)x)->ob_outlet, x->x_on = 0);
}

void window_unbindfocus(t_pd *master){
    if(window_gui_validate(0) && window_gui_focusvalidate(0)
       && window_gui_sink->g_psfocus->s_thing){
        pd_unbind(master, window_gui_sink->g_psfocus);
        if(!window_gui_sink->g_psfocus->s_thing)
            sys_gui("window_gui_refocus\n");
    }
    // else bug("window_unbindfocus");
}

static void window_free(t_window *x){
    window_unbindfocus((t_pd *)x);
}

static void window_bang(t_window *x){
    outlet_float(((t_object *)x)->ob_outlet, x->x_on);
}

void window_gui_bindfocus(t_pd *master){
    window_gui_validate(1);
    window_gui_focusvalidate(1);
    if(!window_gui_sink->g_psfocus->s_thing)
        window_gui_dobindfocus(window_gui_sink);
    pd_bind(master, window_gui_sink->g_psfocus);
}

static void *window_new(t_floatarg f){
    t_window *x = (t_window *)pd_new(window_class);
    t_glist *glist=(t_glist *)canvas_getcurrent();
    t_canvas *canvas=(t_canvas*)glist_getcanvas(glist);
    int depth = (int)f;
    if(depth < 0)
        depth = 0;
    while(depth && canvas){
        canvas = canvas->gl_owner;
        depth--;
    }
    char buf[32];
    sprintf(buf, ".x%lx.c", (unsigned long)canvas);
    x->x_cnvame = gensym(buf);
    x->x_on = 0;
    outlet_new((t_object *)x, &s_float);
    window_gui_bindfocus((t_pd *)x);
    return(x);
}

void window_setup(void){
    window_class = class_new(gensym("window"), (t_newmethod)window_new,
        (t_method)window_free, sizeof(t_window), 0, A_DEFFLOAT, 0);
    class_addbang(window_class, window_bang);
    class_addmethod(window_class, (t_method)window_dofocus, gensym("_focus"), A_SYMBOL, A_FLOAT, 0);
}
