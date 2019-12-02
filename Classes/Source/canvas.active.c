// based on cyclone's active

#include "m_pd.h"
#include "g_canvas.h"
#include <stdio.h>
#include <string.h>

typedef struct _canvas_active_gui{
    t_pd       g_pd;
    t_symbol  *g_psgui;
    t_symbol  *g_psfocus;
}t_canvas_active_gui;

static t_class *canvas_active_gui_class;
static t_canvas_active_gui *canvas_active_gui_sink = 0;
static t_symbol *ps_hashcanvas_active_gui;
static t_symbol *ps__canvas_active_gui;
static t_symbol *ps__focus;

static void canvas_active_gui_anything(void){
} // Dummy

static void canvas_active_gui__focus(t_canvas_active_gui *snk, t_symbol *s, t_floatarg f){
    if(!snk->g_psfocus) // bug("canvas_active_gui__focus");
        return;
    if(snk->g_psfocus->s_thing){
        t_atom at[2];
        SETSYMBOL(&at[0], s);
        SETFLOAT(&at[1], f);
        pd_typedmess(snk->g_psfocus->s_thing, ps__focus, 2, at);
    }
}

static void canvas_active_gui_dobindfocus(t_canvas_active_gui *snk){
    sys_vgui("bind Canvas <<canvas_active_focusin>> \
             {if {[canvas_active_gui_ispatcher %%W]} \
             {pdsend {%s _focus %%W 1}}}\n", snk->g_psgui->s_name);
    sys_vgui("bind Canvas <<canvas_active_focusout>> \
             {if {[canvas_active_gui_ispatcher %%W]} \
             {pdsend {%s _focus %%W 0}}}\n", snk->g_psgui->s_name);
}

static void canvas_active_gui__refocus(t_canvas_active_gui *snk){
    if(!snk->g_psfocus) // bug("canvas_active_gui__refocus");
        return;
    if(snk->g_psfocus->s_thing) // if new master bound in gray period, restore gui bindings
        canvas_active_gui_dobindfocus(snk);
}

static int canvas_active_gui_setup(void){
    ps_hashcanvas_active_gui = gensym("#canvas_active_gui");
    ps__canvas_active_gui = gensym("_canvas_active_gui");
    ps__focus = gensym("_focus");
    if (ps_hashcanvas_active_gui->s_thing){
        if(strcmp(class_getname(*ps_hashcanvas_active_gui->s_thing), ps__canvas_active_gui->s_name))
            // bug("canvas_active_gui_setup"); // avoid something (e.g. receive) bind to #canvas_active_gui
            return (0);
        else{ // FIXME compatibility test
            canvas_active_gui_class = *ps_hashcanvas_active_gui->s_thing;
            return (1);
        }
    }
    canvas_active_gui_class = class_new(ps__canvas_active_gui, 0, 0,
                    sizeof(t_canvas_active_gui), CLASS_PD | CLASS_NOINLET, 0);
    class_addanything(canvas_active_gui_class, canvas_active_gui_anything);
    class_addmethod(canvas_active_gui_class, (t_method)canvas_active_gui__refocus,
                    gensym("_refocus"), 0);
    class_addmethod(canvas_active_gui_class, (t_method)canvas_active_gui__focus,
                    ps__focus, A_SYMBOL, A_FLOAT, 0);
    
    /* Protect against pdCmd being called (via "Canvas <Destroy>" binding)
     during Tcl_Finalize().  FIXME this should be a standard exit handler. */
    sys_gui("proc canvas_active_gui_exithook {cmd op} {proc ::pdsend {} {}}\n");
    sys_gui("if {[info tclversion] >= 8.4} {\n\
            trace add execution exit enter canvas_active_gui_exithook}\n");
    
    sys_gui("proc canvas_active_gui_ispatcher {cv} {\n");
    sys_gui(" if {[string range $cv 0 1] == \".x\"");
    sys_gui("  && [string range $cv end-1 end] == \".c\"} {\n");
    sys_gui("  return 1} else {return 0}\n");
    sys_gui("}\n");
    
    sys_gui("proc canvas_active_gui_getscreen {} {\n");
    sys_gui(" set px [winfo pointerx .]\n");
    sys_gui(" set py [winfo pointery .]\n");
    sys_gui(" pdsend \"#canvas_active_mouse _getscreen $px $py\"\n");
    sys_gui("}\n");
    
    sys_gui("proc canvas_active_gui_getscreenfocused {} {\n");
    sys_gui(" set px [winfo pointerx .]\n");
    sys_gui(" set py [winfo pointery . ]\n");
    sys_gui(" set wx [winfo x $::focused_canvas_active]\n");
    sys_gui(" set wy [winfo y $::focused_canvas_active]\n");
    sys_gui(" pdsend \"#canvas_active_mouse _getscreenfocused ");
    sys_gui("$px $py $wx $wy\"\n");
    sys_gui("}\n");
    
    sys_gui("proc canvas_active_gui_refocus {} {\n");
    sys_gui(" bind Canvas <<canvas_active_focusin>> {}\n");
    sys_gui(" bind Canvas <<canvas_active_focusout>> {}\n");
    sys_gui(" pdsend {#canvas_active_gui _refocus}\n");
    sys_gui("}\n");
    
    return(1);
}

