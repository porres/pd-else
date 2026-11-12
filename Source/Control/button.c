// porres 2020-2025

#include <m_pd.h>
#include <g_canvas.h>

static t_class *button_class, *handle_class, *edit_proxy_class;
static t_widgetbehavior button_widgetbehavior;

#define BUTTON_HANDLE_SIZE 10
#define SHIFT_MASK         0x1

typedef struct _edit_proxy{
    t_object    p_obj;
    t_symbol   *p_sym;
    t_clock    *p_clock;
    struct      _button *p_cnv;
}t_edit_proxy;

typedef struct _button{
    t_object        x_obj;
    t_clock        *x_clock;
    t_glist        *x_glist;
    t_edit_proxy   *x_proxy;
    t_symbol       *x_bindname;
    int             x_mode;
    int             x_oval;
    int             x_theme;
    int             x_transparent;
    int             x_x;
    int             x_y;
    int             x_w;
    int             x_h;
    int             x_sel;
    int             x_zoom;
    int             x_edit;
    int             x_hover;
    t_symbol       *x_snd;
    t_symbol       *x_snd_raw;
    t_symbol       *x_snd_hover;
    int             x_flag;
    int             x_r_flag;
    int             x_s_flag;
    int             x_v_flag;
    int             x_rcv_set;
    int             x_snd_set;
    t_symbol       *x_rcv;
    t_symbol       *x_rcv_raw;
    int             x_state;
    int             x_readonly;
    t_symbol       *x_bg;
    t_symbol       *x_fg;
    char            x_base_tag[32];
    char            x_oval_tag[32];
    char            x_button_tag[32];
    char            x_all_tag[32];
    char            x_IO_tag[32];
    char            x_in_tag[32];
    char            x_out_tag[32];
    char            x_hover_tag[32];
    char            x_tag_handle[32];
// handle
    t_pd           *x_handle;
}t_button;

typedef struct _handle{
    t_pd            h_pd;
    t_button       *h_master;
    t_symbol       *h_bindsym;
    char            h_pathname[64], h_outlinetag[64];
    int             h_dragon;
    int             h_drag_delta_w, h_drag_delta_h; // preview deltas (logical)
    int             h_dragx, h_dragy;               // anchor pos (canvas coords)
    int             h_anchor_w, h_anchor_h;         // size at anchor (logical)
    int             h_last_shift;
}t_handle;

// helper functions

static int button_vis_check(t_button *x){
    return(gobj_shouldvis((t_gobj *)x, x->x_glist) && glist_isvisible(x->x_glist));
}

static t_symbol *button_getcolor(int ac, t_atom *av){
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
        sprintf(color, "#%.2x%.2x%.2x", r, g, b);
        return(gensym(color));
    }
}

/* theme-aware effective colors */
static void button_get_bgcolor(t_button *x, char *buf, size_t size){
    if(x->x_theme)
        snprintf(buf, size, "#%06X", THISGUI->i_backgroundcolor);
    else
        snprintf(buf, size, "%s", x->x_bg->s_name);
}

static void button_get_fgcolor(t_button *x, char *buf, size_t size){
    if(x->x_theme)
        snprintf(buf, size, "#%06X", THISGUI->i_foregroundcolor);
    else
        snprintf(buf, size, "%s", x->x_fg->s_name);
}

static void button_config_io(t_button *x){
    int inlet = (x->x_rcv == &s_ || x->x_rcv == gensym("empty")) && x->x_edit;
    t_canvas *cv = glist_getcanvas(x->x_glist);
    pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_in_tag,
        "-state", inlet ? "normal" : "hidden");
    int outlet = (x->x_snd == &s_ || x->x_snd == gensym("empty")) && x->x_edit;
    pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_out_tag,
        "-state", outlet ? "normal" : "hidden");
}

static void button_setfg(t_button *x){
    if(button_vis_check(x)){
        char fg[32];
        button_get_fgcolor(x, fg, sizeof(fg));
        pdgui_vmess(0, "crs rs", glist_getcanvas(x->x_glist), "itemconfigure",
            x->x_oval ? x->x_oval_tag : x->x_base_tag,
            "-fill", x->x_transparent > 1 ? "" : fg);
    }
}

static void button_setbg(t_button *x){
    if(button_vis_check(x)){
        char bg[32];
        button_get_bgcolor(x, bg, sizeof(bg));
        pdgui_vmess(0, "crs rs", glist_getcanvas(x->x_glist), "itemconfigure",
            x->x_oval ? x->x_oval_tag : x->x_base_tag,
            "-fill", x->x_transparent ? "" : bg);
    }
}

static void button_send_color_validate(t_button *x, const char *color, int which){
    char buf[512];
    snprintf(buf, sizeof(buf), "set color_name {%s}", color);
    pdgui_vmess(buf, NULL);
    pdgui_vmess("set color_escaped [string map {{ } {\\ }} $color_name]", NULL);
    pdgui_vmess("set hex_color [color2hex $color_name]", NULL);
    snprintf(buf, sizeof(buf),
        "if {$hex_color eq \"\"} "
        "{pdsend \"%s _validate 0 $color_escaped %d\"} "
        "else {pdsend \"%s _validate 1 $hex_color %d\"}",
        x->x_bindname->s_name, which,
        x->x_bindname->s_name, which);
    pdgui_vmess(buf, NULL);
}

static void button_validate(t_button *x, t_floatarg v, t_symbol *color, t_floatarg which){
    if(!(int)v){
        pd_error(0, "[button] invalid color '%s'", color->s_name);
        return;
    }
    if((int)which == 0){ // background
        x->x_bg = color;
        if(!x->x_state && button_vis_check(x))
            button_setbg(x);
    }
    else{ // foreground
        x->x_fg = color;
        if(x->x_state && button_vis_check(x))
            button_setfg(x);
    }
}

