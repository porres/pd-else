// porres 2025

#include <m_pd.h>
#include <g_canvas.h>
#include <string.h>
#include <stdlib.h>

#define MAX_ITEMS 1024

#if __APPLE__
char def_font[100] = "Menlo";
#else
char def_font[100] = "DejaVu Sans Mono";
#endif

static t_class *menu_class, *edit_proxy_class;
static t_widgetbehavior menu_widgetbehavior;

typedef struct _edit_proxy{
    t_object      p_obj;
    t_symbol     *p_sym;
    t_clock      *p_clock;
    struct _menu *p_cnv;
}t_edit_proxy;

typedef struct _menu{
    t_object        x_obj;
    t_edit_proxy   *x_proxy;
    t_canvas       *x_cv;
    t_glist        *x_glist;
    int             x_width, x_height;   // Graphical Object's dimensions
    int             x_fontsize;          // Font Size
    int             x_idx;               // selected item's index
    int             x_n_items;           // number of items in the popmenu
    int             x_maxitems;
    int             x_disabled;
    int             x_zoom;
    t_symbol       *x_label;
    t_symbol      **x_items;
    t_symbol       *x_sym;
    t_symbol       *x_param;
    t_symbol       *x_var;
    t_symbol       *x_var_raw;
    int            x_var_set;
    int             x_savestate;
    int             x_keep;         // keep/save contents
    int             x_load;         // value when loading patch
    int             x_lb;
    int             x_outline;
    int             x_outmode;
    int             x_flag;
    int             x_pos;
    t_symbol       *x_dir;
    t_symbol       *x_rcv;
    t_symbol       *x_rcv_raw;
    int             x_rcv_set;
    int             x_r_flag;
    t_symbol       *x_snd;
    t_symbol       *x_snd_raw;
    int             x_snd_set;
    int             x_s_flag;
    int             x_v_flag;
    char            x_tag_obj[32];
    char            x_tag_outline[32];
    char            x_tag_in[32];
    char            x_tag_out[32];
    char            x_tag_sel[32];
    char            x_tag_mb[32];
    char            x_tag_popmenu[32];
    char            x_tag_menu[64];
    char            x_tag_menu_sel[64];
    char            x_callback_proc[64];
    char           *x_cvId;
    int             x_edit;
    t_symbol       *x_bg;
    t_symbol       *x_fg;
    t_symbol       *x_ignore;
    t_atom         *x_options;
    int             x_itemcount;
}t_menu;

// Helper functions -----------------------------------------------------------------
static int vis(t_menu *x){
    return(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist));
}

static void menu_config_fg(t_menu *x){
    pdgui_vmess(0, "rr rs rs", x->x_tag_mb, "configure",
        "-fg", x->x_fg->s_name, "-activeforeground", x->x_fg->s_name);
}

static void menu_config_bg(t_menu *x){
    pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_outline,
        "-fill", x->x_bg->s_name);
    pdgui_vmess(0, "rr rs", x->x_tag_mb, "configure", "-bg", x->x_bg->s_name);
}

static void menu_config_io(t_menu *x){
    if(!vis(x))
        return;
    int inlet = (x->x_rcv == &s_ || x->x_rcv == gensym("empty")) && x->x_edit;
    pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_in,
        "-state", inlet ? "normal" : "hidden");
    int outlet = (x->x_snd == &s_ || x->x_snd == gensym("empty")) && x->x_edit;
    pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_out,
        "-state", outlet ? "normal" : "hidden");
    int outline = x->x_edit || x->x_outline;
    pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_outline,
        "-state", outline ? "normal" : "hidden");
}

static void menu_disablecheck(t_menu *x){
    if(vis(x)){
        pdgui_vmess(0, "rrrrrr", x->x_tag_mb, "configure",
            "-state", (x->x_edit || x->x_disabled) ? "disabled" : "active",
            "-cursor", x->x_edit ? "hand2" : "bottom_side");
    }
}

static void edit_proxy_any(t_edit_proxy *p, t_symbol *s, int ac, t_atom *av){
    int edit = ac = 0;
    if(p->p_cnv){
        if(s == gensym("editmode"))
            edit = (int)(av->a_w.w_float);
        else if(s == gensym("obj") || s == gensym("msg") || s == gensym("floatatom")
        || s == gensym("symbolatom") || s == gensym("text") || s == gensym("bng")
        || s == gensym("toggle") || s == gensym("numbox") || s == gensym("vslider")
        || s == gensym("hslider") || s == gensym("vradio") || s == gensym("hradio")
        || s == gensym("vumeter") || s == gensym("mycnv") || s == gensym("selectall")){
            edit = 1;
        }
        else
            return;
        if(p->p_cnv->x_edit != edit){
            p->p_cnv->x_edit = edit;
            menu_config_io(p->p_cnv);
            menu_disablecheck(p->p_cnv);
        }
    }
}

static t_symbol *getcolor(int ac, t_atom *av){
    if((av)->a_type == A_SYMBOL)
        return(atom_getsymbol(av));
    else{
        int r = atom_getintarg(0, ac, av);
        int g = atom_getintarg(1, ac, av);
        int b = atom_getintarg(2, ac, av);
        r = r < 0 ? 0 : r > 255 ? 255 : r;
        g = g < 0 ? 0 : g > 255 ? 255 : g;
        b = b < 0 ? 0 : b > 255 ? 255 : b;
        char color[20];
        sprintf(color, "#%2.2x%2.2x%2.2x", r, g, b);
        return(gensym(color));
    }
}

