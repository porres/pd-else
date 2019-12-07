// based on cyclone's active

#include "m_pd.h"
#include "g_canvas.h"
#include <string.h>

typedef struct _active_gui{
    t_pd       g_pd;
    t_symbol  *g_psgui;
    t_symbol  *g_psfocus;
}t_active_gui;

static t_class *active_gui_class;
static t_active_gui *gui_sink = 0;
static t_symbol *ps_hashactive_gui;
static t_symbol *ps__active_gui;
static t_symbol *ps__focus;

static void active_gui_anything(void){
} // Dummy

static void active_gui__focus(t_active_gui *snk, t_symbol *s, t_floatarg f){
    if(!snk->g_psfocus) // bug("active_gui__focus");
        return;
    if(snk->g_psfocus->s_thing){
        t_atom at[2];
        SETSYMBOL(&at[0], s);
        SETFLOAT(&at[1], f);
        pd_typedmess(snk->g_psfocus->s_thing, ps__focus, 2, at);
    }
}

static void gui_dobindfocus(t_active_gui *snk){
    sys_vgui("bind Canvas <<active_focusin>> \
             {if {[active_gui_ispatcher %%W]} \
             {pdsend {%s _focus %%W 1}}}\n", snk->g_psgui->s_name);
    sys_vgui("bind Canvas <<active_focusout>> \
             {if {[active_gui_ispatcher %%W]} \
             {pdsend {%s _focus %%W 0}}}\n", snk->g_psgui->s_name);
}

static void active_gui__refocus(t_active_gui *snk){
    if(!snk->g_psfocus) // bug("active_gui__refocus");
        return;
    if(snk->g_psfocus->s_thing) // if new master bound in gray period, restore gui bindings
        gui_dobindfocus(snk);
}

static int active_gui_setup(void){
    ps_hashactive_gui = gensym("#active_gui");
    ps__active_gui = gensym("_active_gui");
    ps__focus = gensym("_focus");
    if(ps_hashactive_gui->s_thing){
        if(strcmp(class_getname(*ps_hashactive_gui->s_thing), ps__active_gui->s_name))
            // bug("active_gui_setup"); // avoid something (e.g. receive) bind to #active_gui
            return (0);
        else{ // FIXME compatibility test
            active_gui_class = *ps_hashactive_gui->s_thing;
            return(1);
        }
    }
    active_gui_class = class_new(ps__active_gui, 0, 0,
        sizeof(t_active_gui), CLASS_PD | CLASS_NOINLET, 0);
    class_addanything(active_gui_class, active_gui_anything);
    class_addmethod(active_gui_class, (t_method)active_gui__refocus,
        gensym("_refocus"), 0);
    class_addmethod(active_gui_class, (t_method)active_gui__focus,
        ps__focus, A_SYMBOL, A_FLOAT, 0);
    
    /* Protect against pdCmd being called (via "Canvas <Destroy>" binding)
     during Tcl_Finalize().  FIXME this should be a standard exit handler. */
    sys_gui("proc active_gui_exithook {cmd op} {proc ::pdsend {} {}}\n");
    sys_gui("if {[info tclversion] >= 8.4} {\n\
            trace add execution exit enter active_gui_exithook}\n");
    
    sys_gui("proc active_gui_ispatcher {cv} {\n");
    sys_gui(" if {[string range $cv 0 1] == \".x\"");
    sys_gui("  && [string range $cv end-1 end] == \".c\"} {\n");
    sys_gui("  return 1} else {return 0}\n");
    sys_gui("}\n");
    
    sys_gui("proc active_gui_getscreen {} {\n");
    sys_gui(" set px [winfo pointerx .]\n");
    sys_gui(" set py [winfo pointery .]\n");
    sys_gui(" pdsend \"#active_mouse _getscreen $px $py\"\n");
    sys_gui("}\n");
    
    sys_gui("proc active_gui_getscreenfocused {} {\n");
    sys_gui(" set px [winfo pointerx .]\n");
    sys_gui(" set py [winfo pointery . ]\n");
    sys_gui(" set wx [winfo x $::focused_active]\n");
    sys_gui(" set wy [winfo y $::focused_active]\n");
    sys_gui(" pdsend \"#active_mouse _getscreenfocused ");
    sys_gui("$px $py $wx $wy\"\n");
    sys_gui("}\n");
    
    sys_gui("proc active_gui_refocus {} {\n");
    sys_gui(" bind Canvas <<active_focusin>> {}\n");
    sys_gui(" bind Canvas <<active_focusout>> {}\n");
    sys_gui(" pdsend {#active_gui _refocus}\n");
    sys_gui("}\n");
    
    return(1);
}