static void button_config_style(t_button *x){
    t_canvas *cv = glist_getcanvas(x->x_glist);
    if(x->x_oval){
        pdgui_vmess(0, "crs rs rs", cv, "itemconfigure", x->x_base_tag,
            "-outline", "", "-fill", "");
        pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_oval_tag,
            "-state", "normal");
    }
    else{
        pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_oval_tag,
            "-state", "hidden");
        char color[32], bg[32], fg[32];
        button_get_bgcolor(x, bg, sizeof(bg));
        button_get_fgcolor(x, fg, sizeof(fg));
        snprintf(color, sizeof(color), "%s",
            x->x_state ? x->x_transparent > 1 ? "" : fg :
            x->x_transparent ? "" : bg);
        pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_base_tag, "-fill", color);
    }
    if(x->x_transparent == 3)
        pdgui_vmess(0, "crs rs", cv, "itemconfigure",
        x->x_oval ? x->x_oval_tag : x->x_base_tag, "-outline", "");
    else{
        pdgui_vmess(0, "crs rk", cv, "itemconfigure",
        x->x_oval ? x->x_oval_tag : x->x_base_tag,
        "-outline",
        x->x_sel ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor);
    }
    if(x->x_edit)
        pdgui_vmess(0, "crs rk", cv, "itemconfigure", x->x_base_tag,
            "-outline", x->x_sel ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor);
}

static void button_draw(t_button *x, t_glist *glist){
    int xpos = text_xpix(&x->x_obj, glist), ypos = text_ypix(&x->x_obj, glist);
    t_canvas *cv = glist_getcanvas(glist);
    int z = x->x_zoom;
    int transp = x->x_transparent;
    char color[32], bg[32], fg[32];
    button_get_bgcolor(x, bg, sizeof(bg));
    button_get_fgcolor(x, fg, sizeof(fg));
    snprintf(color, sizeof(color), "%s",
        x->x_state ? transp > 1 ? "" : fg : transp ? "" : bg);
// rectangle
    char *tags_base[] = {x->x_base_tag, x->x_button_tag, x->x_all_tag};
    pdgui_vmess(0, "crr iiii ri rr rs rS", cv, "create", "rectangle",
        xpos, ypos, xpos + x->x_w*z, ypos + x->x_h*z,
        "-width", z,
        "-outline", "{}",
        "-fill", color,
        "-tags", 3, tags_base);
// oval
    char *tags_oval[] = {x->x_oval_tag, x->x_button_tag, x->x_all_tag};
    pdgui_vmess(0, "crr iiii ri rk rs rS", cv, "create", "oval",
        xpos, ypos, xpos + x->x_w*z, ypos + x->x_h*z,
        "-width", z,
        "-outline", THISGUI->i_foregroundcolor,
        "-fill", color,
        "-tags", 3, tags_oval);
// inlet
    char *tags_in[] = {x->x_in_tag, x->x_IO_tag, x->x_all_tag};
    pdgui_vmess(0, "crr iiii rk rk rS", cv, "create", "rectangle",
        xpos, ypos, xpos+(IOWIDTH*z), ypos+(IHEIGHT*z),
        "-outline", THISGUI->i_foregroundcolor,
        "-fill", THISGUI->i_foregroundcolor,
        "-tags", 3, tags_in);
// outlet
    char *tags_out[] = {x->x_out_tag, x->x_IO_tag, x->x_all_tag};
    pdgui_vmess(0, "crr iiii rk rk rS", cv, "create", "rectangle",
        xpos, ypos+(x->x_h*z), xpos+(IOWIDTH*z), ypos+(x->x_h*z)-(IHEIGHT*z),
        "-outline", THISGUI->i_foregroundcolor,
        "-fill", THISGUI->i_foregroundcolor,
        "-tags", 3, tags_out);
// hover area
    char *tags_hover[] = {x->x_hover_tag, x->x_all_tag};
    pdgui_vmess(0, "crr iiii rs rs rS", cv, "create", "rectangle",
        xpos, ypos, xpos + x->x_w*x->x_zoom, ypos + x->x_h*x->x_zoom,
        "-outline", "", "-fill", "", "-tags", 2, tags_hover);
    button_config_style(x);
    button_config_io(x);
}

static void button_erase(t_button* x, t_glist* glist){
    pdgui_vmess(0, "crs", glist_getcanvas(glist), "delete", x->x_all_tag);
}

static void button_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2);
static void button_select(t_gobj *z, t_glist *glist, int sel);

/* -------- handle (resizing grip) -------- */

static void button_draw_handle(t_button *x, int state){
    t_handle *sh = (t_handle *)x->x_handle;
    if(sh->h_pathname[0])
        pdgui_vmess(0, "rs", "destroy", sh->h_pathname);
    if(state){
        pdgui_vmess(0, "rs riri rkrkrk ri rr",
            "canvas", sh->h_pathname,
            "-width", BUTTON_HANDLE_SIZE, "-height", BUTTON_HANDLE_SIZE,
            "-bg", THISGUI->i_gopcolor,
            "-highlightcolor", THISGUI->i_foregroundcolor,
            "-highlightbackground", THISGUI->i_foregroundcolor,
            "-highlightthickness", 1,
            "-cursor", "bottom_right_corner");
        int x1, y1, x2, y2;
        button_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        t_canvas *cv = glist_getcanvas(x->x_glist);
        char *tags_handle[] = {x->x_tag_handle, x->x_all_tag};
        pdgui_vmess(0, "crr ii rs ri ri rs rS",
            cv, "create", "window",
            x2 - BUTTON_HANDLE_SIZE + 1,
            y2 - BUTTON_HANDLE_SIZE + 1,
            "-anchor", "nw",
            "-width", BUTTON_HANDLE_SIZE,
            "-height", BUTTON_HANDLE_SIZE,
            "-window", sh->h_pathname,
            "-tags", 2, tags_handle);
        char buf[512];
        // NOTE: %%s is Tk's event state mask (passed through pdsend to us)
        snprintf(buf, sizeof(buf),
            "bind %s <Button> {pdsend [concat %s _click 1 [.x%lx.c canvasx %%x] [.x%lx.c canvasy %%y] %%s \\;]}",
            sh->h_pathname, sh->h_bindsym->s_name,
            (unsigned long)cv, (unsigned long)cv);
        pdgui_vmess(buf, NULL);
        snprintf(buf, sizeof(buf),
            "bind %s <ButtonRelease> {pdsend [concat %s _click 0 [.x%lx.c canvasx %%x] [.x%lx.c canvasy %%y] %%s \\;]}",
            sh->h_pathname, sh->h_bindsym->s_name,
            (unsigned long)cv, (unsigned long)cv);
        pdgui_vmess(buf, NULL);
        snprintf(buf, sizeof(buf),
            "bind %s <Motion> {pdsend [concat %s _motion [.x%lx.c canvasx %%x] [.x%lx.c canvasy %%y] %%s \\;]}",
            sh->h_pathname, sh->h_bindsym->s_name,
            (unsigned long)cv, (unsigned long)cv);
        pdgui_vmess(buf, NULL);
    }
}

