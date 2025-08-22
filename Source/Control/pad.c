// porres 2020-2025

#include <m_pd.h>
#include <g_canvas.h>

static t_class *pad_class, *edit_proxy_class;
static t_widgetbehavior pad_widgetbehavior;

typedef struct _edit_proxy{
    t_object    p_obj;
    t_symbol   *p_sym;
    t_clock    *p_clock;
    struct      _pad *p_cnv;
}t_edit_proxy;

typedef struct _pad{
    t_object        x_obj;
    t_glist        *x_glist;
    t_edit_proxy   *x_proxy;
    t_symbol       *x_bindname;
    t_symbol       *x_bg;
    int             x_transparent;
    int             x_x;
    int             x_y;
    int             x_w;
    int             x_h;
    int             x_sel;
    int             x_zoom;
    int             x_edit;
    char            x_base_tag[32];
    char            x_all_tag[32];
    char            x_IOlet_tag[32];
    char            x_outlet_tag[32];
    unsigned char   x_color[3];
}t_pad;

static int pad_vis_check(t_pad *x){
    return(gobj_shouldvis((t_gobj *)x, x->x_glist) && glist_isvisible(x->x_glist));
}

static t_symbol *pad_getcolor(t_pad* x, int ac, t_atom *av){
    if((av)->a_type == A_SYMBOL)
        return(atom_getsymbol(av));
    else{
        int r = atom_getintarg(0, ac, av);
        int g = atom_getintarg(1, ac, av);
        int b = atom_getintarg(2, ac, av);
        x->x_color[0] = r = r < 0 ? 0 : r > 255 ? 255 : r;
        x->x_color[1] = g = g < 0 ? 0 : g > 255 ? 255 : g;
        x->x_color[2] = b = b < 0 ? 0 : b > 255 ? 255 : b;
        char color[8];
        sprintf(color, "#%.2x%.2x%.2x", r, g, b);
        return(gensym(color));
    }
}

static void pad_config_io_let(t_pad *x){
    pdgui_vmess(0, "crs rs", glist_getcanvas(x->x_glist),
        "itemconfigure", x->x_IOlet_tag,
        "-state", x->x_edit ? "normal" : "hidden");
}

static void pad_draw(t_pad *x, t_glist *glist){
    int xpos = text_xpix(&x->x_obj, glist), ypos = text_ypix(&x->x_obj, glist);
    t_canvas *cv = glist_getcanvas(glist);
    int z = x->x_zoom;
    // draw rectangle
    char *tags_base[] = {x->x_base_tag, x->x_all_tag};
    pdgui_vmess(0, "crr iiii ri rk rs rS", cv, "create", "rectangle",
        xpos, ypos, xpos + x->x_w*z, ypos + x->x_h*z,
        "-width", z,
        "-outline", x->x_sel ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor,
        "-fill", x->x_transparent ? "" : x->x_bg->s_name,
        "-tags", 2, tags_base);
    // draw inlet
    char *tags_inlet[] = {x->x_IOlet_tag, x->x_all_tag};
    pdgui_vmess(0, "crr iiii rs rs rS", cv, "create", "rectangle",
        xpos, ypos, xpos+(IOWIDTH*z), ypos+(IHEIGHT*z),
        "-fill", "black",
        "-state", x->x_edit ? "hidden" : "normal",
        "-tags", 2, tags_inlet);
    // draw outlet
    char *tags_outlet[] = {x->x_IOlet_tag, x->x_outlet_tag, x->x_all_tag};
    pdgui_vmess(0, "crr iiii rs rs rS", cv, "create", "rectangle",
        xpos, ypos+x->x_h*z, xpos+IOWIDTH*z, ypos+x->x_h*z-IHEIGHT*z,
        "-fill", "black",
        "-state", x->x_edit ? "hidden" : "normal",
        "-tags", 3, tags_outlet);
}

static void pad_vis(t_gobj *z, t_glist *glist, int vis){
    t_pad* x = (t_pad*)z;
    t_canvas *cv = glist_getcanvas(glist);
    if(vis){
        pad_draw(x, glist);
        sys_vgui(".x%lx.c bind %lxPADBASE <ButtonRelease> {pdsend [concat %s _mouserelease \\;]}\n",
            cv, x, x->x_bindname->s_name);
    }
    else // erase
        pdgui_vmess(0, "crs", glist_getcanvas(glist), "delete", x->x_all_tag);
}

