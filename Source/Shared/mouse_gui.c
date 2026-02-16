#include <stdio.h>
#include <string.h>
#include "m_pd.h"
#include "g_canvas.h"
#include "mouse_gui.h"

static t_class *mouse_gui_class = 0;
static t_mouse_gui *mouse_gui_sink = 0;
static t_symbol *ps_hashmouse_gui;
static t_symbol *ps__mouse_gui;
static t_symbol *ps__up;
static t_symbol *ps__focus;
static t_symbol *ps__vised;
static t_symbol *ps__wheel;

static void mouse_gui_anything(void){ // dummy
}

// filtering out redundant "_up" messages
static void mouse_gui__up(t_mouse_gui *snk, t_floatarg f){
    if(!snk->g_psmouse){
        bug("mouse_gui__up");
        return;
    }
    if((int)f){
        if(!snk->g_isup){
            snk->g_isup = 1;
            if(snk->g_psmouse->s_thing){
                t_atom at;
                SETFLOAT(&at, 1);
                pd_typedmess(snk->g_psmouse->s_thing, ps__up, 1, &at);
            }
        }
    }
    else{
        if(snk->g_isup)
            snk->g_isup = 0;
        if(snk->g_psmouse->s_thing){
            t_atom at;
            SETFLOAT(&at, 0);
            pd_typedmess(snk->g_psmouse->s_thing, ps__up, 1, &at);
        }
    }
}

static void mouse_gui__wheel(t_mouse_gui *snk, t_floatarg delta, t_floatarg h){
    if(!snk->g_psmouse){
        bug("mouse_gui__wheel");
        return;
    }
    if(snk->g_psmouse->s_thing){
        t_atom at[2];
        SETFLOAT(&at[0], delta);
        SETFLOAT(&at[1], h);
        pd_typedmess(snk->g_psmouse->s_thing, ps__wheel, 2, at);
    }
}

static void mouse_gui__focus(t_mouse_gui *snk, t_symbol *s, t_floatarg f){
    if(!snk->g_psfocus){
        bug("mouse_gui__focus");
        return;
    }
    if(snk->g_psfocus->s_thing){
        t_atom at[2];
        SETSYMBOL(&at[0], s);
        SETFLOAT(&at[1], f);
        pd_typedmess(snk->g_psfocus->s_thing, ps__focus, 2, at);
    }
}

static void mouse_gui__vised(t_mouse_gui *snk, t_symbol *s, t_floatarg f){
    if(!snk->g_psvised){
        bug("mouse_gui__vised");
        return;
    }
    if(snk->g_psvised->s_thing){
        t_atom at[2];
        SETSYMBOL(&at[0], s);
        SETFLOAT(&at[1], f);
        pd_typedmess(snk->g_psvised->s_thing, ps__vised, 2, at);
    }
}

static void mouse_gui_dobindmouse(t_mouse_gui *snk){
#if 0
// How to be notified about changes of button state, prior to gui objects
// in a canvas?  LATER find a reliable way -- delete if failed
    sys_vgui("bind mouse_tag <<mouse_down>> {pdsend {%s _up 0}}\n", snk->g_psgui->s_name);
    sys_vgui("bind mouse_tag <<mouse_up>> {pdsend {%s _up 1}}\n", snk->g_psgui->s_name);
#endif
    sys_vgui("bind all <<mouse_down>> {pdsend {%s _up 0}}\n", snk->g_psgui->s_name);
    sys_vgui("bind all <<mouse_up>> {pdsend {%s _up 1}}\n", snk->g_psgui->s_name);
    // bind mousewheel events
    sys_vgui("bind all <<mouse_wheel_v>> {pdsend {%s _wheel %%D 0}}\n", snk->g_psgui->s_name);
    sys_vgui("bind all <<mouse_wheel_h>> {pdsend {%s _wheel %%D 1}}\n", snk->g_psgui->s_name);
}