static void button_vis(t_gobj *z, t_glist *glist, int vis){
    t_button* x = (t_button*)z;
    t_canvas *cv = glist_getcanvas(glist);
    t_handle *sh = (t_handle *)x->x_handle;
    if(x->x_edit && sh->h_pathname[0])
        pdgui_vmess(0, "rs", "destroy", sh->h_pathname);
    if(vis){
        button_draw(x, glist);
        char buf[256];
        snprintf(buf, sizeof(buf),
            ".x%lx.c bind %s <ButtonRelease> {pdsend [concat %s _mouserelease \\;]}",
            (unsigned long)cv, x->x_hover_tag, x->x_bindname->s_name);
        pdgui_vmess(buf, NULL);
        snprintf(buf, sizeof(buf),
            ".x%lx.c bind %s <Enter> {pdsend [concat %s _mouse_enter \\;]}",
            (unsigned long)cv, x->x_hover_tag, x->x_bindname->s_name);
        pdgui_vmess(buf, NULL);
        snprintf(buf, sizeof(buf),
            ".x%lx.c bind %s <Leave> {pdsend [concat %s _mouse_leave \\;]}",
            (unsigned long)cv, x->x_hover_tag, x->x_bindname->s_name);
        pdgui_vmess(buf, NULL);
        // handle
        sprintf(sh->h_pathname, ".x%lx.h%lx",
            (unsigned long)cv, (unsigned long)sh);
        button_draw_handle(x, x->x_edit);
    }
    else
        button_erase(x, glist);
}

static void button_delete(t_gobj *z, t_glist *glist){
    canvas_deletelinesfor(glist, (t_text *)z);
}

static void button_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_button *x = (t_button *)z;
    x->x_obj.te_xpix += dx, x->x_obj.te_ypix += dy;
    dx *= x->x_zoom, dy *= x->x_zoom;
    pdgui_vmess(0, "crs ii", glist_getcanvas(glist), "move", x->x_all_tag, dx, dy);
    canvas_fixlinesfor(glist, (t_text*)x);
}

static void button_select(t_gobj *z, t_glist *glist, int sel){
    t_button *x = (t_button *)z;
    t_handle *sh = (t_handle *)x->x_handle;
    t_canvas *cv = glist_getcanvas(glist);
    x->x_sel = sel;
    // outer button outline
    pdgui_vmess(0, "crs rk", cv, "itemconfigure", x->x_button_tag,
        "-outline", sel ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor);
    // inlet / outlet
    pdgui_vmess(0, "crs rk rk", cv, "itemconfigure", x->x_IO_tag,
        "-outline", sel ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor,
        "-fill", sel ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor);
    // handle
    if(x->x_edit && sh->h_pathname[0]){
        pdgui_vmess(0, "rr rk", sh->h_pathname, "configure",
            "-highlightbackground",
            sel ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor);
        pdgui_vmess(0, "rr rk", sh->h_pathname, "configure",
            "-bg", sel ? THISGUI->i_selectcolor : THISGUI->i_gopcolor);
    }
}

static void button_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2){
    t_button *x = (t_button *)z;
    *xp1 = text_xpix(&x->x_obj, glist);
    *yp1 = text_ypix(&x->x_obj, glist);
    *xp2 = *xp1 + x->x_w*x->x_zoom;
    *yp2 = *yp1 + x->x_h*x->x_zoom;
}