static void pad_delete(t_gobj *z, t_glist *glist){
    canvas_deletelinesfor(glist, (t_text *)z);
}

static void pad_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_pad *x = (t_pad *)z;
    x->x_obj.te_xpix += dx, x->x_obj.te_ypix += dy;
    dx *= x->x_zoom, dy *= x->x_zoom;
    pdgui_vmess(0, "crs ii", glist_getcanvas(glist), "move", x->x_all_tag, dx, dy);
    canvas_fixlinesfor(glist, (t_text*)x);
}

static void pad_select(t_gobj *z, t_glist *glist, int sel){
    t_pad *x = (t_pad *)z;
    t_canvas *cv = glist_getcanvas(glist);
    x->x_sel = sel;
    pdgui_vmess(0, "crs rk", cv, "itemconfigure", x->x_base_tag, "-outline",
        sel ? THISGUI->i_selectcolor :  THISGUI->i_foregroundcolor);
}

static void pad_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2){
    t_pad *x = (t_pad *)z;
    *xp1 = text_xpix(&x->x_obj, glist);
    *yp1 = text_ypix(&x->x_obj, glist);
    *xp2 = *xp1 + x->x_w*x->x_zoom;
    *yp2 = *yp1 + x->x_h*x->x_zoom;
}

static void pad_save(t_gobj *z, t_binbuf *b){
  t_pad *x = (t_pad *)z;
  binbuf_addv(b, "ssiisii iii i", gensym("#X"),gensym("obj"),
    (int)x->x_obj.te_xpix, (int)x->x_obj.te_ypix,
    atom_getsymbol(binbuf_getvec(x->x_obj.te_binbuf)),
    x->x_w, x->x_h,
    x->x_color[0], x->x_color[1], x->x_color[2],
    x->x_transparent);
  binbuf_addv(b, ";");
}

static void pad_mouserelease(t_pad* x){
    if(!x->x_glist->gl_edit){ // ignore if edit mode!
        t_atom at[1];
        SETFLOAT(at, 0);
        outlet_anything(x->x_obj.ob_outlet, gensym("click"), 1, at);
    }
}

static void pad_motion(t_pad *x, t_floatarg dx, t_floatarg dy){
    x->x_x += (int)(dx/x->x_zoom);
    x->x_y -= (int)(dy/x->x_zoom);
    t_atom at[2];
    SETFLOAT(at, (t_float)x->x_x);
    SETFLOAT(at+1, (t_float)x->x_y);
    outlet_anything(x->x_obj.ob_outlet, &s_list, 2, at);
}

static int pad_click(t_gobj *z, struct _glist *glist, int xpix, int ypix,
int shift, int alt, int dbl, int doit){
    dbl = shift = alt = 0;
    t_pad* x = (t_pad *)z;
    t_atom at[3];
    int xpos = text_xpix(&x->x_obj, glist), ypos = text_ypix(&x->x_obj, glist);
    x->x_x = (xpix - xpos) / x->x_zoom;
    x->x_y = x->x_h - (ypix - ypos) / x->x_zoom;
    if(doit){
        SETFLOAT(at, (float)doit);
        outlet_anything(x->x_obj.ob_outlet, gensym("click"), 1, at);
        glist_grab(x->x_glist, &x->x_obj.te_g,
            (t_glistmotionfn)pad_motion, 0, (float)xpix, (float)ypix);
    }
    else{
        SETFLOAT(at, (float)x->x_x);
        SETFLOAT(at+1, (float)x->x_y);
        outlet_anything(x->x_obj.ob_outlet, &s_list, 2, at);
    }
    return(1);
}

static void pad_config_size(t_pad *x){
    if(pad_vis_check(x)){
        int z = x->x_zoom;
        int x1 = text_xpix(&x->x_obj, x->x_glist);
        int y1 = text_ypix(&x->x_obj, x->x_glist);
        int x2 = x1 + x->x_w * z;
        int y2 = y1 + x->x_h * z;
        t_canvas *cv = glist_getcanvas(x->x_glist);
        pdgui_vmess(0, "crs iiii", cv, "coords", x->x_base_tag,
            x1, y1, x2, y2);
        pdgui_vmess(0, "crs iiii", cv, "coords", x->x_outlet_tag,
            x1, y2 - OHEIGHT*z, x1 + IOWIDTH*z, y2);
        canvas_fixlinesfor(glist_getcanvas(x->x_glist), (t_text*)x);
    }
}
                