static void menu_get_rcv(t_menu* x){
    if(!x->x_rcv_set){ // no receive set, search arguments
        t_binbuf *bb = x->x_obj.te_binbuf;
        int n_args = binbuf_getnatom(bb) - 1; // number of arguments
        char buf[128];
        if(n_args > 0){ // we have arguments, let's search them
            if(x->x_flag){ // arguments are flags actually
                if(x->x_r_flag){ // we got a receive flag, let's get it
                    for(int i = 0;  i <= n_args; i++){
                        atom_string(binbuf_getvec(bb) + i, buf, 128);
                        if(gensym(buf) == gensym("-receive")){
                            i++;
                            atom_string(binbuf_getvec(bb) + i, buf, 128);
                            x->x_rcv_raw = gensym(buf);
                            break;
                        }
                    }
                }
            }
            else{ // we got no flags, let's search for argument
                int arg_n = 7; // receive argument number
                if(n_args >= arg_n){ // we have it, get it
                    atom_string(binbuf_getvec(bb) + arg_n, buf, 128);
                    x->x_rcv_raw = gensym(buf);
                }
            }
        }
    }
    if(x->x_rcv_raw == &s_)
        x->x_rcv_raw = gensym("empty");
}

static void menu_get_snd(t_menu* x){
    if(!x->x_snd_set){ // no send set, search arguments
        t_binbuf *bb = x->x_obj.te_binbuf;
        int n_args = binbuf_getnatom(bb) - 1; // number of arguments
        char buf[128];
        if(n_args > 0){ // we have arguments, let's search them
            if(x->x_flag){ // arguments are flags actually
                if(x->x_s_flag){ // we got a send flag, let's get it
                    for(int i = 0;  i <= n_args; i++){
                        atom_string(binbuf_getvec(bb) + i, buf, 128);
                        if(gensym(buf) == gensym("-send")){
                            i++;
                            atom_string(binbuf_getvec(bb) + i, buf, 128);
                            x->x_snd_raw = gensym(buf);
                            break;
                        }
                    }
                }
            }
            else{ // we got no flags, let's search for argument
                int arg_n = 8; // send argument number
                if(n_args >= arg_n){ // we have it, get it
                    atom_string(binbuf_getvec(bb) + arg_n, buf, 128);
                    x->x_snd_raw = gensym(buf);
                }
            }
        }
    }
    if(x->x_snd_raw == &s_)
        x->x_snd_raw = gensym("empty");
}

static void menu_get_var(t_menu* x){
    if(!x->x_var_set){ // no var set, search arguments
        t_binbuf *bb = x->x_obj.te_binbuf;
        int n_args = binbuf_getnatom(bb) - 1; // number of arguments
        char buf[128];
        if(n_args > 0){ // we have arguments, let's search them
            if(x->x_flag){ // arguments are flags actually
                if(x->x_v_flag){ // we got a var flag, let's get it
                    for(int i = 0;  i <= n_args; i++){
                        atom_string(binbuf_getvec(bb) + i, buf, 128);
                        if(gensym(buf) == gensym("-var")){
                            i++;
                            atom_string(binbuf_getvec(bb) + i, buf, 128);
                            x->x_var_raw = gensym(buf);
                            break;
                        }
                    }
                }
            }
            else{ // we got no flags, let's search for argument
                int arg_n = 9; // var argument number
                if(n_args >= arg_n){ // we have it, get it
                    atom_string(binbuf_getvec(bb) + arg_n, buf, 128);
                    x->x_var_raw = gensym(buf);
                }
            }
        }
    }
    if(x->x_var_raw == &s_)
        x->x_var_raw = gensym("empty");
}

// DRAW functions --------------------------------------------------------------
static void create_menu(t_menu *x, t_glist *glist){
    char buf[MAXPDSTRING];
    t_canvas *cv = glist_getcanvas(glist);
    sprintf(x->x_tag_mb, ".x%lx.c.s%lx", (unsigned long)cv, (unsigned long)x);
    sprintf(x->x_tag_menu, "%s.menu", x->x_tag_mb);
    sprintf(x->x_tag_menu_sel, "%s_idx", x->x_tag_menu);
    sys_vgui("set %s {}\n", x->x_tag_menu_sel); // e.g., "menu_selectedItem"
    t_symbol* temp_name;
    if(x->x_idx < 0)  // use default if not selected
        temp_name = x->x_label;
    else
        temp_name = x->x_items[x->x_idx];
    pdgui_vmess(0, "rr", "destroy", x->x_tag_mb); // in case it exists (vis?)
    // Create a shared Tcl variable for the selected item
    t_atom at[2];
    SETSYMBOL(at, gensym(def_font));
    SETFLOAT(at+1, -x->x_fontsize*x->x_zoom);
    // Create the menubutton
    pdgui_vmess(0, "rs rs rA rsrs rs rs rs rr rs rs rs",
        "menubutton", x->x_tag_mb,
        "-text", temp_name->s_name,
        "-font", 2, at,
        "-bg", x->x_bg->s_name, "-fg", x->x_fg->s_name,
        "-activeforeground", x->x_fg->s_name,
        "-direction", x->x_dir->s_name,
        "-menu", x->x_tag_menu,
        "-cursor", "bottom_side",
        "-relief", "raised",
        "-indicatoron", "1",
        "-compound", "right");
    // Create the menu and attach it to the menubutton
    pdgui_vmess(0, "rsri", "menu", x->x_tag_menu, "-tearoff", 0);
    // Loop through menu items to add radiobuttons
    for(int i = 0; i < x->x_n_items; i++) {
        sprintf(buf, "%s", x->x_items[i]->s_name);
        sys_vgui("%s add radiobutton -label \"%s\" -variable %s -value \"option_%d\" \
            -command {%s configure -text \"%s\" \; %s \"%d\"} \n",
            x->x_tag_menu, buf, x->x_tag_menu_sel, i,
            x->x_tag_mb, buf, x->x_callback_proc, i);
    }
    // Set the selected index value to the variable (clear any previous selection)
    sys_vgui("set %s \"option_%d\" \n", x->x_tag_menu_sel, x->x_idx);
    // Reconfigure the selected entry to match the current index
    if (x->x_idx >= 0) {
        sys_vgui("%s entryconfigure %d -variable %s -value \"option_%d\" \n",
            x->x_tag_menu, x->x_idx, x->x_tag_menu_sel, x->x_idx);
    }
    // set tk widget ids
    sprintf(buf, ".x%lx.c", (unsigned long)cv);
    x->x_cvId = getbytes(strlen(buf) + 1); // Tk ID for current canvas this object is drawn in
    strcpy(x->x_cvId, buf);
    sys_vgui("bind %s.s%lx <Button> {pdtk_canvas_mouse %s [expr %%X - [winfo rootx %s]] \
        [expr %%Y - [winfo rooty %s]] %%b 0}\n", x->x_cvId, x, x->x_cvId, x->x_cvId, x->x_cvId);
    sys_vgui("bind %s.s%lx <Button-2> {pdtk_canvas_rightclick %s [expr %%X - [winfo rootx %s]] \
        [expr %%Y - [winfo rooty %s]] %%b}\n", x->x_cvId, x, x->x_cvId, x->x_cvId, x->x_cvId);
}