static void button_get_snd(t_button* x){
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
                int arg_n = 10; // send argument number
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

static void button_get_rcv(t_button* x){
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
                int arg_n = 11; // receive argument number
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

static void button_save(t_gobj *z, t_binbuf *b){
    t_button *x = (t_button *)z;
    button_get_snd(x);
    button_get_rcv(x);
    binbuf_addv(b, "ssiisiissiiiiiss", gensym("#X"),gensym("obj"),
        (int)x->x_obj.te_xpix, (int)x->x_obj.te_ypix,
        atom_getsymbol(binbuf_getvec(x->x_obj.te_binbuf)),
        x->x_w, x->x_h,
        x->x_bg, x->x_fg,
        x->x_mode,
        x->x_theme, x->x_transparent,
        x->x_oval, x->x_readonly,
        x->x_snd_raw, x->x_rcv_raw);
  binbuf_addv(b, ";");
}

static void button_mouserelease(t_button* x){
    if(!x->x_glist->gl_edit && x->x_mode == 0){
        outlet_float(x->x_obj.ob_outlet, x->x_state = 0);
        if(x->x_snd->s_thing && !x->x_edit && x->x_snd != gensym("empty") && x->x_snd != &s_)
            pd_float(x->x_snd->s_thing, x->x_state);
        button_setbg(x);
    }
}

void button_mouse_hover(t_button *x, int h){
    if(x->x_readonly)
        return;
    if(x->x_snd_hover->s_thing && x->x_snd != gensym("empty") && x->x_snd != &s_)
        pd_float(x->x_snd_hover->s_thing, h);
}

void button_mouse_enter(t_button *x){
    button_mouse_hover(x, 1);
}

void button_mouse_leave(t_button *x){
    button_mouse_hover(x, 0);
}

void button_snd_config(t_button *x){
    char namebuf[512];
    sprintf(namebuf, "%s-hover", x->x_snd->s_name);
    x->x_snd_hover = gensym(namebuf);
}

static void button_receive(t_button *x, t_symbol *s){
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
        button_config_io(x);
    }
}

static void button_send(t_button *x, t_symbol *s){
    if(s == gensym(""))
        s = gensym("empty");
    t_symbol *snd = s == gensym("empty") ? &s_ : canvas_realizedollar(x->x_glist, s);
    if(snd != x->x_snd){
        x->x_snd_set = 1;
        x->x_snd_raw = s;
        x->x_snd = snd;
        button_snd_config(x);
        button_config_io(x);
    }
}

static void button_flash(t_button *x){
    button_setfg(x);
    clock_delay(x->x_clock, 250);
}

static void button_toggle(t_button *x){
    outlet_float(x->x_obj.ob_outlet, x->x_state = !x->x_state);
    if(x->x_snd->s_thing && !x->x_edit && x->x_snd != gensym("empty") && x->x_snd != &s_)
        pd_float(x->x_snd->s_thing, x->x_state);
    x->x_state ? button_setfg(x) : button_setbg(x);
}

static void button_press(t_button *x){
    if(x->x_mode == 1)
        button_toggle(x);
}

static void button_bang(t_button *x){
    if(x->x_mode == 2){
        outlet_bang(x->x_obj.ob_outlet);
        if(x->x_snd->s_thing && !x->x_edit && x->x_snd != gensym("empty") && x->x_snd != &s_)
            pd_bang(x->x_snd->s_thing);
        button_flash(x);
    }
    else if(x->x_mode < 2){
        outlet_float(x->x_obj.ob_outlet, x->x_state);
        if(x->x_snd->s_thing && !x->x_edit && x->x_snd != gensym("empty") && x->x_snd != &s_)
            pd_float(x->x_snd->s_thing, x->x_state);
    }
}

static void button_set(t_button *x, t_floatarg f){
    if(x->x_mode != 2){
        int state = (int)(f != 0);
        if(x->x_state != state){
            x->x_state = state;
            x->x_state ? button_setfg(x) : button_setbg(x);
        }
    }
}

static void button_float(t_button *x, t_floatarg f){
    if(x->x_mode != 2){
        outlet_float(x->x_obj.ob_outlet, x->x_state = (int)(f != 0));
        if(x->x_snd->s_thing && !x->x_edit && x->x_snd != gensym("empty") && x->x_snd != &s_)
            pd_float(x->x_snd->s_thing, x->x_state);
        x->x_state ? button_setfg(x) : button_setbg(x);
    }
}

static int button_click(t_gobj *z, struct _glist *glist, int xpix, int ypix,
int shift, int alt, int dbl, int doit){
    t_button* x = (t_button *)z;
    if(x->x_readonly)
        return(0);
    int xpos = text_xpix(&x->x_obj, glist), ypos = text_ypix(&x->x_obj, glist);
    x->x_x = (xpix - xpos) / x->x_zoom;
    x->x_y = x->x_h - (ypix - ypos) / x->x_zoom;
    if(doit){
        if(x->x_mode == 0){ // latch
            outlet_float(x->x_obj.ob_outlet, x->x_state = 1);
            if(x->x_snd->s_thing && !x->x_edit && x->x_snd != gensym("empty") && x->x_snd != &s_)
                pd_float(x->x_snd->s_thing, x->x_state);
            button_setfg(x);
        }
        else if(x->x_mode == 1)
            button_toggle(x);
        else if(x->x_mode == 2){
            outlet_bang(x->x_obj.ob_outlet);
            if(x->x_snd->s_thing && !x->x_edit && x->x_snd != gensym("empty") && x->x_snd != &s_)
                pd_bang(x->x_snd->s_thing);
            button_flash(x);
        }
        else if(x->x_mode == 3){
            t_atom at[4];
            SETFLOAT(at+0, doit);
            SETFLOAT(at+1, dbl);
            SETFLOAT(at+2, shift);
            SETFLOAT(at+3, alt);
            outlet_list(x->x_obj.ob_outlet, &s_list, 4, at);
            if(x->x_snd->s_thing && !x->x_edit && x->x_snd != gensym("empty") && x->x_snd != &s_)
                pd_list(x->x_snd->s_thing, &s_list, 4, at);
            button_flash(x);
        }
    }
    return(1);
}

static void button_latch(t_button *x){
    x->x_mode = 0;
}

static void button_tgl(t_button *x){
    x->x_mode = 1;
}

static void button_bng(t_button *x){
    x->x_mode = 2;
}

static void button_clickmode(t_button *x){
    x->x_mode = 3;
}

static void button_config_size(t_button *x){
    int z = x->x_zoom;
    int x1 = text_xpix(&x->x_obj, x->x_glist);
    int y1 = text_ypix(&x->x_obj, x->x_glist);
    int x2 = x1 + x->x_w * z, y2 = y1 + x->x_h * z;
    t_canvas *cv = glist_getcanvas(x->x_glist);
    pdgui_vmess(0, "crs iiii", cv, "coords", x->x_oval_tag,
        x1, y1, x2, y2);
    pdgui_vmess(0, "crs iiii", cv, "coords", x->x_base_tag,
        x1, y1, x2, y2);
    pdgui_vmess(0, "crs iiii", cv, "coords", x->x_hover_tag,
        x1, y1, x2, y2);
    pdgui_vmess(0, "crs iiii", cv, "coords", x->x_out_tag,
        x1, y2 - OHEIGHT*z, x1 + IOWIDTH*z, y2);
}

static void button_size(t_button *x, t_floatarg f){
    int s = f < 12 ? 12 : (int)f;
    if(s != x->x_w || s != x->x_h){
        x->x_w = s; x->x_h = s;
        if(button_vis_check(x)){
            button_config_size(x);
            canvas_fixlinesfor(x->x_glist, (t_text*)x);
        }
    }
}

static void button_dim(t_button *x, t_floatarg f1, t_floatarg f2){
    int w = f1 < 12 ? 12 : (int)f1, h = f2 < 12 ? 12 : (int)f2;
    if(w != x->x_w || h != x->x_h){
        x->x_w = w; x->x_h = h;
        if(button_vis_check(x)){
            button_config_size(x);
            canvas_fixlinesfor(x->x_glist, (t_text*)x);
        }
    }
}

static void button_width(t_button *x, t_floatarg f){
    int w = f < 12 ? 12 : (int)f;
    if(w != x->x_w){
        x->x_w = w;
        if(button_vis_check(x))
            button_config_size(x);
    }
}

static void button_height(t_button *x, t_floatarg f){
    int h = f < 12 ? 12 : (int)f;
    if(h != x->x_h){
        x->x_h = h;
        if(button_vis_check(x)){
            button_config_size(x);
            canvas_fixlinesfor(x->x_glist, (t_text*)x);
        }
    }
}

/* ---------------- colors ---------------- */

static void button_fgcolor(t_button *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(!ac)
        return;
    button_send_color_validate(x, button_getcolor(ac, av)->s_name, 1);
}

static void button_setfgcolor(t_button *x, t_symbol *s, int ac, t_atom *av){
    x->x_theme = 0;
    button_fgcolor(x, s, ac, av);
}

static void button_bgcolor(t_button *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(!ac)
        return;
    button_send_color_validate(x, button_getcolor(ac, av)->s_name, 0);
}

static void button_setbgcolor(t_button *x, t_symbol *s, int ac, t_atom *av){
    x->x_theme = 0;
    button_bgcolor(x, s, ac, av);
}

static void button_colors(t_button *x, t_symbol *bg, t_symbol *fg){
    t_atom at[1];
    SETSYMBOL(at, bg);
    button_bgcolor(x, NULL, 1, at);
    SETSYMBOL(at, fg);
    button_fgcolor(x, NULL, 1, at);
}

static void button_transparent(t_button *x, t_floatarg f){
    if((int)f == x->x_transparent)
        return;
    x->x_transparent = f < 0 ? 0 : f > 3 ? 3 : (int)(f);
    if(!button_vis_check(x))
        return;
    button_config_style(x);
    x->x_state ? button_setfg(x) : button_setbg(x);
}

static void button_theme(t_button *x, t_floatarg f){
    int theme = (int)(f != 0);
    if(theme == x->x_theme)
        return;
    x->x_theme = theme;
    if(!button_vis_check(x))
        return;
    button_config_style(x);
    x->x_state ? button_setfg(x) : button_setbg(x);
}

static void button_readonly(t_button *x, t_floatarg f){
    x->x_readonly = (int)(f != 0);
}

static void button_oval(t_button *x, t_floatarg f){
    x->x_oval = (int)(f != 0);
    if(!button_vis_check(x))
        return;
    button_config_style(x);
    x->x_state ? button_setfg(x) : button_setbg(x);
}

static void button_zoom(t_button *x, t_floatarg zoom){
    x->x_zoom = (int)zoom;
}

static t_symbol *button_getmode(t_button *x){
    switch(x->x_mode){
        case 1: return(gensym("Toggle"));
        case 2: return(gensym("Bang"));
        case 3: return(gensym("Click"));
        default: return(gensym("Latch"));
    }
}

static void button_properties(t_gobj *z, t_glist *owner){
    (void)owner;
    t_button *x = (t_button *)z;
    char pd_bg[32];
    snprintf(pd_bg, sizeof(pd_bg), "#%06X", THISGUI->i_backgroundcolor);
    char pd_fg[32];
    snprintf(pd_fg, sizeof(pd_fg), "#%06X", THISGUI->i_foregroundcolor);
    // display raw symbols for rcv/snd; fall back to actual / "empty"
    t_symbol *snd_disp = x->x_snd_raw != &s_ ? x->x_snd_raw
        : (x->x_snd != &s_ ? x->x_snd : gensym("empty"));
    t_symbol *rcv_disp = x->x_rcv_raw != &s_ ? x->x_rcv_raw
        : (x->x_rcv != &s_ ? x->x_rcv : gensym("empty"));
    t_symbol *mode_sym = button_getmode(x);
    pdgui_stub_vnew(&x->x_obj.ob_pd, "button_dialog", x,
        "ii si ss ssss ii",
        x->x_w, x->x_h,
        mode_sym->s_name, x->x_oval,
        rcv_disp->s_name, snd_disp->s_name,
        x->x_bg->s_name, x->x_fg->s_name,
        pd_bg, pd_fg,
        x->x_theme, x->x_transparent);
}

static void button_apply(t_button *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_atom undo[10];
    SETFLOAT(undo+0, x->x_w);
    SETFLOAT(undo+1, x->x_h);
    SETSYMBOL(undo+2, button_getmode(x));
    SETFLOAT(undo+3, x->x_oval);
    SETSYMBOL(undo+4, x->x_rcv_raw != &s_ ? x->x_rcv_raw : gensym("empty"));
    SETSYMBOL(undo+5, x->x_snd_raw != &s_ ? x->x_snd_raw : gensym("empty"));
    SETSYMBOL(undo+6, x->x_bg);
    SETSYMBOL(undo+7, x->x_fg);
    SETFLOAT(undo+8, x->x_theme);
    SETFLOAT(undo+9, x->x_transparent);
    pd_undo_set_objectstate(x->x_glist, (t_pd*)x, gensym("dialog"), 10, undo, ac, av);
    // read incoming values
    int w = atom_getintarg(0, ac, av);
    int h = atom_getintarg(1, ac, av);
    t_symbol *mode = atom_getsymbolarg(2, ac, av);
    int oval = atom_getintarg(3, ac, av);
    t_symbol *rcv = atom_getsymbolarg(4, ac, av);
    t_symbol *snd = atom_getsymbolarg(5, ac, av);
    t_symbol *bg = atom_getsymbolarg(6, ac, av);
    t_symbol *fg = atom_getsymbolarg(7, ac, av);
    int theme = atom_getintarg(8, ac, av);
    int transp = atom_getintarg(9, ac, av);
    // apply dimensions / colors / theme / transparency
    x->x_w = w < 12 ? 12 : w;
    x->x_h = h < 12 ? 12 : h;
    x->x_bg = bg;
    x->x_fg = fg;
    x->x_theme = (theme != 0);
    x->x_transparent = transp < 0 ? 0 : transp > 3 ? 3 : transp;
    // apply mode (symbol → int; unknown symbol keeps current mode)
    int mode_int = x->x_mode;
    if(mode == gensym("Latch"))       mode_int = 0;
    else if(mode == gensym("Toggle")) mode_int = 1;
    else if(mode == gensym("Bang"))   mode_int = 2;
    else if(mode == gensym("Click"))  mode_int = 3;
    x->x_mode = mode_int;
    x->x_oval = (oval != 0);
    // send/receive (handles bind/unbind + config_io)
    button_receive(x, rcv);
    button_send(x, snd);
    // redraw
    if(button_vis_check(x)){
        button_erase(x, x->x_glist);
        button_draw(x, x->x_glist);
        canvas_fixlinesfor(x->x_glist, (t_text*)x);
        button_draw_handle(x, x->x_edit);
    }
    canvas_dirty(x->x_glist, 1);
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
            button_draw_handle(p->p_cnv, edit);
            button_config_io(p->p_cnv);
            button_config_style(p->p_cnv);
        }
    }
}