static void pad_dim(t_pad *x, t_floatarg f1, t_floatarg f2){
    int w = f1 < 12 ? 12 : (int)f1, h = f2 < 12 ? 12 : (int)f2;
    if(w != x->x_w || h != x->x_h){
        x->x_w = w; x->x_h = h;
        pad_config_size(x);
    }
}

static void pad_width(t_pad *x, t_floatarg f){
    int w = f < 12 ? 12 : (int)f;
    if(w != x->x_w){
        x->x_w = w;
        pad_config_size(x);
    }
}

static void pad_height(t_pad *x, t_floatarg f){
    int h = f < 12 ? 12 : (int)f;
    if(h != x->x_h){
        x->x_h = h;
        pad_config_size(x);
    }
}

static void pad_setbg(t_pad *x){
    if(pad_vis_check(x))
        pdgui_vmess(0, "crs rs", glist_getcanvas(x->x_glist), "itemconfigure",
            x->x_base_tag, "-fill", x->x_transparent ? "" : x->x_bg->s_name);
}

static void pad_color(t_pad *x, t_symbol *s, int ac, t_atom *av){
    t_symbol *color = s = pad_getcolor(x, ac, av);
    if(color == x->x_bg)
        return;
    x->x_bg = color;
    pad_setbg(x);
}

static void pad_transparent(t_pad *x, t_floatarg f){
    int transp = (int)(f != 0);
    if(transp == x->x_transparent)
        return;
    x->x_transparent = transp;
    pad_setbg(x);
}

static void pad_zoom(t_pad *x, t_floatarg zoom){
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
            pad_config_io_let(p->p_cnv);
        }
    }
}

static void pad_free(t_pad *x){
    pd_unbind(&x->x_obj.ob_pd, x->x_bindname);
    x->x_proxy->p_cnv = NULL;
    clock_delay(x->x_proxy->p_clock, 0);
    pdgui_stub_deleteforkey(x);
}

static void edit_proxy_free(t_edit_proxy *p){
    pd_unbind(&p->p_obj.ob_pd, p->p_sym);
    clock_free(p->p_clock);
    pd_free(&p->p_obj.ob_pd);
}

static t_edit_proxy * edit_proxy_new(t_pad *x, t_symbol *s){
    t_edit_proxy *p = (t_edit_proxy*)pd_new(edit_proxy_class);
    p->p_cnv = x;
    pd_bind(&p->p_obj.ob_pd, p->p_sym = s);
    p->p_clock = clock_new(p, (t_method)edit_proxy_free);
    return(p);
}