static void menu_draw(t_menu *x, t_glist *glist){
    t_canvas *cv = x->x_glist = glist_getcanvas(glist);
    int z = x->x_zoom;
    int x1 = text_xpix(&x->x_obj, glist), y1 = text_ypix(&x->x_obj, glist);
    int x2 = x1 + x->x_width*z, y2 = y1 + x->x_height*z;
    create_menu(x, glist);
// popmenu
    int xb = 2; // horizontal border
    int yb = 4; // vertical border
    int w = x->x_width*z, h = x->x_height*z;
    char *tags_popmenu[] = {x->x_tag_popmenu, x->x_tag_obj};
    pdgui_vmess(0, "crr iiriri rsrs rS", cv, "create", "window",
        x1+xb, y1+yb, "-width", w-xb, "-height", h-yb*2,
        "-anchor", "nw", "-window", x->x_tag_mb,
        "-tags", 2, tags_popmenu);
    menu_disablecheck(x);
 // Object Outline
    char *tags_outline[] = {x->x_tag_outline, x->x_tag_obj, x->x_tag_sel};
    pdgui_vmess(0, "crr iiii rs rs rS", cv, "create", "rectangle",
        x1, y1, x2, y2, "-fill", x->x_bg->s_name,
        "-state", x->x_outline ? "normal" : "hidden",
        "-tags", 3, tags_outline);
// inlet
    char *tags_in[] = {x->x_tag_in, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rs rs rS", cv, "create", "rectangle",
        x1, y1, x1+IOWIDTH*z, y1+IHEIGHT*z,
        "-fill", "black", "-state", "hidden", "-tags", 2, tags_in);
// outlet
    char *tags_out[] = {x->x_tag_out, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rs rs rS", cv, "create", "rectangle",
        x1, y2-OHEIGHT*z, x1+IOWIDTH*z, y2,
        "-fill", "black", "-state", "hidden", "-tags", 2, tags_out);
    menu_config_io(x);
}

static void menu_erase(t_menu* x,t_glist* glist){
    t_canvas *cv = glist_getcanvas(glist);
    pdgui_vmess(0, "rr", "destroy", x->x_tag_mb);
    pdgui_vmess(0, "crs", cv, "delete", x->x_tag_obj);
}

// ---------------------------------------------------------------------------
// callback comes here
static void menu_output(t_menu* x, t_floatarg i){
    x->x_idx = i;
    if(x->x_outmode == 0){ // just index
        if(x->x_param != gensym("empty")){
            t_atom at[1];
            SETFLOAT(at, x->x_idx);
            outlet_anything(x->x_obj.ob_outlet, x->x_param, 1, at);
            if(x->x_snd->s_thing)
                typedmess(x->x_snd->s_thing, x->x_param, 1, at);
        }
        else{
            outlet_float(x->x_obj.ob_outlet, x->x_idx);
            if (x->x_snd->s_thing && x->x_snd_raw != gensym("empty"))
                pd_float(x->x_snd->s_thing, x->x_idx);
        }
        if(x->x_var != gensym("empty"))
            value_setfloat(x->x_var, x->x_idx);
    }
    else if(x->x_outmode == 1){ // just selection
        if((x->x_options+x->x_idx)->a_type == A_SYMBOL){
            t_symbol *out = atom_getsymbol(x->x_options+x->x_idx);
            if(x->x_param != gensym("empty")){
                t_atom at[1];
                SETSYMBOL(at, out);
                outlet_anything(x->x_obj.ob_outlet, x->x_param, 1, at);
                if(x->x_snd->s_thing)
                    typedmess(x->x_snd->s_thing, x->x_param, 1, at);
            }
            else{
                outlet_symbol(x->x_obj.ob_outlet, out);
                if (x->x_snd->s_thing && x->x_snd_raw != gensym("empty"))
                    pd_symbol(x->x_snd->s_thing, out);
            }
        }
        else{
            float out = atom_getfloat(x->x_options+x->x_idx);
            if(x->x_param != gensym("empty")){
                t_atom at[1];
                SETFLOAT(at, out);
                outlet_anything(x->x_obj.ob_outlet, x->x_param, 1, at);
                if(x->x_snd->s_thing)
                    typedmess(x->x_snd->s_thing, x->x_param, 1, at);
            }
            else{
                outlet_float(x->x_obj.ob_outlet, out);
                if (x->x_snd->s_thing && x->x_snd_raw != gensym("empty"))
                    pd_float(x->x_snd->s_thing, out);
            }
        }
    }
    else if(x->x_outmode == 2){ // index + selection
        t_atom at[2];
        SETFLOAT(at, x->x_idx);
        if((x->x_options+x->x_idx)->a_type == A_SYMBOL)
            SETSYMBOL(at+1, atom_getsymbol(x->x_options+x->x_idx));
        else
            SETFLOAT(at+1, atom_getfloat(x->x_options+x->x_idx));
        if(x->x_param != gensym("empty")){
            outlet_anything(x->x_obj.ob_outlet, x->x_param, 2, at);
            if(x->x_snd->s_thing)
                typedmess(x->x_snd->s_thing, x->x_param, 2, at);
        }
        else{
            outlet_list(x->x_obj.ob_outlet, &s_list, 2, at);
            if (x->x_snd->s_thing && x->x_snd_raw != gensym("empty"))
                pd_list(x->x_snd->s_thing, &s_list, 2, at);
        }
    }
}
// ---------------------------------------------------------------------------

static void menu_bang(t_menu* x){
    if(x->x_idx >= 0 && x->x_n_items > 0)
        menu_output(x, x->x_idx);
}