static void button_free(t_button *x){
    pd_unbind(&x->x_obj.ob_pd, x->x_bindname);
    if(x->x_handle){
        pd_unbind(x->x_handle, ((t_handle *)x->x_handle)->h_bindsym);
        pd_free(x->x_handle);
    }
    x->x_proxy->p_cnv = NULL;
    clock_free(x->x_proxy->p_clock);
    clock_free(x->x_clock);
    pdgui_stub_deleteforkey(x);
}

static void edit_proxy_free(t_edit_proxy *p){
    pd_unbind(&p->p_obj.ob_pd, p->p_sym);
    clock_free(p->p_clock);
    pd_free(&p->p_obj.ob_pd);
}

static t_edit_proxy * edit_proxy_new(t_button *x, t_symbol *s){
    t_edit_proxy *p = (t_edit_proxy*)pd_new(edit_proxy_class);
    p->p_cnv = x;
    pd_bind(&p->p_obj.ob_pd, p->p_sym = s);
    p->p_clock = clock_new(p, (t_method)edit_proxy_free);
    return(p);
}

static void *button_new(t_symbol *s, int ac, t_atom *av){
    t_button *x = (t_button *)pd_new(button_class);
    // handle
    x->x_handle = pd_new(handle_class);
    t_handle *sh = (t_handle *)x->x_handle;
    sh->h_master = x;
    char hbuf[64];
    sprintf(hbuf, "_h%lx", (unsigned long)sh);
    pd_bind(x->x_handle, sh->h_bindsym = gensym(hbuf));
    sprintf(sh->h_outlinetag, "h%lx", (unsigned long)sh);
    sh->h_dragon = 0;
    sh->h_drag_delta_w = sh->h_drag_delta_h = 0;
    sh->h_dragx = sh->h_dragy = 0;
    sh->h_anchor_w = sh->h_anchor_h = 0;
    sh->h_last_shift = -1;
    sh->h_pathname[0] = 0;
    //
    x->x_clock = clock_new(x, (t_method)button_setbg);
    t_canvas *cv = canvas_getcurrent();
    x->x_glist = (t_glist*)cv;
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING-1, ".x%lx", (unsigned long)cv);
    buf[MAXPDSTRING-1] = 0;
    x->x_proxy = edit_proxy_new(x, gensym(buf));
    sprintf(buf, "#%lx", (long)x);
    pd_bind(&x->x_obj.ob_pd, x->x_bindname = gensym(buf));
    x->x_edit = cv->gl_edit; x->x_zoom = x->x_glist->gl_zoom;
    x->x_x = x->x_y = 0;
    x->x_fg = gensym("#808080"), x->x_bg = gensym("#FFFFFF");
    t_symbol *snd = gensym("empty");
    t_symbol *rcv = gensym("empty");
    x->x_theme = x->x_mode = x->x_readonly = x->x_transparent = 0;
    int w = 20, h = 20;
    if(ac && av->a_type == A_FLOAT){
        w = av->a_w.w_float; // width
        ac--; av++;
        if(!ac) goto end;
        h = av->a_w.w_float; // height
        ac--, av++;
        if(!ac) goto end;
        if(av->a_type == A_FLOAT && ac >= 6) // Backward compat with the old color format
        {
            uint8_t bgcolor_rgb[3];
            uint8_t fgcolor_rgb[3];

            for (int i = 0; i < 3; i++) {
                bgcolor_rgb[i] = (uint8_t)av->a_w.w_float; ac--; av++;
            }
            for (int i = 0; i < 3; i++) {
                fgcolor_rgb[i] = (uint8_t)av->a_w.w_float; ac--; av++;
            }
            
            char bgstr[20];
            char fgstr[20];
            snprintf(bgstr, 20, "#%.2x%.2x%.2x",  bgcolor_rgb[0], bgcolor_rgb[1], bgcolor_rgb[2]);
            snprintf(fgstr, 20, "#%.2x%.2x%.2x", fgcolor_rgb[0], fgcolor_rgb[1], fgcolor_rgb[2]);

            // Assign to t_symbol
            x->x_bg = gensym(bgstr);
            x->x_fg = gensym(fgstr);
        }
        else if(av->a_type == A_SYMBOL && ac >= 2) {
            x->x_bg = av->a_w.w_symbol;  // BG
            ac--, av++;
            if(!ac) goto end;
            x->x_fg = av->a_w.w_symbol;  // FG
            ac--, av++;
        }
        else goto errstate;
        
        if(!ac) goto end;
        int mode = (int)av->a_w.w_float;  // mode
        x->x_mode = mode < 0 ? 0 : mode > 3 ? 3 : mode;
        ac--, av++;
        if(!ac) goto end;
        x->x_theme = (int)(av->a_w.w_float != 0); // theme
        ac--, av++;
        if(!ac) goto end;
        x->x_transparent = (int)(av->a_w.w_float); // transparent
        ac--, av++;
        if(!ac) goto end;
        x->x_oval = (int)(av->a_w.w_float != 0); // outline
        ac--, av++;
        if(!ac) goto end;
        x->x_readonly = (int)(av->a_w.w_float != 0); // readonly
        ac--, av++;
        if(!ac) goto end;
        snd = av->a_w.w_symbol;  // snd
        ac--, av++;
        if(!ac) goto end;
        rcv = av->a_w.w_symbol;  // rcv
        goto end;
    }
    while(ac > 0){
        if(av->a_type == A_SYMBOL){
            s = atom_getsymbolarg(0, ac, av);
            if(s == gensym("-dim")){
                if(ac >= 3 && (av+1)->a_type == A_FLOAT && (av+2)->a_type == A_FLOAT){
                    w = atom_getfloatarg(1, ac, av);
                    h = atom_getfloatarg(2, ac, av);
                    ac-=3, av+=3;
                    x->x_flag = 1;
                }
                else goto errstate;
            }
            else if(s == gensym("-tgl")){
                x->x_mode = 1;
                ac--, av++;
                x->x_flag = 1;
            }
            else if(s == gensym("-bng")){
                x->x_mode = 2;
                ac--, av++;
                x->x_flag = 1;
            }
            else if(s == gensym("-click")){
                x->x_mode = 3;
                ac--, av++;
                x->x_flag = 1;
            }
            else if(s == gensym("-transparent")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    int t = atom_getint(av);
                    x->x_transparent = t < 0 ? 0 : t > 3 ? 3 : t;
                    ac-=2, av+=2;
                    x->x_flag = 1;
                }
                else goto errstate;
            }
            else if(s == gensym("-send")){
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
            else if(s == gensym("-receive")){
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
            else if(s == gensym("-notheme")){
                x->x_flag = 1;
                x->x_theme = 0;
                ac--, av++;
            }
            else if(s == gensym("-oval")){
                x->x_flag = 1;
                x->x_oval = 1;
                ac--, av++;
            }
            else if(s == gensym("-readonly")){
                x->x_readonly = 1;
                ac--, av++;
                x->x_flag = 1;
            }
            else if(s == gensym("-size")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    w = h = atom_getfloat(av+1);
                    ac-=2, av+=2;
                    x->x_flag = 1;
                }
                else goto errstate;
            }
            else if(s == gensym("-bgcolor")){
                if(ac >= 2 && (av+1)->a_type == A_SYMBOL){
                    x->x_bg = atom_getsymbol(av+1);
                    ac-=2, av+=2;
                    x->x_flag = 1;
                }
                else goto errstate;
            }
            else if(s == gensym("-fgcolor")){
                if(ac >= 2 && (av+1)->a_type == A_SYMBOL){
                    x->x_fg = atom_getsymbol(av+1);
                    ac-=2, av+=2;
                    x->x_flag = 1;
                }
                else goto errstate;
            }
            else goto errstate;
        }
        else goto errstate;
    }
    end:
    x->x_snd = canvas_realizedollar(x->x_glist, x->x_snd_raw = snd);
    x->x_rcv = canvas_realizedollar(x->x_glist, x->x_rcv_raw = rcv);
    button_snd_config(x);
    x->x_w = w, x->x_h = h;
    outlet_new(&x->x_obj, &s_anything);
    sprintf(x->x_base_tag, "%pBASE", x);
    sprintf(x->x_oval_tag, "%pOVAL", x);
    sprintf(x->x_button_tag, "%pBUTTON", x);
    sprintf(x->x_all_tag, "%pALL", x);
    sprintf(x->x_IO_tag, "%pIO", x);
    sprintf(x->x_in_tag, "%pIN", x);
    sprintf(x->x_out_tag, "%pOUT", x);
    sprintf(x->x_hover_tag, "%pHOVER", x);
    sprintf(x->x_tag_handle, "%pHANDLE", x);
    if(x->x_rcv != gensym("empty") && x->x_rcv != &s_)
        pd_bind(&x->x_obj.ob_pd, x->x_rcv);
    return(x);
    errstate:
        pd_error(x, "[button]: improper args");
        return(NULL);
}

