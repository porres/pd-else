// porres 2020-2025

#include <m_pd.h>
#include <g_canvas.h>

static t_class *button_class, *edit_proxy_class;
static t_widgetbehavior button_widgetbehavior;

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
}t_button;

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
    if(button_vis_check(x))
        pdgui_vmess(0, "crs rs", glist_getcanvas(x->x_glist), "itemconfigure",
            x->x_oval ? x->x_oval_tag : x->x_base_tag,
            "-fill", x->x_transparent > 1 ? "" : x->x_fg->s_name);
}

static void button_setbg(t_button *x){
    if(button_vis_check(x))
        pdgui_vmess(0, "crs rs", glist_getcanvas(x->x_glist), "itemconfigure",
            x->x_oval ? x->x_oval_tag : x->x_base_tag,
            "-fill", x->x_transparent ? "" : x->x_bg->s_name);
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
        char color[32];
        snprintf(color, sizeof(color), "%s",
            x->x_state ? x->x_transparent > 1 ? "" :
            x->x_fg->s_name : x->x_transparent ? "" :
            x->x_bg->s_name);
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
    char color[32];
    snprintf(color, sizeof(color), "%s",
        x->x_state ? transp > 1 ? "" : x->x_fg->s_name : transp ? "" : x->x_bg->s_name);
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

static void button_vis(t_gobj *z, t_glist *glist, int vis){
    t_button* x = (t_button*)z;
    t_canvas *cv = glist_getcanvas(glist);
    if(vis){
        button_draw(x, glist);
        sys_vgui(".x%lx.c bind %s <ButtonRelease> {pdsend [concat %s _mouserelease \\;]}\n",
            cv, x->x_hover_tag, x->x_bindname->s_name);
        sys_vgui(".x%lx.c bind %s <Enter> {pdsend [concat %s _mouse_enter \\;]}\n",
            cv, x->x_hover_tag, x->x_bindname->s_name);
        sys_vgui(".x%lx.c bind %s <Leave> {pdsend [concat %s _mouse_leave \\;]}\n",
            cv, x->x_hover_tag, x->x_bindname->s_name);
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
    pdgui_vmess(0, "crs rk", glist_getcanvas(glist), "itemconfigure", x->x_button_tag,
        "-outline", (x->x_sel = sel) ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor);
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

static void button_fgcolor(t_button *x, t_symbol *s, int ac, t_atom *av){
    t_symbol *color = s = button_getcolor(ac, av);
    if(color == x->x_fg)
        return;
    x->x_fg = color;
    if(x->x_state && button_vis_check(x))
        button_setfg(x);
}

static void button_bgcolor(t_button *x, t_symbol *s, int ac, t_atom *av){
    t_symbol *color = s = button_getcolor(ac, av);
    if(color == x->x_bg)
        return;
    x->x_bg = color;
    if(!x->x_state && button_vis_check(x))
        button_setbg(x);
}

static void button_transparent(t_button *x, t_floatarg f){
    x->x_transparent = f < 0 ? 0 : f > 3 ? 3 : (int)(f);
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
            button_config_io(p->p_cnv);
            button_config_style(p->p_cnv);
        }
    }
}

static void button_free(t_button *x){
    pd_unbind(&x->x_obj.ob_pd, x->x_bindname);
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
    x->x_theme = 1;
    x->x_mode = x->x_readonly = x->x_transparent = 0;
    int w = 20, h = 20;
    if(ac && av->a_type == A_FLOAT){
        w = av->a_w.w_float; // width
        ac--; av++;
        if(!ac) goto end;
        h = av->a_w.w_float; // height
        ac--, av++;
        if(!ac) goto end;
        x->x_bg = av->a_w.w_symbol;  // BG
        ac--, av++;
        if(!ac) goto end;
        x->x_fg = av->a_w.w_symbol;  // FG
        ac--, av++;
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
                    w = h = atom_getfloat(av);
                    ac-=2, av+=2;
                    x->x_flag = 1;
                }
                else goto errstate;
            }
            else if(s == gensym("-bgcolor")){
                if(ac >= 2 && (av+1)->a_type == A_SYMBOL){
                    x->x_bg = atom_getsymbol(av);
                    ac-=2, av+=2;
                    x->x_flag = 1;
                }
                else goto errstate;
            }
            else if(s == gensym("-fgcolor")){
                if(ac >= 2 && (av+1)->a_type == A_SYMBOL){
                    x->x_fg = atom_getsymbol(av);
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
    if(x->x_rcv != gensym("empty"))
        pd_bind(&x->x_obj.ob_pd, x->x_rcv);
    return(x);
    errstate:
        pd_error(x, "[button]: improper args");
        return(NULL);
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
    class_addmethod(button_class, (t_method)button_bgcolor, gensym("bgcolor"), A_GIMME, 0);
    class_addmethod(button_class, (t_method)button_fgcolor, gensym("fgcolor"), A_GIMME, 0);
    class_addmethod(button_class, (t_method)button_readonly, gensym("readonly"), A_FLOAT, 0);
    class_addmethod(button_class, (t_method)button_oval, gensym("oval"), A_FLOAT, 0);
    class_addmethod(button_class, (t_method)button_transparent, gensym("transparent"), A_FLOAT, 0);
    class_addmethod(button_class, (t_method)button_send, gensym("send"), A_DEFSYM, 0);
    class_addmethod(button_class, (t_method)button_receive, gensym("receive"), A_DEFSYM, 0);
    class_addmethod(button_class, (t_method)button_zoom, gensym("zoom"), A_CANT, 0);
    class_addmethod(button_class, (t_method)button_mouserelease, gensym("_mouserelease"), 0);
    class_addmethod(button_class, (t_method)button_mouse_enter, gensym("_mouse_enter"), 0);
    class_addmethod(button_class, (t_method)button_mouse_leave, gensym("_mouse_leave"), 0);
    edit_proxy_class = class_new(0, 0, 0, sizeof(t_edit_proxy), CLASS_NOINLET | CLASS_PD, 0);
    class_addanything(edit_proxy_class, edit_proxy_any);
    button_widgetbehavior.w_getrectfn  = button_getrect;
    button_widgetbehavior.w_displacefn = button_displace;
    button_widgetbehavior.w_selectfn   = button_select;
    button_widgetbehavior.w_activatefn = NULL;
    button_widgetbehavior.w_deletefn   = button_delete;
    button_widgetbehavior.w_visfn      = button_vis;
    button_widgetbehavior.w_clickfn    = button_click;
    class_setsavefn(button_class, button_save);
    class_setwidget(button_class, &button_widgetbehavior);
}