static void menu_param(t_menu *x, t_symbol *s){
    if(s == gensym("") || s == &s_)
        x->x_param = gensym("empty");
    else
        x->x_param = s;
}

static void menu_var(t_menu *x, t_symbol *s){
    if(s == gensym("") || s == &s_)
        s = gensym("empty");
    t_symbol *var = s == gensym("empty") ? &s_ : canvas_realizedollar(x->x_glist, s);
    if(var != x->x_var){
        x->x_var_set = 1;
        x->x_var_raw = s;
        x->x_var = var;
    }
}

static void menu_set(t_menu* x, t_floatarg f){
    x->x_idx = f < -1 ? -1 : f >= x->x_n_items ? x->x_n_items - 1 : (int)f;
    if(!vis(x))
        return;
    sys_vgui("set %s \"option_%d\" \n", x->x_tag_menu_sel, x->x_idx);
    // Reconfigure the selected entry to match the current index
    if(x->x_idx >= 0){
        sys_vgui("%s entryconfigure %d -variable %s -value \"option_%d\" \n",
            x->x_tag_menu, x->x_idx, x->x_tag_menu_sel, x->x_idx);
    }
    pdgui_vmess(0, "rr rs", x->x_tag_mb, "configure",
        "-text", x->x_idx == -1 ? x->x_label->s_name : x->x_items[x->x_idx]->s_name);
}

static void menu_float(t_menu* x, t_floatarg f){
    menu_set(x, f);
    menu_bang(x);
}

static void menu_add(t_menu* x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    int new_limit = x->x_n_items + ac;
    if(new_limit > x->x_maxitems){
        int new_size = new_limit * 2;
        t_symbol **dummy = (t_symbol**)getbytes(sizeof(t_symbol*)*new_size);
        if(dummy){
            memcpy(dummy, x->x_items, sizeof(t_symbol*)*x->x_maxitems);
            freebytes(x->x_items, sizeof(t_symbol*)*x->x_maxitems);
            x->x_maxitems = new_size;
            x->x_items = dummy;
        }
        else{
            pd_error(x, "[popmenu]: no memory for items");
            return;
        }
    }
    char buf[MAXPDSTRING];
    for(int i = x->x_n_items; i < new_limit; i++){
        if((av+i-x->x_n_items)->a_type == A_FLOAT){
            sprintf(buf, "%g", atom_getfloat(av+i-x->x_n_items));
            x->x_items[i] = gensym(buf);
        }
        else
            x->x_items[i] = atom_getsymbol(av+i-x->x_n_items);
        sprintf(buf, "%s", x->x_items[i]->s_name);
        if(vis(x)){
            sys_vgui("%s add radiobutton -label \"%s\" -variable %s -value \"%s\" \
                -command {%s configure -text \"%s\" \; %s \"%d\"} \n",
                     x->x_tag_menu, buf, x->x_tag_menu_sel, buf,
                     x->x_tag_mb, buf, x->x_callback_proc, i);
        }
    }
    x->x_n_items = new_limit;
    x->x_disabled = 0;
    menu_disablecheck(x);
    for(int i = 0; i < ac; i++){  // Copy atoms
        *(x->x_options+x->x_itemcount) = *(av+i);
        x->x_itemcount++;
    }
}

static void menu_clear(t_menu* x){
    x->x_n_items = x->x_itemcount = 0;
    x->x_disabled = 1;
    menu_disablecheck(x);
    if(vis(x)){
        pdgui_vmess(0, "rr ir", x->x_tag_menu, "delete", 0, "end");
        pdgui_vmess(0, "rr rs", x->x_tag_mb, "configure", "-text", x->x_label->s_name);
    }
}

static void menu_bg(t_menu *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(!ac)
        return;
    t_symbol *color = getcolor(ac, av);
    if(x->x_bg != color){
        x->x_bg = color;
        if(vis(x))
            menu_config_bg(x);
    }
}

static void menu_label(t_menu *x, t_symbol *s){
    x->x_label = s;
    if(x->x_idx == -1 && vis(x))
        pdgui_vmess(0, "rr rs", x->x_tag_mb, "configure", "-text", x->x_label->s_name);
}

static void menu_receive(t_menu *x, t_symbol *s){
    if(s == gensym(""))
        s = gensym("empty");
    t_symbol *rcv = s == gensym("empty") ? &s_ : canvas_realizedollar(x->x_glist, s);
    if(rcv != x->x_rcv){
        x->x_rcv_set = 1;
        t_symbol *old_rcv = x->x_rcv;
        x->x_rcv_raw = s;
        x->x_rcv = rcv;
        if(old_rcv != &s_ && old_rcv != gensym("empty"))
            pd_unbind(&x->x_obj.ob_pd, old_rcv);
        if(x->x_rcv != &s_)
            pd_bind(&x->x_obj.ob_pd, x->x_rcv);
        menu_config_io(x);
    }
}

static void menu_send(t_menu *x, t_symbol *s){
    if(s == gensym(""))
        s = gensym("empty");
    t_symbol *snd = s == gensym("empty") ? &s_ : canvas_realizedollar(x->x_glist, s);
    if(snd != x->x_snd){
        x->x_snd_set = 1;
        x->x_snd_raw = s;
        x->x_snd = snd;
        menu_config_io(x);
    }
}

static void menu_fg(t_menu *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(!ac)
        return;
    t_symbol *color = getcolor(ac, av);
    if(x->x_fg != color){
        x->x_fg = color;
        if(vis(x))
            menu_config_fg(x);
    }
}

static void menu_load(t_menu *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(!ac)
        x->x_load = x->x_idx;
    else if(ac == 1 && av->a_type == A_FLOAT){
        int i = atom_getint(av);
        x->x_load = i < -1 ? -1 : i >= x->x_n_items ? x->x_n_items - 1 : i;
    }
}

static void menu_mode(t_menu* x, t_floatarg f){
    x->x_outmode = f < 0 ? 0 : f > 2 ? 2 : (int)f;
}