/* -------- handle callbacks -------- */

/* Click: f=1 (press) / f=0 (release); xarg,yarg = canvas coords; state = Tk event state mask */
static void handle__click_callback(t_handle *sh, t_floatarg f, t_floatarg xarg, t_floatarg yarg, t_floatarg state){
    int click = (int)f;
    t_button *x = sh->h_master;
    if(sh->h_dragon && click == 0){
        char buf[128];
        snprintf(buf, sizeof(buf),
            ".x%lx.c delete %s",
            (unsigned long)glist_getcanvas(x->x_glist), sh->h_outlinetag);
        pdgui_vmess(buf, NULL);
        int new_w = x->x_w + sh->h_drag_delta_w;
        int new_h = x->x_h + sh->h_drag_delta_h;
        button_dim(x, new_w, new_h);
        button_draw_handle(x, 1);
        button_select((t_gobj *)x, x->x_glist, x->x_sel);
        canvas_dirty(x->x_glist, 1);
    }
    else if(!sh->h_dragon && click){
        int x1, y1, x2, y2;
        button_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        t_canvas *cv = glist_getcanvas(x->x_glist);
        pdgui_vmess(0, "crr iiii rk ri rs",
            cv, "create", "rectangle",
            x1, y1, x2, y2,
            "-outline", THISGUI->i_selectcolor,
            "-width", 2,
            "-tags", sh->h_outlinetag);
        pdgui_vmess(0, "rr rk",  sh->h_pathname, "configure",
            "-highlightcolor", THISGUI->i_selectcolor);
        // Initialize anchor at press
        sh->h_dragx = (int)xarg;
        sh->h_dragy = (int)yarg;
        sh->h_anchor_w = x->x_w;
        sh->h_anchor_h = x->x_h;
        sh->h_drag_delta_w = 0;
        sh->h_drag_delta_h = 0;
        sh->h_last_shift = ((int)state & SHIFT_MASK) ? 1 : 0;
    }
    sh->h_dragon = click;
}