static void mouse_gui__remouse(t_mouse_gui *snk){
    if(!snk->g_psmouse){
        bug("mouse_gui__remouse");
        return;
    }
    if(snk->g_psmouse->s_thing){
        /* if a new master was bound in a gray period, we need to
         restore gui bindings */
#if 0
        post("rebinding mouse...");
#endif
        mouse_gui_dobindmouse(snk);
    }
}

static void mouse_gui_dobindfocus(t_mouse_gui *snk){
    const char *script =
        "bind Canvas <<mouse_focusin>> {if {[mouse_gui_ispatcher %%W]} {pdsend {%s _focus %%W 1}}}\n"
        "bind Canvas <<mouse_focusout>> {if {[mouse_gui_ispatcher %%W]} {pdsend {%s _focus %%W 0}}}\n";

    pdgui_vmess(script, snk->g_psgui->s_name, snk->g_psgui->s_name);
}

static void mouse_gui__refocus(t_mouse_gui *snk){
    if(!snk->g_psfocus){
        bug("mouse_gui__refocus");
        return;
    }
    if(snk->g_psfocus->s_thing){
        /* if a new master was bound in a gray period, we need to
         restore gui bindings */
#if 0
        post("rebinding focus...");
#endif
        mouse_gui_dobindfocus(snk);
    }
}

static void mouse_gui_dobindvised(t_mouse_gui *snk){
    sys_vgui("bind Canvas <<mouse_vised>> \
             {if {[mouse_gui_ispatcher %%W]} \
             {pdsend {%s _vised %%W 1}}}\n", snk->g_psgui->s_name);
    sys_vgui("bind Canvas <<mouse_unvised>> \
             {if {[mouse_gui_ispatcher %%W]} \
             {pdsend {%s _vised %%W 0}}}\n", snk->g_psgui->s_name);
}

static void mouse_gui__revised(t_mouse_gui *snk){
    if(!snk->g_psvised){
        bug("mouse_gui__revised");
        return;
    }
    if(snk->g_psvised->s_thing){
        /* if a new master was bound in a gray period, we need to
         restore gui bindings */
#if 0
        post("rebinding vised events...");
#endif
        mouse_gui_dobindvised(snk);
    }
}