static void menu_width(t_menu* x, t_floatarg f){
    x->x_width = f < 40 ? 40 : (int)f;
    int z = x->x_zoom;
    int xb = 2;
    pdgui_vmess(0, "crs ri", x->x_cv, "itemconfigure",
        x->x_tag_popmenu, "-width", x->x_width*z-xb);
    int x1 = text_xpix(&x->x_obj, x->x_glist);
    int y1 = text_ypix(&x->x_obj, x->x_glist);
    int x2 = x1 + x->x_width*z;
    int y2 = y1 + x->x_height*z;
    if(vis(x))
        pdgui_vmess(0, "crs iiii", x->x_cv, "coords",
            x->x_tag_outline, x1, y1, x2, y2);
}

static void menu_height(t_menu* x, t_floatarg f){
    x->x_height = f < 25 ? 25 : (int)f;
    int z = x->x_zoom;
    int yb = 4;
    pdgui_vmess(0, "crs ri", x->x_cv, "itemconfigure",
        x->x_tag_popmenu, "-height", x->x_height*z-yb*2);
    int x1 = text_xpix(&x->x_obj, x->x_glist);
    int y1 = text_ypix(&x->x_obj, x->x_glist);
    int x2 = x1 + x->x_width*z;
    int y2 = y1 + x->x_height*z;
    if(vis(x)){
        pdgui_vmess(0, "crs iiii", x->x_cv, "coords",
            x->x_tag_outline, x1, y1, x2, y2);
        pdgui_vmess(0, "crs iiii", x->x_cv, "coords", x->x_tag_out,
            x1, y2 - OHEIGHT*z, x1 + IOWIDTH*z, y2);
        canvas_fixlinesfor(x->x_glist, (t_text *)x);
    }
}

static void menu_fontsize(t_menu* x, t_floatarg f){
    x->x_fontsize = f < 8 ? 8 : (int)f;
    t_atom at[2];
    SETSYMBOL(at, gensym(def_font));
    SETFLOAT(at+1, -x->x_fontsize*x->x_zoom);
    if(vis(x))
        pdgui_vmess(0, "rr rA", x->x_tag_mb, "configure",
            "-font", 2, at);
}

static void menu_outline(t_menu* x, t_floatarg f){
    x->x_outline = f != 0;
    menu_config_io(x);
}

static void menu_savestate(t_menu *x, t_floatarg f){
    x->x_savestate = (f != 0);
}

static void menu_keep(t_menu *x, t_floatarg f){
    x->x_keep = (f != 0);
}

static void menu_pos(t_menu *x, t_floatarg f){
    x->x_pos = f < 0 ? 0 : f > 4 ? 4 : (int)f;
    switch(x->x_pos){
        case 0:
            x->x_dir = gensym("below");
            break;
        case 1:
            x->x_dir = gensym("above");
            break;
        case 2:
            x->x_dir = gensym("left");
            break;
        case 3:
            x->x_dir = gensym("right");
            break;
        case 4:
            x->x_dir = gensym("flush");
            break;
        default:
            break;
    };
    if(vis(x))
        pdgui_vmess(0, "rr rs", x->x_tag_mb, "configure",
            "-direction", x->x_dir->s_name);
}

static void menu_lb(t_menu *x, t_floatarg f){
    x->x_lb = (f != 0);
}

static void menu_zoom(t_menu *x, t_floatarg f){
    x->x_zoom = (int)f;
}

// --------------- properties stuff --------------------
static void menu_properties(t_gobj *z, t_glist *owner){
    t_menu *x = (t_menu *)z;
    menu_get_rcv(x);
    menu_get_snd(x);
    menu_get_var(x);
    t_symbol *outmode = gensym("Index");
    if(x->x_outmode == 1)
        outmode = gensym("Item");
    else if(x->x_outmode == 2)
        outmode = gensym("Both");
    t_symbol *position = gensym("Bottom");
    if(x->x_outmode == 1)
        position = gensym("Top");
    else if(x->x_outmode == 2)
        position = gensym("Left");
    else if(x->x_outmode == 2)
        position = gensym("Right");
    else if(x->x_outmode == 2)
        position = gensym("Over");
    pdgui_stub_vnew(&x->x_obj.ob_pd, "menu_dialog", x,
        "ii iis iiiis sssssss",
        x->x_width, x->x_height,
        x->x_fontsize, x->x_outline, outmode->s_name,
        x->x_load, x->x_lb, x->x_savestate, x->x_keep, position->s_name,
        x->x_label->s_name, x->x_rcv_raw->s_name, x->x_snd_raw->s_name,
        x->x_param->s_name, x->x_var_raw->s_name,
        x->x_bg->s_name, x->x_fg->s_name);
}

