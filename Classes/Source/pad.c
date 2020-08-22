// porres 2020

#include "m_pd.h"
#include "g_canvas.h"

t_widgetbehavior pad_widgetbehavior;

static t_class *pad_class;

typedef struct _pad{
    t_object    x_obj;
    t_glist    *x_glist;
    t_symbol   *x_bindname;
    int         x_x;
    int         x_y;
    int         x_w;
    int         x_h;
    int         x_sel;
    int         x_zoom;
}t_pad;

t_widgetbehavior pad_widgetbehavior;
static t_class *pad_class;

static void pad_draw(t_pad *x, t_glist *glist){
    int xpos = text_xpix(&x->x_obj, glist), ypos = text_ypix(&x->x_obj, glist);
    sys_vgui(".x%lx.c create rectangle %d %d %d %d -width %d -outline %s -fill white -tags %lxBASE\n",
        glist_getcanvas(glist), xpos, ypos,
        xpos + x->x_w*x->x_zoom, ypos + x->x_h*x->x_zoom,
        x->x_zoom, x->x_sel ? "blue" : "black", x);
}

static void pad_erase(t_pad* x, t_glist* glist){
    sys_vgui(".x%lx.c delete %lxBASE\n", glist_getcanvas(glist), x);
}

static void pad_update(t_pad *x){
    int xpos = text_xpix(&x->x_obj, x->x_glist), ypos = text_ypix(&x->x_obj, x->x_glist);
    t_canvas *cv = glist_getcanvas(x->x_glist);
    sys_vgui(".x%lx.c coords %lxBASE %d %d %d %d\n", cv, x, xpos, ypos,
        xpos + x->x_w*x->x_zoom, ypos + x->x_h*x->x_zoom);
    canvas_fixlinesfor(glist_getcanvas(x->x_glist), (t_text*)x);
}

static void pad_vis(t_gobj *z, t_glist *glist, int vis){
    t_pad* x = (t_pad*)z;
    t_canvas *cv = glist_getcanvas(glist);
    if(vis){
        pad_draw(x, glist);
        sys_vgui(".x%lx.c bind %lxBASE <ButtonRelease> {pdsend [concat %s _mouserelease \\;]}\n", cv, x, x->x_bindname->s_name);
    }
    else
        pad_erase(x, glist);
}

static void pad_delete(t_gobj *z, t_glist *glist){
    canvas_deletelinesfor(glist, (t_text *)z);
}

static void pad_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_pad *x = (t_pad *)z;
    x->x_obj.te_xpix += dx, x->x_obj.te_ypix += dy;
    sys_vgui(".x%lx.c move %lxBASE %d %d\n", glist_getcanvas(glist), x, dx*x->x_zoom, dy*x->x_zoom);
    canvas_fixlinesfor(glist, (t_text*)x);
}

static void pad_select(t_gobj *z, t_glist *glist, int selected){
    t_pad *x = (t_pad *)z;
    t_canvas *cv = glist_getcanvas(glist);
    if((x->x_sel = selected))
        sys_vgui(".x%lx.c itemconfigure %lxBASE -outline blue\n", cv, x);
    else
        sys_vgui(".x%lx.c itemconfigure %lxBASE -outline black\n", cv, x);
}

// ------------------------ widgetbehaviour-----------------------------
static void pad_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2){
    t_pad *x = (t_pad *)z;
    *xp1 = text_xpix(&x->x_obj, glist);
    *yp1 = text_ypix(&x->x_obj, glist);
    *xp2 = *xp1 + x->x_w*x->x_zoom;
    *yp2 = *yp1 + x->x_h*x->x_zoom;
}

static void pad_save(t_gobj *z, t_binbuf *b){
  t_pad *x = (t_pad *)z;
  binbuf_addv(b, "ssiisii", gensym("#X"),gensym("obj"),
    (int)x->x_obj.te_xpix, (int)x->x_obj.te_ypix,
    atom_getsymbol(binbuf_getvec(x->x_obj.te_binbuf)),
    x->x_w, x->x_h);
  binbuf_addv(b, ";");
}

static void pad_motion(t_pad *x, t_floatarg dx, t_floatarg dy){
    x->x_x += (int)(dx/x->x_zoom);
    x->x_y -= (int)(dy/x->x_zoom);
    t_atom at[2];
    SETFLOAT(at, (t_float)x->x_x);
    SETFLOAT(at+1, (t_float)x->x_y);
    outlet_anything(x->x_obj.ob_outlet, &s_list, 2, at);
}
                   