static void *pad_new(t_symbol *s, int ac, t_atom *av){
    t_pad *x = (t_pad *)pd_new(pad_class);
    t_canvas *cv = canvas_getcurrent();
    x->x_glist = (t_glist*)cv;
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING-1, ".x%lx", (unsigned long)cv);
    buf[MAXPDSTRING-1] = 0;
    x->x_proxy = edit_proxy_new(x, gensym(buf));
    sprintf(buf, "#%lx", (long)x);
    pd_bind(&x->x_obj.ob_pd, x->x_bindname = gensym(buf));
    x->x_edit = cv->gl_edit;
    x->x_zoom = x->x_glist->gl_zoom;
    x->x_x = x->x_y = 0;
    x->x_color[0] = x->x_color[1] = x->x_color[2] = 255;
    int w = 127, h = 127;
    if(ac && av->a_type == A_FLOAT){ // 1st Width
        w = av->a_w.w_float;
        ac--; av++;
        if(ac && av->a_type == A_FLOAT){ // 2nd Height
            h = av->a_w.w_float;
            ac--, av++;
            if(ac && av->a_type == A_FLOAT){ // Red
                x->x_color[0] = (unsigned char)av->a_w.w_float;
                ac--, av++;
                if(ac && av->a_type == A_FLOAT){ // Green
                    x->x_color[1] = (unsigned char)av->a_w.w_float;
                    ac--, av++;
                    if(ac && av->a_type == A_FLOAT){ // Blue
                        x->x_color[2] = (unsigned char)av->a_w.w_float;
                        ac--, av++;
                        if(ac && av->a_type == A_FLOAT){ // Transparency
                            x->x_transparent = (int)av->a_w.w_float;
                            ac--, av++;
                        }
                    }
                }
            }
        }
    }
    while(ac > 0){
        if(av->a_type == A_SYMBOL){
            s = atom_getsymbolarg(0, ac, av);
            if(s == gensym("-dim")){
                if(ac >= 3 && (av+1)->a_type == A_FLOAT && (av+2)->a_type == A_FLOAT){
                    w = atom_getfloatarg(1, ac, av);
                    h = atom_getfloatarg(2, ac, av);
                    ac-=3, av+=3;
                }
                else goto errstate;
            }
            else if(s == gensym("-color")){
                if(ac >= 4 && (av+1)->a_type == A_FLOAT
                   && (av+2)->a_type == A_FLOAT
                   && (av+3)->a_type == A_FLOAT){
                    int r = (int)atom_getfloatarg(1, ac, av);
                    int g = (int)atom_getfloatarg(2, ac, av);
                    int b = (int)atom_getfloatarg(3, ac, av);
                    x->x_color[0] = r < 0 ? 0 : r > 255 ? 255 : r;
                    x->x_color[1] = g < 0 ? 0 : g > 255 ? 255 : g;
                    x->x_color[2] = b < 0 ? 0 : b > 255 ? 255 : b;
                    ac-=4, av+=4;
                }
                else goto errstate;
            }
            else if(s == gensym("-transparent"))
                x->x_transparent = 1;
            else
                goto errstate;
        }
        else
            goto errstate;
    }
    x->x_w = w, x->x_h = h;
    
    t_atom at[3];
    SETFLOAT(&at[0], x->x_color[0]);
    SETFLOAT(&at[1], x->x_color[1]);
    SETFLOAT(&at[2], x->x_color[2]);
    pad_color(x, NULL, 3, at);
    outlet_new(&x->x_obj, &s_anything);
    sprintf(x->x_base_tag, "%pPAD_BASE", x);
    sprintf(x->x_all_tag, "%pPAD_ALL", x);
    sprintf(x->x_IOlet_tag, "%pPAD_IO", x);
    sprintf(x->x_outlet_tag, "%pPAD_outlet", x);
    return(x);
    errstate:
        pd_error(x, "[pad]: improper args");
        return(NULL);
}

void pad_setup(void){
    pad_class = class_new(gensym("pad"), (t_newmethod)pad_new,
        (t_method)pad_free, sizeof(t_pad), 0, A_GIMME, 0);
    class_addmethod(pad_class, (t_method)pad_dim, gensym("dim"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(pad_class, (t_method)pad_width, gensym("width"), A_FLOAT, 0);
    class_addmethod(pad_class, (t_method)pad_height, gensym("height"), A_FLOAT, 0);
    class_addmethod(pad_class, (t_method)pad_color, gensym("color"), A_GIMME, 0);
    class_addmethod(pad_class, (t_method)pad_transparent, gensym("transparent"), A_FLOAT, 0);
    class_addmethod(pad_class, (t_method)pad_zoom, gensym("zoom"), A_CANT, 0);
    class_addmethod(pad_class, (t_method)pad_mouserelease, gensym("_mouserelease"), 0);
    edit_proxy_class = class_new(0, 0, 0, sizeof(t_edit_proxy), CLASS_NOINLET | CLASS_PD, 0);
    class_addanything(edit_proxy_class, edit_proxy_any);
    pad_widgetbehavior.w_getrectfn  = pad_getrect;
    pad_widgetbehavior.w_displacefn = pad_displace;
    pad_widgetbehavior.w_selectfn   = pad_select;
    pad_widgetbehavior.w_activatefn = NULL;
    pad_widgetbehavior.w_deletefn   = pad_delete;
    pad_widgetbehavior.w_visfn      = pad_vis;
    pad_widgetbehavior.w_clickfn    = pad_click;
    class_setsavefn(pad_class, pad_save);
    class_setwidget(pad_class, &pad_widgetbehavior);
}