static int mouse_gui_setup(void){
    ps_hashmouse_gui = gensym("#mouse_gui");
    ps__mouse_gui = gensym("_mouse_gui");
    ps__up = gensym("_up");
    ps__focus = gensym("_focus");
    ps__vised = gensym("_vised");
    ps__wheel = gensym("_wheel");
    if(ps_hashmouse_gui->s_thing){
        if(strcmp(class_getname(*ps_hashmouse_gui->s_thing), ps__mouse_gui->s_name)){
            /* FIXME protect against the danger of someone else
             (e.g. receive) binding to #mouse_gui */
            bug("mouse_gui_setup");
            return(0);
        }
        else{ // FIXME compatibility test
            mouse_gui_class = *ps_hashmouse_gui->s_thing;
            return(1);
        }
    }
    mouse_gui_class = class_new(ps__mouse_gui, 0, 0, sizeof(t_mouse_gui),
        CLASS_PD | CLASS_NOINLET, 0);
    class_addanything(mouse_gui_class, mouse_gui_anything);
    class_addmethod(mouse_gui_class, (t_method)mouse_gui__remouse, gensym("_remouse"), 0);
    class_addmethod(mouse_gui_class, (t_method)mouse_gui__refocus, gensym("_refocus"), 0);
    class_addmethod(mouse_gui_class, (t_method)mouse_gui__revised, gensym("_revised"), 0);
    class_addmethod(mouse_gui_class, (t_method)mouse_gui__up, ps__up, A_FLOAT, 0);
    class_addmethod(mouse_gui_class, (t_method)mouse_gui__focus, ps__focus, A_SYMBOL, A_FLOAT, 0);
    class_addmethod(mouse_gui_class, (t_method)mouse_gui__vised, ps__vised, A_SYMBOL, A_FLOAT, 0);
    class_addmethod(mouse_gui_class, (t_method)mouse_gui__wheel, ps__wheel, A_FLOAT, A_FLOAT, 0);
    /* Protect against pdCmd being called (via "Canvas <Destroy>" binding)
     during Tcl_Finalize().  FIXME this should be a standard exit handler. */
    const char *script =
        "proc mouse_gui_exithook {cmd op} {proc ::pdsend {} {}}\n"
        "if {[info tclversion] >= 8.4} {\n"
        "    trace add execution exit enter mouse_gui_exithook\n"
        "}\n"
        "proc mouse_gui_ispatcher {cv} {\n"
        "    if {[string range $cv 0 1] == \".x\" && [string range $cv end-1 end] == \".c\"} {\n"
        "        return 1\n"
        "    } else {return 0}\n"
        "}\n"
        "proc mouse_gui_remouse {} {\n"
        "    bind all <<mouse_down>> {}\n"
        "    bind all <<mouse_up>> {}\n"
        "    bind all <<mouse_wheel_v>> {}\n"
        "    bind all <<mouse_wheel_h>> {}\n"
        "    pdsend {#mouse_gui _remouse}\n"
        "}\n"
        "proc mouse_gui_getscreen {} {\n"
        "    set px [winfo pointerx .]\n"
        "    set py [winfo pointery .]\n"
        "    pdsend \"#mouse_mouse _getscreen $px $py\"\n"
        "}\n"
        "proc mouse_gui_getscreenfocused {} {\n"
        "    set px [winfo pointerx .]\n"
        "    set py [winfo pointery .]\n"
        "    set wx [winfo x $::focused_window]\n"
        "    set wy [winfo y $::focused_window]\n"
        "    pdsend \"#mouse_mouse _getscreenfocused $px $py $wx $wy\"\n"
        "}\n"
        "global mouse_gui_ispolling mouse_gui_px mouse_gui_py mouse_gui_wx mouse_gui_wy\n"
        "set mouse_gui_ispolling 0\n"
        "set mouse_gui_px 0\n"
        "set mouse_gui_py 0\n"
        "set mouse_gui_wx 0\n"
        "set mouse_gui_wy 0\n"
        "proc mouse_gui_poll {} {\n"
        "    global mouse_gui_ispolling mouse_gui_px mouse_gui_py mouse_gui_wx mouse_gui_wy\n"
        "    if {$mouse_gui_ispolling > 0} {\n"
        "        set px [winfo pointerx .]\n"
        "        set py [winfo pointery .]\n"
        "        if {$mouse_gui_ispolling <= 2} {\n"
        "            if {$mouse_gui_px != $px || $mouse_gui_py != $py} {\n"
        "                pdsend \"#mouse_mouse _getscreen $px $py\"\n"
        "                set mouse_gui_px $px\n"
        "                set mouse_gui_py $py\n"
        "            }\n"
        "        } elseif {$mouse_gui_ispolling == 3} {\n"
        "            set wx [winfo x $::focused_window]\n"
        "            set wy [winfo y $::focused_window]\n"
        "            if {$mouse_gui_px != $px || $mouse_gui_py != $py || $mouse_gui_wx != $wx || $mouse_gui_wy != $wy} {\n"
        "                pdsend \"#mouse_mouse _getscreenfocused $px $py $wx $wy\"\n"
        "                set mouse_gui_px $px\n"
        "                set mouse_gui_py $py\n"
        "                set mouse_gui_wx $wx\n"
        "                set mouse_gui_wy $wy\n"
        "            }\n"
        "        }\n"
        "        after 50 mouse_gui_poll\n"
        "    }\n"
        "}\n"
        "proc mouse_gui_refocus {} {\n"
        "    bind Canvas <<mouse_focusin>> {}\n"
        "    bind Canvas <<mouse_focusout>> {}\n"
        "    pdsend {#mouse_gui _refocus}\n"
        "}\n"
        "proc mouse_gui_revised {} {\n"
        "    bind Canvas <<mouse_vised>> {}\n"
        "    bind Canvas <<mouse_unvised>> {}\n"
        "    pdsend {#mouse_gui _revised}\n"
        "}\n";
    pdgui_vmess(script, NULL);
    return(1);
}