static int gui_validate(int dosetup){
    if(dosetup && !gui_sink && (active_gui_class || active_gui_setup())){
        if(ps_hashactive_gui->s_thing)
            gui_sink = (t_active_gui *)ps_hashactive_gui->s_thing;
        else{
            gui_sink = (t_active_gui *)pd_new(active_gui_class);
            gui_sink->g_psgui = ps_hashactive_gui;
            pd_bind((t_pd *)gui_sink, ps_hashactive_gui); // never unbound
        }
    }
    if(active_gui_class && gui_sink)
        return(1);
    else // bug("gui_validate");
        return(0);
}

static int gui_focusvalidate(int dosetup){
    if(dosetup && !gui_sink->g_psfocus){
        gui_sink->g_psfocus = gensym("#active_focus");
        sys_gui("event add <<active_focusin>> <FocusIn>\n");
        sys_gui("event add <<active_focusout>> <FocusOut>\n");
    }
    if(gui_sink->g_psfocus)
        return(1);
    else // bug("gui_focusvalidate");
        return(0);
}

void active_gui_getscreenfocused(void){
    if(gui_validate(0))
        sys_gui("active_gui_getscreenfocused\n");
}

void active_gui_getscreen(void){
    if(gui_validate(0))
        sys_gui("active_gui_getscreen\n");
}

/////////////  CLASS!!!

typedef struct _active{
    t_object   x_obj;
    t_symbol  *x_cname;
}t_active;

static t_class *active_class;

static void active_dofocus(t_active *x, t_symbol *s, t_floatarg f){
    if(s == x->x_cname)
        outlet_float(x->x_obj.ob_outlet, f);
}

static void active_free(t_active *x){ // unbind focus
    if(gui_validate(0) && gui_focusvalidate(0) && gui_sink->g_psfocus->s_thing){
        pd_unbind((t_pd *)x, gui_sink->g_psfocus);
        if(!gui_sink->g_psfocus->s_thing)
            sys_gui("active_gui_refocus\n");
    }
}

static void *active_new(t_floatarg f){
    t_active *x = (t_active *)pd_new(active_class);
    t_glist *glist = (t_glist *)canvas_getcurrent();
    t_canvas *cnv = (t_canvas*)glist_getcanvas(glist);
    int depth = (int)f < 0 ? 0 : (int)f;
    while(depth-- && cnv)
        cnv = cnv->gl_owner;
    char buf[32];
    sprintf(buf, ".x%lx.c", (unsigned long)cnv);
    x->x_cname = gensym(buf);
    outlet_new((t_object *)x, &s_float);
// bind focus
    gui_validate(1);
    gui_focusvalidate(1);
    if(!gui_sink->g_psfocus->s_thing)
        gui_dobindfocus(gui_sink);
    pd_bind((t_pd *)x, gui_sink->g_psfocus);
    return(x);
}

void setup_canvas0x2eactive(void){
    active_class = class_new(gensym("canvas.active"), (t_newmethod)active_new,
        (t_method)active_free, sizeof(t_active), CLASS_NOINLET, A_DEFFLOAT, 0);
    class_addmethod(active_class, (t_method)active_dofocus, gensym("_focus"), A_SYMBOL, A_FLOAT, 0);
}