/* Motion: xarg,yarg = canvas coords; state = Tk event state mask (bit 0 = Shift) */
static void handle__motion_callback(t_handle *sh, t_floatarg xarg, t_floatarg yarg, t_floatarg state){
    if(!sh->h_dragon)
        return;
    t_button *x = sh->h_master;
    int shift = ((int)state & SHIFT_MASK) ? 1 : 0;
    int zoom = (x->x_zoom > 0 ? x->x_zoom : 1);

    // If Shift state toggled mid-drag, re-anchor to current preview size + mouse pos
    // so the visual preview does not jump.
    if(shift != sh->h_last_shift){
        sh->h_anchor_w = x->x_w + sh->h_drag_delta_w;
        sh->h_anchor_h = x->x_h + sh->h_drag_delta_h;
        sh->h_dragx = (int)xarg;
        sh->h_dragy = (int)yarg;
        sh->h_last_shift = shift;
    }

    int dx = (int)xarg - sh->h_dragx;   // canvas pixels (zoomed)
    int dy = (int)yarg - sh->h_dragy;
    int new_w, new_h;
    if(shift){ // independent W/H
        new_w = sh->h_anchor_w + dx / zoom;
        new_h = sh->h_anchor_h + dy / zoom;
    }
    else{ // proportional (average both deltas, like knob)
        int delta = (dx + dy) / 2;
        new_w = sh->h_anchor_w + delta / zoom;
        new_h = sh->h_anchor_h + delta / zoom;
    }
    if(new_w < 12) new_w = 12;
    if(new_h < 12) new_h = 12;
    sh->h_drag_delta_w = new_w - x->x_w;
    sh->h_drag_delta_h = new_h - x->x_h;

    // Preview rectangle
    int x1, y1, x2, y2;
    button_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
    (void)x2; (void)y2;
    char buf[128];
    snprintf(buf, sizeof(buf),
        ".x%lx.c coords %s %d %d %d %d",
        (unsigned long)glist_getcanvas(x->x_glist), sh->h_outlinetag,
        x1, y1,
        x1 + new_w * zoom,
        y1 + new_h * zoom);
    pdgui_vmess(buf, NULL);
}