static int mouse_gui_validate(int dosetup){
    if(dosetup && !mouse_gui_sink
    && (mouse_gui_class || mouse_gui_setup())){
        if(ps_hashmouse_gui->s_thing)
            mouse_gui_sink = (t_mouse_gui *)ps_hashmouse_gui->s_thing;
        else{
            mouse_gui_sink = (t_mouse_gui *)pd_new(mouse_gui_class);
            mouse_gui_sink->g_psgui = ps_hashmouse_gui;
            pd_bind((t_pd *)mouse_gui_sink,
                    ps_hashmouse_gui);  /* never unbound */
        }
    }
    if(mouse_gui_class && mouse_gui_sink)
        return(1);
    else{
        bug("mouse_gui_validate");
        return(0);
    }
}

static int mouse_gui_mousevalidate(int dosetup){
    if(dosetup && !mouse_gui_sink->g_psmouse){
        mouse_gui_sink->g_psmouse = gensym("#mouse_mouse");
        
        sys_gui("event add <<mouse_down>> <ButtonPress>\n");
        sys_gui("event add <<mouse_up>> <ButtonRelease>\n");
    // Separate vertical and horizontal wheel events
        sys_gui("event add <<mouse_wheel_v>> <MouseWheel>\n");
        sys_gui("event add <<mouse_wheel_h>> <Shift-MouseWheel>\n");
          
    // For X11 systems
        sys_gui("event add <<mouse_wheel_v>> <Button-4>\n");
        sys_gui("event add <<mouse_wheel_v>> <Button-5>\n");
        sys_gui("event add <<mouse_wheel_h>> <Shift-Button-4>\n");
        sys_gui("event add <<mouse_wheel_h>> <Shift-Button-5>\n");
    }
    if(mouse_gui_sink->g_psmouse)
        return(1);
    else{
        bug("mouse_gui_mousevalidate");
        return(0);
    }
}

static int mouse_gui_pollvalidate(int dosetup){
    if(dosetup && !mouse_gui_sink->g_pspoll){
        mouse_gui_sink->g_pspoll = gensym("#mouse_poll");
        pd_bind((t_pd *)mouse_gui_sink,
                mouse_gui_sink->g_pspoll);  /* never unbound */
    }
    if(mouse_gui_sink->g_pspoll)
        return(1);
    else{
        bug("mouse_gui_pollvalidate");
        return(0);
    }
}

static int mouse_gui_focusvalidate(int dosetup){
    if(dosetup && !mouse_gui_sink->g_psfocus){
        mouse_gui_sink->g_psfocus = gensym("#mouse_focus");
        sys_gui("event add <<mouse_focusin>> <FocusIn>\n");
        sys_gui("event add <<mouse_focusout>> <FocusOut>\n");
    }
    if(mouse_gui_sink->g_psfocus)
        return(1);
    else{
        bug("mouse_gui_focusvalidate");
        return(0);
    }
}

static int mouse_gui_visedvalidate(int dosetup){
    if(dosetup && !mouse_gui_sink->g_psvised){
        mouse_gui_sink->g_psvised = gensym("#mouse_vised");
        /* subsequent map events have to be filtered out at the caller's side,
         LATER investigate */
        sys_gui("event add <<mouse_vised>> <Map>\n");
        sys_gui("event add <<mouse_unvised>> <Destroy>\n");
    }
    if(mouse_gui_sink->g_psvised)
        return(1);
    else{
        bug("mouse_gui_visedvalidate");
        return(0);
    }
}