static void menu_apply(t_menu *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    t_atom undo[17];
    SETFLOAT(undo+0, x->x_width);
    SETFLOAT(undo+1, x->x_height);
    SETFLOAT(undo+2, x->x_fontsize);
    SETFLOAT(undo+3, x->x_outline);
    SETFLOAT(undo+4, x->x_outmode);
    SETFLOAT(undo+5, x->x_load);
    SETFLOAT(undo+6, x->x_lb);
    SETFLOAT(undo+7, x->x_savestate);
    SETFLOAT(undo+8, x->x_keep);
    SETFLOAT(undo+9, x->x_pos);
    SETSYMBOL(undo+10, x->x_label);
    SETSYMBOL(undo+11, x->x_rcv);
    SETSYMBOL(undo+12, x->x_snd);
    SETSYMBOL(undo+13, x->x_param);
    SETSYMBOL(undo+14, x->x_var);
    SETSYMBOL(undo+15, x->x_bg);
    SETSYMBOL(undo+16, x->x_fg);
    pd_undo_set_objectstate(x->x_glist, (t_pd*)x, gensym("dialog"), 17, undo, ac, av);
    int width = atom_getintarg(0, ac, av);
    int height = atom_getintarg(1, ac, av);
    int fontsize = atom_getintarg(2, ac, av);
    x->x_outline = atom_getintarg(3, ac, av);
    t_symbol *outmode = atom_getsymbolarg(4, ac, av);
    x->x_load = atom_getfloatarg(5, ac, av);
    x->x_lb = atom_getintarg(6, ac, av);
    x->x_savestate = atom_getintarg(7, ac, av);
    x->x_keep = atom_getintarg(8, ac, av);
    t_symbol *position = atom_getsymbolarg(9, ac, av);
    t_symbol *label = atom_getsymbolarg(10, ac, av);
    t_symbol *rcv = atom_getsymbolarg(11, ac, av);
    t_symbol *snd = atom_getsymbolarg(12, ac, av);
    t_symbol *param = atom_getsymbolarg(13, ac, av);
    t_symbol *var = atom_getsymbolarg(14, ac, av);
    t_symbol *bg = atom_getsymbolarg(15, ac, av);
    t_symbol *fg = atom_getsymbolarg(16, ac, av);
    menu_config_io(x); // for outline
    if(outmode == gensym("Index"))
        x->x_outmode = 0;
    else if(outmode == gensym("Item"))
        x->x_outmode = 1;
    else if(outmode == gensym("Both"))
        x->x_outmode = 2;
    int pos = 0;
    if(position == gensym("Top"))
        pos = 1;
    else if(position == gensym("Left"))
        pos = 2;
    else if(position == gensym("Right"))
        pos = 3;
    else if(position == gensym("Ober"))
        pos = 4;
    menu_pos(x, pos);
    menu_width(x, width);
    menu_height(x, height);
    menu_fontsize(x, fontsize);
    menu_label(x, label);
    menu_receive(x, rcv);
    menu_send(x, snd);
    menu_param(x, param);
    menu_var(x, var);
    t_atom at[1];
    SETSYMBOL(at, bg);
    menu_bg(x, NULL, 1, at);
    SETSYMBOL(at, fg);
    menu_fg(x, NULL, 1, at);
    canvas_dirty(x->x_glist, 1);
}

// ------------------------ widgetbehaviour-----------------------------
static void menu_getrect(t_gobj *z, t_glist *gl, int *xp1, int *yp1, int *xp2, int *yp2){
    t_menu* x = (t_menu*)z;
    *xp1 = text_xpix(&x->x_obj, gl);
    *yp1 = text_ypix(&x->x_obj, gl);
    *xp2 = text_xpix(&x->x_obj, gl) + x->x_width*x->x_zoom;
    *yp2 = text_ypix(&x->x_obj, gl) + x->x_height*x->x_zoom;
}

static void menu_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_menu *x = (t_menu *)z;
    x->x_obj.te_xpix += dx, x->x_obj.te_ypix += dy;
    dx *= x->x_zoom, dy *= x->x_zoom;
    t_canvas *cv = glist_getcanvas(glist);
    pdgui_vmess(0, "crs ii", cv, "move", x->x_tag_obj, dx, dy);
    canvas_fixlinesfor(glist, (t_text*)x);
}

static void menu_select(t_gobj *z, t_glist *glist, int sel){
    t_menu *x = (t_menu *)z;
    t_canvas *cv = glist_getcanvas(glist);
    pdgui_vmess(0, "crs rs", cv, "itemconfigure",
        x->x_tag_sel, "-outline", sel ? "blue" : "black");
}

static void menu_delete(t_gobj *z, t_glist *glist){
    canvas_deletelinesfor(glist, (t_text *)z);
}

static void menu_vis(t_gobj *z, t_glist *glist, int vis){
    t_menu* x = (t_menu*)z;
    x->x_cv = glist_getcanvas(x->x_glist = glist);
    vis ? menu_draw(x, glist) : menu_erase(x, glist);
}

static void menu_save(t_gobj *z, t_binbuf *b){
    t_menu *x = (t_menu *)z;
    binbuf_addv(b, "ssiis", gensym("#X"), gensym("obj"),
        (t_int)x->x_obj.te_xpix, (t_int)x->x_obj.te_ypix,
        atom_getsymbol(binbuf_getvec(x->x_obj.te_binbuf)));
    if(x->x_savestate)
        x->x_load = x->x_idx;
    menu_get_rcv(x);
    menu_get_snd(x);
    menu_get_var(x);
    binbuf_addv(b, "iiisssssssiiiiiiiiiii",
        x->x_width, x->x_height, x->x_fontsize,
        x->x_bg, x->x_fg,
        x->x_label, x->x_rcv_raw, x->x_snd_raw, // label rcv snd
        x->x_param, x->x_var_raw, // prm var
        x->x_outline, x->x_outmode, // outline, output mode
        x->x_load, x->x_lb, x->x_savestate, // initial value, loadbang, savestate
        x->x_keep, x->x_pos, // save contents, position
        0, 0, 0, 0); // placeholders, justify + ??????
    if(x->x_keep)
        for(int i = 0; i < x->x_n_items; i++) // Loop for menu items
            binbuf_addv(b, "s", x->x_items[i]);
    binbuf_addv(b, ";");
}

// INIT STUFF --------------------------------------------------------------------
static void menu_loadbang(t_menu *x, t_float f){
    if((int)f == LB_LOAD && x->x_lb)
        menu_bang(x);
}

static void menu_free(t_menu*x){
    if(x->x_items)
        freebytes(x->x_items, sizeof(t_symbol*)*x->x_maxitems);
    pd_unbind(&x->x_obj.ob_pd, x->x_sym);
    if(x->x_rcv != gensym("empty") && x->x_rcv != &s_)
        pd_unbind(&x->x_obj.ob_pd, x->x_rcv);
    x->x_proxy->p_cnv = NULL;
    pdgui_stub_deleteforkey(x);
    freebytes(x->x_options, MAX_ITEMS*sizeof(*(x->x_options)));
}

static void edit_proxy_free(t_edit_proxy *p){
    pd_unbind(&p->p_obj.ob_pd, p->p_sym);
    clock_free(p->p_clock);
    pd_free(&p->p_obj.ob_pd);
}

static t_edit_proxy *edit_proxy_new(t_menu *x, t_symbol *s){
    t_edit_proxy *p = (t_edit_proxy*)pd_new(edit_proxy_class);
    p->p_cnv = x;
    pd_bind(&p->p_obj.ob_pd, p->p_sym = s);
    p->p_clock = clock_new(p, (t_method)edit_proxy_free);
    return(p);
}