void button_setup(void){
    button_class = class_new(gensym("button"), (t_newmethod)button_new,
        (t_method)button_free, sizeof(t_button), 0, A_GIMME, 0);
    class_addbang(button_class, button_bang);
    class_addfloat(button_class, button_float);
    class_addmethod(button_class, (t_method)button_dim, gensym("dim"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(button_class, (t_method)button_size, gensym("size"), A_FLOAT, 0);
    class_addmethod(button_class, (t_method)button_width, gensym("width"), A_FLOAT, 0);
    class_addmethod(button_class, (t_method)button_height, gensym("height"), A_FLOAT, 0);
    class_addmethod(button_class, (t_method)button_set, gensym("set"), A_FLOAT, 0);
    class_addmethod(button_class, (t_method)button_bng, gensym("bng"), 0);
    class_addmethod(button_class, (t_method)button_clickmode, gensym("click"), 0);
    class_addmethod(button_class, (t_method)button_press, gensym("toggle"), 0);
    class_addmethod(button_class, (t_method)button_tgl, gensym("tgl"), 0);
    class_addmethod(button_class, (t_method)button_latch, gensym("latch"), 0);
    class_addmethod(button_class, (t_method)button_setbgcolor, gensym("bgcolor"), A_GIMME, 0);
    class_addmethod(button_class, (t_method)button_setfgcolor, gensym("fgcolor"), A_GIMME, 0);
    class_addmethod(button_class, (t_method)button_colors, gensym("colors"), A_SYMBOL, A_SYMBOL, 0);
    class_addmethod(button_class, (t_method)button_validate, gensym("_validate"),
        A_FLOAT, A_SYMBOL, A_FLOAT, 0);
    class_addmethod(button_class, (t_method)button_readonly, gensym("readonly"), A_FLOAT, 0);
    class_addmethod(button_class, (t_method)button_oval, gensym("oval"), A_FLOAT, 0);
    class_addmethod(button_class, (t_method)button_transparent, gensym("transparent"), A_FLOAT, 0);
    class_addmethod(button_class, (t_method)button_theme, gensym("theme"), A_FLOAT, 0);
    class_addmethod(button_class, (t_method)button_send, gensym("send"), A_DEFSYM, 0);
    class_addmethod(button_class, (t_method)button_receive, gensym("receive"), A_DEFSYM, 0);
    class_addmethod(button_class, (t_method)button_zoom, gensym("zoom"), A_CANT, 0);
    class_addmethod(button_class, (t_method)button_mouserelease, gensym("_mouserelease"), 0);
    class_addmethod(button_class, (t_method)button_mouse_enter, gensym("_mouse_enter"), 0);
    class_addmethod(button_class, (t_method)button_mouse_leave, gensym("_mouse_leave"), 0);
    class_addmethod(button_class, (t_method)button_apply, gensym("dialog"), A_GIMME, 0);
    edit_proxy_class = class_new(0, 0, 0, sizeof(t_edit_proxy), CLASS_NOINLET | CLASS_PD, 0);
    class_addanything(edit_proxy_class, edit_proxy_any);
    handle_class = class_new(gensym("_handle"), 0, 0, sizeof(t_handle), CLASS_PD, 0);
    class_addmethod(handle_class, (t_method)handle__click_callback, gensym("_click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(handle_class, (t_method)handle__motion_callback, gensym("_motion"),
        A_FLOAT, A_FLOAT, A_FLOAT, 0);
    button_widgetbehavior.w_getrectfn  = button_getrect;
    button_widgetbehavior.w_displacefn = button_displace;
    button_widgetbehavior.w_selectfn   = button_select;
    button_widgetbehavior.w_activatefn = NULL;
    button_widgetbehavior.w_deletefn   = button_delete;
    button_widgetbehavior.w_visfn      = button_vis;
    button_widgetbehavior.w_clickfn    = button_click;
    class_setsavefn(button_class, button_save);
    class_setpropertiesfn(button_class, button_properties);
    class_setwidget(button_class, &button_widgetbehavior);
    #include "../Extra/else_tcl_utils.h"
    pdgui_vmess(else_tcl_utils, NULL);
    #include "../Extra/button_dialog.h"
    pdgui_vmess(button_dialog_tcl, NULL);
}