void mouse_gui_bindmouse(t_pd *master){
    mouse_gui_validate(1);
    mouse_gui_mousevalidate(1);
    if(!mouse_gui_sink->g_psmouse->s_thing)
        mouse_gui_dobindmouse(mouse_gui_sink);
    pd_bind(master, mouse_gui_sink->g_psmouse);
}

void mouse_gui_unbindmouse(t_pd *master){
    if(mouse_gui_validate(0) && mouse_gui_mousevalidate(0)
    && mouse_gui_sink->g_psmouse->s_thing){
        pd_unbind(master, mouse_gui_sink->g_psmouse);
        if(!mouse_gui_sink->g_psmouse->s_thing)
            sys_gui("mouse_gui_remouse\n");
    }
    else
        bug("mouse_gui_unbindmouse");
}

void mouse_gui_getscreenfocused(void){
    if(mouse_gui_validate(0))
        sys_gui("mouse_gui_getscreenfocused\n");
}

void mouse_gui_getscreen(void){
    if(mouse_gui_validate(0))
        sys_gui("mouse_gui_getscreen\n");
}

void mouse_gui_willpoll(void){
    mouse_gui_validate(1);
    mouse_gui_pollvalidate(1);
}

void mouse_gui_startpolling(t_pd *master, int pollmode){
    if(mouse_gui_validate(0) && mouse_gui_pollvalidate(0)){
        int doinit =
        (mouse_gui_sink->g_pspoll->s_thing == (t_pd *)mouse_gui_sink);
        pd_bind(master, mouse_gui_sink->g_pspoll);
        if(doinit){ // visibility hack for msw, LATER rethink
            sys_gui("global mouse_gui_ispolling\n");
            sys_vgui("set mouse_gui_ispolling %d\n", pollmode);
            sys_gui("mouse_gui_poll\n");
        }
    }
}

void mouse_gui_stoppolling(t_pd *master){
    if(mouse_gui_validate(0) && mouse_gui_pollvalidate(0)){
        pd_unbind(master, mouse_gui_sink->g_pspoll);
        if(mouse_gui_sink->g_pspoll->s_thing == (t_pd *)mouse_gui_sink){
            sys_gui("global mouse_gui_ispolling\n");
            sys_gui("set mouse_gui_ispolling 0\n");
            sys_vgui("after cancel [mouse_gui_poll]\n");
            /* visibility hack for msw, LATER rethink */
        }
    }
}

void mouse_gui_bindfocus(t_pd *master){
    mouse_gui_validate(1);
    mouse_gui_focusvalidate(1);
    if(!mouse_gui_sink->g_psfocus->s_thing)
        mouse_gui_dobindfocus(mouse_gui_sink);
    pd_bind(master, mouse_gui_sink->g_psfocus);
}

void mouse_gui_unbindfocus(t_pd *master){
    if(mouse_gui_validate(0) && mouse_gui_focusvalidate(0) && mouse_gui_sink->g_psfocus->s_thing){
        pd_unbind(master, mouse_gui_sink->g_psfocus);
        if(!mouse_gui_sink->g_psfocus->s_thing)
            sys_gui("mouse_gui_refocus\n");
    }
    else
        bug("mouse_gui_unbindfocus");
}

void mouse_gui_bindvised(t_pd *master){
    mouse_gui_validate(1);
    mouse_gui_visedvalidate(1);
    if(!mouse_gui_sink->g_psvised->s_thing)
        mouse_gui_dobindvised(mouse_gui_sink);
    pd_bind(master, mouse_gui_sink->g_psvised);
}

void mouse_gui_unbindvised(t_pd *master){
    if(mouse_gui_validate(0) && mouse_gui_visedvalidate(0)
    && mouse_gui_sink->g_psvised->s_thing){
        pd_unbind(master, mouse_gui_sink->g_psvised);
        if(!mouse_gui_sink->g_psvised->s_thing)
            sys_gui("mouse_gui_revised\n");
    }
    else
        bug("mouse_gui_unbindvised"); // bug?????
}