static void *menu_new(t_symbol *s, int ac, t_atom *av){
    t_menu *x = (t_menu *)pd_new(menu_class);
    x->x_options = getbytes(sizeof(*(x->x_options)) * MAX_ITEMS);
    x->x_ignore = s;
	char buf[256];
    t_canvas *cv = canvas_getcurrent();
    x->x_glist = (t_glist*)cv;
    x->x_maxitems = MAX_ITEMS;
    x->x_items = (t_symbol**)getbytes(sizeof(t_symbol*)*x->x_maxitems);
    x->x_width = 128, x->x_height = 26;
    x->x_load = -1;
    x->x_lb = 1;
    x->x_fontsize = 12;
    x->x_fg = gensym("black"), x->x_bg = gensym("#dfdfdf");
    t_symbol *rcv = gensym("empty"), *snd = gensym("empty");
    t_symbol *param = gensym("empty"), *var = gensym("empty");
    x->x_label = gensym(" ");
    x->x_disabled = x->x_outline = x->x_keep = 1;
    x->x_n_items = x->x_itemcount = x->x_pos = 0;
    x->x_rcv_set = x->x_r_flag = 0;
    x->x_snd_set = x->x_s_flag = 0;
    x->x_var_set = x->x_v_flag = 0;
    if(ac){
        if(av->a_type == A_FLOAT){
            x->x_width = atom_getintarg(0, ac, av);
            x->x_height = atom_getintarg(1, ac, av);
            x->x_fontsize = atom_getintarg(2, ac, av);
            x->x_bg = atom_getsymbolarg(3, ac, av);
            x->x_fg = atom_getsymbolarg(4, ac, av);
            x->x_label = atom_getsymbolarg(5, ac, av); // label name
            rcv = atom_getsymbolarg(6, ac, av);
            snd = atom_getsymbolarg(7, ac, av);
            param = atom_getsymbolarg(8, ac, av); // param name
            var = atom_getsymbolarg(9, ac, av); // var name
            x->x_outline = atom_getintarg(10, ac, av);
            x->x_outmode = atom_getintarg(11, ac, av);
            x->x_load = atom_getintarg(12, ac, av);
            x->x_lb = atom_getintarg(13, ac, av);
            x->x_savestate = atom_getintarg(14, ac, av);
            x->x_keep = atom_getintarg(15, ac, av);
            x->x_pos = atom_getintarg(16, ac, av);
            // 4 placeholders
            ac-=21, av+=21;
            if(ac){
                x->x_n_items = ac;
                x->x_disabled = 0;
                for(int i = 0; i < x->x_n_items; i++){
                    if((av+i)->a_type == A_FLOAT){
                        sprintf(buf, "%g", atom_getfloat(av+i));
                        x->x_items[i] = gensym(buf);
                    }
                    else
                        x->x_items[i] = atom_getsymbol(av+i);
                    *(x->x_options+x->x_itemcount) = *(av+i);
                    x->x_itemcount++;
                }
            }
        }
        else{
            while(ac){
                t_symbol *sym = atom_getsymbol(av);
                if(sym == gensym("-fontsize")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_FLOAT){
                            x->x_fontsize = atom_getint(av);
                            if(x->x_fontsize < 8)
                                x->x_fontsize = 8;
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                if(sym == gensym("-width")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_FLOAT){
                            x->x_width = atom_getint(av);
                            if(x->x_width < 40)
                                x->x_fontsize = 40;
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                if(sym == gensym("-height")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_FLOAT){
                            x->x_height = atom_getint(av);
                            if(x->x_height < 25)
                                x->x_height = 25;
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-bg")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_SYMBOL){
                            x->x_bg = atom_getsymbol(av);
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-fg")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_SYMBOL){
                            x->x_fg = atom_getsymbol(av);
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-label")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_SYMBOL){
                            x->x_label = atom_getsymbol(av);
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-param")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_SYMBOL){
                            param = atom_getsymbol(av);
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-var")){
                    if(ac >= 2){
                        x->x_flag = x->x_v_flag = 1, av++, ac--;
                        if(av->a_type == A_SYMBOL){
                            var = atom_getsymbol(av);
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-nooutline")){
                    x->x_flag = 1, av++, ac--;
                    x->x_outline = 0;
                }
                else if(sym == gensym("-noloadbang")){
                    x->x_flag = 1, av++, ac--;
                    x->x_lb = 0;
                }
                else if(sym == gensym("-nokeep")){
                    x->x_flag = 1, av++, ac--;
                    x->x_keep = 0;
                }
                else if(sym == gensym("-receive")){
                    if(ac >= 2){
                        x->x_flag = x->x_r_flag = 1, av++, ac--;
                        if(av->a_type == A_SYMBOL){
                            rcv = atom_getsymbol(av);
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-send")){
                    if(ac >= 2){
                        x->x_flag = x->x_s_flag = 1, av++, ac--;
                        if(av->a_type == A_SYMBOL){
                            snd = atom_getsymbol(av);
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-load")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_FLOAT){
                            x->x_load = atom_getint(av);
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-outmode")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_FLOAT){
                            int mode = atom_getint(av);
                            x->x_outmode = mode < 0 ? 0 : mode > 2 ? 2 : mode;
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-pos")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_FLOAT){
                            int pos = atom_getint(av);
                            x->x_pos = pos < 0 ? 0 : pos > 4 ? 4 : pos;
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-savestate")){
                    x->x_flag = 1, av++, ac--;
                    x->x_savestate = 1;
                }
                else if(sym == gensym("-lb")){
                    x->x_flag = 1, av++, ac--;
                    x->x_lb = 1;
                }
                else
                    goto errstate;
            }
        }
    }
    x->x_edit = x->x_glist->gl_edit;
    x->x_zoom = x->x_glist->gl_zoom;
    menu_param(x, param);
    x->x_rcv = canvas_realizedollar(x->x_glist, x->x_rcv_raw = rcv);
    x->x_snd = canvas_realizedollar(x->x_glist, x->x_snd_raw = snd);
    x->x_var = canvas_realizedollar(x->x_glist, x->x_var_raw = var);
    x->x_idx = x->x_load;
    switch(x->x_pos){
        case 0:
            x->x_dir = gensym("below");
            break;
        case 1:
            x->x_dir = gensym("above");
            break;
        case 2:
            x->x_dir = gensym("left");
            break;
        case 3:
            x->x_dir = gensym("right");
            break;
        case 4:
            x->x_dir = gensym("flush");
            break;
        default:
            break;
    };
    if(x->x_idx < -1)
        x->x_idx = x->x_load = -1;
    else if(x->x_idx >= x->x_n_items)
        x->x_idx = x->x_load = (x->x_n_items - 1);
    // callback function
    sprintf(buf,"menu%p", (void *)x);
    x->x_sym = gensym(buf);
    pd_bind(&x->x_obj.ob_pd, x->x_sym); // bind receiver to the object
    sprintf(x->x_callback_proc, "menu_callback%p", (void *)x);
    sys_vgui("proc %s {index} {\n pdsend \"%s _callback $index \"\n }\n",
        x->x_callback_proc, buf);
    char buff[MAXPDSTRING];
    snprintf(buff, MAXPDSTRING-1, ".x%lx", (unsigned long)x->x_glist);
    buff[MAXPDSTRING-1] = 0;
    x->x_proxy = edit_proxy_new(x, gensym(buff));
    sprintf(x->x_tag_mb, ".x%lx.c.s%lx", (unsigned long)cv, (unsigned long)x);
    sprintf(x->x_tag_obj, "%pOBJ", (void *)x);
    sprintf(x->x_tag_outline, "%pOUTLINE", (void *)x);
    sprintf(x->x_tag_popmenu, "%pMENU", (void *)x);
    sprintf(x->x_tag_in, "%pIN", (void *)x);
    sprintf(x->x_tag_out, "%pOUT", (void *)x);
    sprintf(x->x_tag_sel, "%pSEL", (void *)x);
    if(x->x_rcv != gensym("empty") && x->x_rcv != &s_)
        pd_bind(&x->x_obj.ob_pd, x->x_rcv);
    outlet_new(&x->x_obj, &s_float);
    return(x);
errstate:
    pd_error(x, "[popmenu]: improper creation arguments");
    return(NULL);
}

void popmenu_setup(void){
    menu_class = class_new(gensym("popmenu"), (t_newmethod)menu_new,
        (t_method)menu_free, sizeof(t_menu), 0, A_GIMME, 0);
    class_addbang(menu_class, (t_method)menu_bang);
    class_addfloat(menu_class, (t_method)menu_float);
    class_addmethod(menu_class, (t_method)menu_set, gensym("set"), A_FLOAT, 0);
    class_addmethod(menu_class, (t_method)menu_fontsize, gensym("fontsize"), A_FLOAT, 0);
    class_addmethod(menu_class, (t_method)menu_width, gensym("width"), A_FLOAT, 0);
    class_addmethod(menu_class, (t_method)menu_height, gensym("height"), A_FLOAT, 0);
    class_addmethod(menu_class, (t_method)menu_clear, gensym("clear"), 0);
    class_addmethod(menu_class, (t_method)menu_add, gensym("add"), A_GIMME, 0);
    class_addmethod(menu_class, (t_method)menu_bg, gensym("bg"), A_GIMME, 0);
    class_addmethod(menu_class, (t_method)menu_fg, gensym("fg"), A_GIMME, 0);
    class_addmethod(menu_class, (t_method)menu_receive, gensym("receive"), A_DEFSYM, 0);
    class_addmethod(menu_class, (t_method)menu_send, gensym("send"), A_DEFSYM, 0);
    class_addmethod(menu_class, (t_method)menu_var, gensym("var"), A_DEFSYM, 0);
    class_addmethod(menu_class, (t_method)menu_param, gensym("param"), A_DEFSYM, 0);
    class_addmethod(menu_class, (t_method)menu_mode, gensym("mode"), A_FLOAT, 0);
    class_addmethod(menu_class, (t_method)menu_label, gensym("label"), A_SYMBOL, 0);
    class_addmethod(menu_class, (t_method)menu_outline, gensym("outline"), A_FLOAT, 0);
    class_addmethod(menu_class, (t_method)menu_keep, gensym("keep"), A_FLOAT, 0);
    class_addmethod(menu_class, (t_method)menu_pos, gensym("pos"), A_FLOAT, 0);
    class_addmethod(menu_class, (t_method)menu_lb, gensym("lb"), A_FLOAT, 0);
    class_addmethod(menu_class, (t_method)menu_load, gensym("load"), A_GIMME, 0);
    class_addmethod(menu_class, (t_method)menu_savestate, gensym("savestate"), A_FLOAT, 0);
    class_addmethod(menu_class, (t_method)menu_loadbang, gensym("loadbang"), A_DEFFLOAT, 0);
    class_addmethod(menu_class, (t_method)menu_zoom, gensym("zoom"), A_CANT, 0);
    class_addmethod(menu_class, (t_method)menu_output, gensym("_callback"), A_DEFFLOAT, 0);
    class_addmethod(menu_class, (t_method)menu_apply, gensym("dialog"), A_GIMME, 0);
    class_setpropertiesfn(menu_class, menu_properties);
    edit_proxy_class = class_new(0, 0, 0, sizeof(t_edit_proxy), CLASS_NOINLET | CLASS_PD, 0);
    class_addanything(edit_proxy_class, edit_proxy_any);
    class_setwidget(menu_class, &menu_widgetbehavior);
    menu_widgetbehavior.w_getrectfn  = menu_getrect;
    menu_widgetbehavior.w_displacefn = menu_displace;
    menu_widgetbehavior.w_selectfn   = menu_select;
    menu_widgetbehavior.w_deletefn   = menu_delete;
    menu_widgetbehavior.w_visfn      = menu_vis;
    class_setsavefn(menu_class, &menu_save);
    #include "../Extra/menu_dialog.h"
}