static void pad_mouserelease(t_pad* x){
    t_atom at[3];
    SETFLOAT(at, 0);
    SETFLOAT(at+1, 0);
    SETFLOAT(at+2, 0);
    outlet_anything(x->x_obj.ob_outlet, gensym("click"), 3, at);
}

static int pad_click(t_gobj *z, struct _glist *glist, int xpix, int ypix,
int shift, int alt, int dbl, int doit){
    dbl = 0;
    t_pad* x = (t_pad *)z;
    t_atom at[3];
    int xpos = text_xpix(&x->x_obj, glist), ypos = text_ypix(&x->x_obj, glist);
    x->x_x = (xpix - xpos) / x->x_zoom;
    x->x_y = x->x_h - (ypix - ypos) / x->x_zoom;
    if(doit){
        SETFLOAT(at, (float)doit);
        SETFLOAT(at+1, (float)shift);
        SETFLOAT(at+2, (float)alt);
        outlet_anything(x->x_obj.ob_outlet, gensym("click"), 3, at);
        glist_grab(x->x_glist, &x->x_obj.te_g, (t_glistmotionfn)pad_motion, 0,
            (float)xpix, (float)ypix);
        SETFLOAT(at, (float)x->x_x);
        SETFLOAT(at+1, (float)x->x_y);
        outlet_anything(x->x_obj.ob_outlet, &s_list, 2, at);
    }
    else{
        SETFLOAT(at, (float)x->x_x);
        SETFLOAT(at+1, (float)x->x_y);
        outlet_anything(x->x_obj.ob_outlet, &s_list, 2, at);
    }
    return(1);
}

static void pad_dim(t_pad *x, t_floatarg f1, t_floatarg f2){
    int w = f1 < 12 ? 12 : (int)f1, h = f2 < 12 ? 12 : (int)f2;
    if(w != x->x_w || h != x->x_h){
        x->x_w = w; x->x_h = h;
        // dirty
        pad_update(x);
    }
}

static void pad_zoom(t_pad *x, t_floatarg zoom){
    x->x_zoom = (int)zoom;
    sys_vgui(".x%lx.c itemconfigure %lxBASE -width %d\n", glist_getcanvas(x->x_glist), x, x->x_zoom);
    pad_update(x);
}

static void *pad_new(t_symbol *s, int argc, t_atom *argv){
    s = NULL;
    t_pad *x = (t_pad *)pd_new(pad_class);
    x->x_glist = (t_glist *)canvas_getcurrent();
    char buf[MAXPDSTRING];
    sprintf(buf, "#%lx", (long)x);
    pd_bind(&x->x_obj.ob_pd, x->x_bindname = gensym(buf));
    x->x_zoom = x->x_glist->gl_zoom;
    t_int w = 127, h = 127;
    if(argc == 2){
        w = (int)atom_getintarg(0, argc, argv);
        h = (int)atom_getintarg(1, argc, argv);
    }
    x->x_w = w, x->x_h = h;
    outlet_new(&x->x_obj, &s_anything);
    x->x_x = x->x_y = 0;
    return(x);
}

static void pad_free(t_pad *x){
    pd_unbind(&x->x_obj.ob_pd, x->x_bindname);
    gfxstub_deleteforkey(x);
}

void pad_setup(void){
    pad_class = class_new(gensym("pad"), (t_newmethod)pad_new,
        (t_method)pad_free, sizeof(t_pad), 0, A_GIMME, 0);
    class_addmethod(pad_class, (t_method)pad_dim, gensym("dim"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(pad_class, (t_method)pad_zoom, gensym("zoom"), A_CANT, 0);
    class_addmethod(pad_class, (t_method)pad_mouserelease, gensym("_mouserelease"), 0);
    pad_widgetbehavior.w_getrectfn      = pad_getrect;
    pad_widgetbehavior.w_displacefn     = pad_displace;
    pad_widgetbehavior.w_selectfn       = pad_select;
    pad_widgetbehavior.w_activatefn     = NULL;
    pad_widgetbehavior.w_deletefn       = pad_delete;
    pad_widgetbehavior.w_visfn          = pad_vis;
    pad_widgetbehavior.w_clickfn        = pad_click;
    class_setsavefn(pad_class, pad_save);
    class_setwidget(pad_class, &pad_widgetbehavior);
}