static int canvas_active_gui_validate(int dosetup){
    if(dosetup && !canvas_active_gui_sink && (canvas_active_gui_class || canvas_active_gui_setup())){
        if(ps_hashcanvas_active_gui->s_thing)
            canvas_active_gui_sink = (t_canvas_active_gui *)ps_hashcanvas_active_gui->s_thing;
        else{
            canvas_active_gui_sink = (t_canvas_active_gui *)pd_new(canvas_active_gui_class);
            canvas_active_gui_sink->g_psgui = ps_hashcanvas_active_gui;
            pd_bind((t_pd *)canvas_active_gui_sink, ps_hashcanvas_active_gui); // never unbound
        }
    }
    if(canvas_active_gui_class && canvas_active_gui_sink)
        return(1);
    else // bug("canvas_active_gui_validate");
        return(0);
}

static int canvas_active_gui_focusvalidate(int dosetup){
    if(dosetup && !canvas_active_gui_sink->g_psfocus){
        canvas_active_gui_sink->g_psfocus = gensym("#canvas_active_focus");
        sys_gui("event add <<canvas_active_focusin>> <FocusIn>\n");
        sys_gui("event add <<canvas_active_focusout>> <FocusOut>\n");
    }
    if(canvas_active_gui_sink->g_psfocus)
        return(1);
    else // bug("canvas_active_gui_focusvalidate");
        return(0);
}

void canvas_active_gui_getscreenfocused(void){
    if(canvas_active_gui_validate(0))
        sys_gui("canvas_active_gui_getscreenfocused\n");
}

void canvas_active_gui_getscreen(void){
    if(canvas_active_gui_validate(0))
        sys_gui("canvas_active_gui_getscreen\n");
}

///////////// canvas_active CLASS!!!!!!! ////////////////

typedef struct _canvas_active{
    t_object   x_ob;
    t_symbol  *x_cnvame;
    t_canvas  *x_canvas;
    t_outlet  *x_outlet;
    int        x_on;
}t_canvas_active;

static t_class *canvas_active_class;

static void canvas_active_dofocus(t_canvas_active *x, t_symbol *s, t_floatarg f){
    if((int)f){
        int on = (s == x->x_cnvame);
        if(on != x->x_on){ // ???
            outlet_float(x->x_outlet, glist_isvisible(x->x_canvas));
            outlet_float(((t_object *)x)->ob_outlet, x->x_on = on);
        }
    }
    else if(x->x_on && s == x->x_cnvame){ // ???
            outlet_float(x->x_outlet, glist_isvisible(x->x_canvas));
            outlet_float(((t_object *)x)->ob_outlet, x->x_on = 0);
        }
}

void canvas_active_unbindfocus(t_pd *master){
    if(canvas_active_gui_validate(0) && canvas_active_gui_focusvalidate(0)
       && canvas_active_gui_sink->g_psfocus->s_thing){
        pd_unbind(master, canvas_active_gui_sink->g_psfocus);
        if(!canvas_active_gui_sink->g_psfocus->s_thing)
            sys_gui("canvas_active_gui_refocus\n");
    }
}

static void canvas_active_free(t_canvas_active *x){
    canvas_active_unbindfocus((t_pd *)x);
    outlet_free(x->x_outlet);
}

static void canvas_active_bang(t_canvas_active *x){
    outlet_float(x->x_outlet, glist_isvisible(x->x_canvas));
    outlet_float(((t_object *)x)->ob_outlet, x->x_on);
}

void canvas_active_gui_bindfocus(t_pd *master){
    canvas_active_gui_validate(1);
    canvas_active_gui_focusvalidate(1);
    if(!canvas_active_gui_sink->g_psfocus->s_thing)
        canvas_active_gui_dobindfocus(canvas_active_gui_sink);
    pd_bind(master, canvas_active_gui_sink->g_psfocus);
}

static void *canvas_active_new(t_floatarg f){
    t_canvas_active *x = (t_canvas_active *)pd_new(canvas_active_class);
    t_glist *glist=(t_glist *)canvas_getcurrent();
    t_canvas *canvas=(t_canvas*)glist_getcanvas(glist);
    int depth = (int)f;
    if(depth < 0)
        depth = 0;
    while(depth && canvas){
        canvas = canvas->gl_owner;
        depth--;
    }
    x->x_canvas = canvas;
    char buf[32];
    sprintf(buf, ".x%lx.c", (unsigned long)canvas);
    x->x_cnvame = gensym(buf);
    x->x_on = 0;
    outlet_new((t_object *)x, &s_float);
    x->x_outlet = outlet_new(&x->x_ob, 0);
    canvas_active_gui_bindfocus((t_pd *)x);
    return(x);
}

void setup_canvas0x2eactive(void){
    canvas_active_class = class_new(gensym("canvas.active"), (t_newmethod)canvas_active_new,
        (t_method)canvas_active_free, sizeof(t_canvas_active), 0, A_DEFFLOAT, 0);
    class_addbang(canvas_active_class, canvas_active_bang);
    class_addmethod(canvas_active_class, (t_method)canvas_active_dofocus, gensym("_focus"), A_SYMBOL, A_FLOAT, 0);
}
