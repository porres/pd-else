// stolen from (and based on) cyclone/scope~

#include <stdlib.h>
#include <string.h>
#include <m_pd.h>
#include <g_canvas.h>
#include <magic.h>

#define SCOPE_MINSIZE       20
#define SCOPE_MINPERIOD     2
#define SCOPE_MAXPERIOD     8192
#define SCOPE_MINBUFSIZE    8
#define SCOPE_MAXBUFSIZE    256
#define SCOPE_MINDELAY      0
#define SCOPE_MINLINEWIDTH  1
#define SCOPE_MAXLINEWIDTH  4
#define SCOPE_SELBDWIDTH    2
#define HANDLE_SIZE         12
#define SCOPE_GUICHUNK      128 // performance-related hacks, LATER investigate

typedef struct _edit_proxy{
    t_object        p_obj;
    t_symbol       *p_sym;
    t_clock        *p_clock;
    struct _scope  *p_cnv;
}t_edit_proxy;

typedef struct _scope{
    t_object        x_obj;
    t_inlet        *x_rightinlet;
    t_glist        *x_glist;
    t_canvas       *x_cv;
    t_edit_proxy   *x_proxy;
    t_symbol       *x_bg, *x_fg, *x_gg;
    float           x_xbuffer[SCOPE_MAXBUFSIZE*4];
    float           x_ybuffer[SCOPE_MAXBUFSIZE*4];
    float           x_xbuflast[SCOPE_MAXBUFSIZE*4];
    float           x_ybuflast[SCOPE_MAXBUFSIZE*4];
    float           x_min, x_max;
    float           x_trigx, x_triglevel;
    float           x_ksr;
    float           x_currx, x_curry;
    int             x_select;
    int             x_width, x_height;
    int             x_delay;
    int             x_trigmode;
    int             x_bufsize, x_lastbufsize;
    int             x_period;
    int             x_bufphase, x_precount, x_phase;
    int             x_xymode, x_frozen, x_retrigger;
    int             x_edit;
    int             x_theme;
    int             x_transparent;
    int             x_linewidth;
    t_float        *x_signalscalar;
    int             x_rcv_set;
    int             x_flag;
    int             x_r_flag;
    t_symbol       *x_receive;
    t_symbol       *x_rcv_raw;
    t_symbol       *x_bindsym;
    t_clock        *x_clock;
    t_pd           *x_handle;
    char            x_tag_all[32], x_tag_bg[32], x_tag_fg[32], x_tag_gr[32];
    char            x_tag_inlets[32], x_tag_in1[32], x_tag_in2[32];
}t_scope;

typedef struct _handle{
    t_pd            h_pd;
    t_scope        *h_master;
    t_symbol       *h_bindsym;
    char            h_pathname[64], h_outlinetag[64];
    int             h_dragon;
}t_handle;

static t_class *scope_class, *handle_class, *edit_proxy_class;
static t_widgetbehavior scope_widgetbehavior;

static void scope_getrect(t_gobj *z, t_glist *gl, int *xp1, int *yp1, int *xp2, int *yp2);

// ----------------- COLOR HELPERS --------------------------------------------------------
static t_symbol *scope_color_from_int(int c){
    char buf[16];
    snprintf(buf, sizeof(buf), "#%06X", c & 0xFFFFFF);
    return(gensym(buf));
}

static int scope_color_to_int(t_symbol *s){
    if(!s || !s->s_name || s->s_name[0] != '#')
        return(0);
    return((int)strtol(s->s_name+1, 0, 16));
}

static t_symbol *scope_getcolor(int ac, t_atom *av){
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

static t_symbol *scope_get_bg(t_scope *x){
    if(x->x_theme){
        char buf[16];
        snprintf(buf, sizeof(buf), "#%06X", THISGUI->i_backgroundcolor);
        return(gensym(buf));
    }
    return(x->x_bg);
}

static t_symbol *scope_get_fg(t_scope *x){
    if(x->x_theme){
        char buf[16];
        snprintf(buf, sizeof(buf), "#%06X", THISGUI->i_gopcolor);
        return(gensym(buf));
    }
    return(x->x_fg);
}

static t_symbol *scope_get_gg(t_scope *x){
    if(x->x_theme){
        char buf[16];
        snprintf(buf, sizeof(buf), "#%06X", THISGUI->i_foregroundcolor);
        return(gensym(buf));
    }
    return(x->x_gg);
}

// ----------------- DRAW ----------------------------------------------------------------
static void scope_draw_handle(t_scope *x, int state){
    t_handle *sh = (t_handle *)x->x_handle;
    t_canvas *cv = x->x_cv;
    // always destroy if pathname is set
    if(sh->h_pathname[0])
        pdgui_vmess(0, "rs", "destroy", sh->h_pathname);
    if(state){
        pdgui_vmess(0, "rs riri rkrkrk ri rr",
            "canvas", sh->h_pathname,
            "-width", HANDLE_SIZE, "-height", HANDLE_SIZE,
            "-bg", THISGUI->i_gopcolor,
            "-highlightcolor", THISGUI->i_foregroundcolor,
            "-highlightbackground", THISGUI->i_foregroundcolor,
            "-highlightthickness", 1,
            "-cursor", "bottom_right_corner");
        int x1, y1, x2, y2;
        scope_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        pdgui_vmess(0, "crr ii rs ri ri rs rs",
            cv, "create", "window",
            x2 - HANDLE_SIZE + 1,
            y2 - HANDLE_SIZE + 1,
            "-anchor", "nw",
            "-width", HANDLE_SIZE,
            "-height", HANDLE_SIZE,
            "-window", sh->h_pathname,
            "-tags", x->x_tag_all);
        char buf[512];
        snprintf(buf, sizeof(buf),
            "bind %s <Button> {pdsend [concat %s _click 1 [.x%lx.c canvasx %%x] [.x%lx.c canvasy %%y] \\;]}",
            sh->h_pathname, sh->h_bindsym->s_name, (unsigned long)x->x_cv, (unsigned long)x->x_cv);
        pdgui_vmess(buf, NULL);
        snprintf(buf, sizeof(buf),
            "bind %s <ButtonRelease> {pdsend [concat %s _click 0 [.x%lx.c canvasx %%x] [.x%lx.c canvasy %%y] \\;]}",
            sh->h_pathname, sh->h_bindsym->s_name, (unsigned long)x->x_cv, (unsigned long)x->x_cv);
        pdgui_vmess(buf, NULL);
        // convert motion coordinates to canvas system, so that the zoom is taken into account
        snprintf(buf, sizeof(buf),
            "bind %s <Motion> {pdsend [concat %s _motion [.x%lx.c canvasx %%x] [.x%lx.c canvasy %%y] \\;]}",
            sh->h_pathname, sh->h_bindsym->s_name, (unsigned long)x->x_cv, (unsigned long)x->x_cv);
        pdgui_vmess(buf, NULL);
    }
}

static void scope_drawfg(t_scope *x, t_canvas *cv, int x1, int y1, int x2, int y2){
    float dx, dy, xx = 0, yy = 0, oldx, oldy, sc, xsc, ysc;
    float *xbp = x->x_xbuflast, *ybp = x->x_ybuflast;
    int bufsize = x->x_lastbufsize;
    if(x->x_xymode == 1){
        dx = (float)(x2 - x1) / (float)bufsize;
        oldx = x1;
        sc = ((float)x->x_height - 2.) / (float)(x->x_max - x->x_min);
    }
    else if(x->x_xymode == 2){
        dy = (float)(y2 - y1) / (float)bufsize;
        oldy = y1;
        sc = ((float)x->x_width - 2.) / (float)(x->x_max - x->x_min);
    }
    else if(x->x_xymode == 3){
        xsc = ((float)x->x_width - 2.) / (float)(x->x_max - x->x_min);
        ysc = ((float)x->x_height - 2.) / (float)(x->x_max - x->x_min);
    }
    char chunk[32 * SCOPE_MAXBUFSIZE], *chunkp = chunk;
    chunk[0] = 0;
    for(int i = 0; i < bufsize; i++){
        if(x->x_xymode == 1){
            xx = oldx;
            yy = (y2 - 1) - sc * (*xbp++ - x->x_min);
            if(yy > y2)
                yy = y2;
            else if(yy < y1)
                yy = y1;
            oldx += dx;
        }
        else if(x->x_xymode == 2){
            yy = oldy;
            xx = (x2 - 1) - sc * (*ybp++ - x->x_min);
            if(xx > x2)
                xx = x2;
            else if(xx < x1)
                xx = x1;
            oldy += dy;
        }
        else if(x->x_xymode == 3){
            xx = x1 + xsc * (*xbp++ - x->x_min);
            yy = y2 - ysc * (*ybp++ - x->x_min);
            if(xx > x2)
                xx = x2;
            else if(xx < x1)
                xx = x1;
            if(yy > y2)
                yy = y2;
            else if(yy < y1)
                yy = y1;
        }
        sprintf(chunkp, "%d %d ", (int)xx, (int)yy);
        chunkp += strlen(chunkp);
    }
    char *tags[] = {x->x_tag_fg, x->x_tag_all};
    pdgui_vmess(0, "crr r rs ri rS", cv, "create", "line", chunk,
        "-fill", scope_get_fg(x)->s_name, "-width", x->x_linewidth, "-tags", 2, tags);
}

static void scope_draw_grid(t_scope *x, t_canvas *cv, int x1, int y1, int x2, int y2){
    float dx = (x2-x1)*0.125, dy = (y2-y1)*0.25, xx, yy;
    char *tags[] = {x->x_tag_gr, x->x_tag_all};
    int i;
    for(i = 0, xx = x1 + dx; i < 7; i++, xx += dx)
        pdgui_vmess(0, "crr fifi ri rS rs",
            cv, "create", "line", xx, y1, xx, y2,
            "-width", 1, "-tags", 2, tags,
            "-fill", scope_get_gg(x)->s_name);
    for(i = 0, yy = y1 + dy; i < 3; i++, yy += dy)
        pdgui_vmess(0, "crr ifif ri rS rs",
            cv, "create", "line", x1, yy, x2, yy,
            "-width", 1, "-tags", 2, tags,
            "-fill", scope_get_gg(x)->s_name);
}

static void scope_draw_bg(t_scope *x, t_canvas *cv, int x1, int y1, int x2, int y2){
    char *tags[] = {x->x_tag_bg, x->x_tag_all};
    if(x->x_transparent){
        pdgui_vmess(0, "crr iiii rk rs ri rS",
            cv, "create", "rectangle", x1, y1, x2, y2,
            "-outline", THISGUI->i_foregroundcolor,
            "-fill", "",
            "-width", 1,
            "-tags", 2, tags);
    }
    else{
        pdgui_vmess(0, "crr iiii rk rs ri rS",
            cv, "create", "rectangle", x1, y1, x2, y2,
            "-outline", THISGUI->i_foregroundcolor,
            "-fill", scope_get_bg(x)->s_name,
            "-width", 1,
            "-tags", 2, tags);
    }
}

static void scope_draw_inlets(t_scope *x){
    if(x->x_edit && x->x_receive == &s_){
        t_canvas *cv = glist_getcanvas(x->x_glist);
        int xpos = text_xpix(&x->x_obj, x->x_glist), ypos = text_ypix(&x->x_obj, x->x_glist);
        char *tags1[] = {x->x_tag_in1, x->x_tag_inlets, x->x_tag_all};
        char *tags2[] = {x->x_tag_in2, x->x_tag_inlets, x->x_tag_all};
        pdgui_vmess(0, "crr iiii rk rS",
            cv, "create", "rectangle", xpos, ypos, xpos+IOWIDTH, ypos+IHEIGHT,
            "-fill", THISGUI->i_foregroundcolor,
            "-tags", 3, tags1);
        pdgui_vmess(0, "crr iiii rk rS",
            cv, "create", "rectangle", xpos+x->x_width, ypos, xpos+x->x_width-IOWIDTH, ypos+IHEIGHT,
            "-fill", THISGUI->i_foregroundcolor,
            "-tags", 3, tags2);
    }
}

static void scope_draw(t_scope *x, t_canvas *cv){
    int x1, y1, x2, y2;
    scope_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
    scope_draw_bg(x, cv, x1, y1, x2, y2);
    scope_draw_grid(x, cv, x1, y1, x2, y2);
    if(x->x_xymode)
        scope_drawfg(x, cv, x1, y1, x2, y2);
    scope_draw_inlets(x);
}

static void scope_redraw(t_scope *x, t_canvas *cv){
    char chunk[32 * SCOPE_MAXBUFSIZE];  // LATER estimate
    char *chunkp = chunk;
    int bufsize, nleft = bufsize = x->x_lastbufsize, x1, y1, x2, y2, xymode = x->x_xymode;
    scope_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
    float dx, dy, xx, yy, oldx, oldy, sc, xsc, ysc;
    float *xbp = x->x_xbuflast, *ybp = x->x_ybuflast;
    if(xymode == 1){
        dx = (float)(x2 - x1) / (float)bufsize;
        oldx = x1;
        sc = ((float)x->x_height - 2.) / (float)(x->x_max - x->x_min);
    }
    else if(xymode == 2){
        dy = (float)(y2 - y1) / (float)bufsize;
        oldy = y1;
        sc = ((float)x->x_width - 2.) / (float)(x->x_max - x->x_min);
    }
    else if(xymode == 3){
        xsc = ((float)x->x_width - 2.) / (float)(x->x_max - x->x_min);
        ysc = ((float)x->x_height - 2.) / (float)(x->x_max - x->x_min);
    }
    chunk[0] = 0; // all points are collected and sent in a single message
    while(nleft > SCOPE_GUICHUNK){
        int i = SCOPE_GUICHUNK;
        while(i--){
            if(xymode == 1){
                xx = oldx;
                yy = (y2 - 1) - sc * (*xbp++ - x->x_min);
                if(yy > y2)
                    yy = y2;
                else if(yy < y1)
                    yy = y1;
                oldx += dx;
            }
            else if(xymode == 2){
                yy = oldy;
                xx = (x2 - 1) - sc * (*ybp++ - x->x_min);
                if(xx > x2)
                    xx = x2;
                else if(xx < x1)
                    xx = x1;
                oldy += dy;
            }
            else if(xymode == 3){
                xx = x1 + xsc * (*xbp++ - x->x_min);
                yy = y2 - ysc * (*ybp++ - x->x_min);
                if(xx > x2)
                    xx = x2;
                else if(xx < x1)
                    xx = x1;
                if(yy > y2)
                    yy = y2;
                else if(yy < y1)
                    yy = y1;
            }
            sprintf(chunkp, "%d %d ", (int)xx, (int)yy);
            chunkp += strlen(chunkp);
        }
        nleft -= SCOPE_GUICHUNK;
    }
    while(nleft--){
        if(xymode == 1){
            xx = oldx;
            yy = (y2 - 1) - sc * (*xbp++ - x->x_min);
            if(yy > y2)
                yy = y2;
            else if(yy < y1)
                yy = y1;
            oldx += dx;
        }
        else if(xymode == 2){
            yy = oldy;
            xx = (x2 - 1) - sc * (*ybp++ - x->x_min);
            if(xx > x2)
                xx = x2;
            else if(xx < x1)
                xx = x1;
            oldy += dy;
        }
        else if(xymode == 3){
            xx = x1 + xsc * (*xbp++ - x->x_min);
            yy = y2 - ysc * (*ybp++ - x->x_min);
            if(xx > x2)
                xx = x2;
            else if(xx < x1)
                xx = x1;
            if(yy > y2)
                yy = y2;
            else if(yy < y1)
                yy = y1;
        }
        sprintf(chunkp, "%d %d ", (int)xx, (int)yy);
        chunkp += strlen(chunkp);
    }
    pdgui_vmess(0, "crs r", cv, "coords", x->x_tag_fg, chunk);
}

//------------------ WIDGET -----------------------------------------------------------------
static void scope_getrect(t_gobj *z, t_glist *gl, int *xp1, int *yp1, int *xp2, int *yp2){
    t_scope *x = (t_scope *)z;
    float x1 = text_xpix((t_text *)x, gl), y1 = text_ypix((t_text *)x, gl);
    *xp1 = x1;
    *yp1 = y1;
    *xp2 = x1 + x->x_width;
    *yp2 = y1 + x->x_height;
}

static void scope_displace(t_gobj *z, t_glist *gl, int dx, int dy){
    t_scope *x = (t_scope *)z;
    x->x_obj.te_xpix += dx, x->x_obj.te_ypix += dy;
    t_canvas *cv = glist_getcanvas(gl);
    pdgui_vmess(0, "crs ii", cv, "move", x->x_tag_all, dx, dy);
    canvas_fixlinesfor(gl, (t_text*)x);
}

static void scope_select(t_gobj *z, t_glist *glist, int state){
    t_scope *x = (t_scope *)z;
    t_handle *sh = (t_handle *)x->x_handle;
    t_canvas *cv = glist_getcanvas(glist);
    x->x_select = state;
    int col = state ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor;
    if(x->x_transparent)
        pdgui_vmess(0, "crs rk ri rs", cv, "itemconfigure", x->x_tag_bg,
            "-outline", col,
            "-width", state ? SCOPE_SELBDWIDTH : 1,
            "-fill", "");
    else
        pdgui_vmess(0, "crs rk ri rs", cv, "itemconfigure", x->x_tag_bg,
            "-outline", col,
            "-width", state ? SCOPE_SELBDWIDTH : 1,
            "-fill", scope_get_bg(x)->s_name);
    if(x->x_edit && x->x_receive == &s_)
        pdgui_vmess(0, "crs rk rk", cv, "itemconfigure", x->x_tag_inlets,
            "-fill", col, "-outline", col);
    if(x->x_edit && sh->h_pathname[0]){
        if(state){
            pdgui_vmess(0, "rr rk",  sh->h_pathname, "configure",
                "-highlightbackground", THISGUI->i_selectcolor);
            pdgui_vmess(0, "rr rk",  sh->h_pathname, "configure",
                "-bg", THISGUI->i_selectcolor);
        }
        else{
            pdgui_vmess(0, "rr rk",  sh->h_pathname, "configure",
                "-highlightbackground", THISGUI->i_foregroundcolor);
            pdgui_vmess(0, "rr rk",  sh->h_pathname, "configure",
                "-bg", THISGUI->i_gopcolor);
        }
    }
}

static void scope_delete(t_gobj *z, t_glist *glist){
    canvas_deletelinesfor(glist, (t_text *)z);
}

static void scope_mouserelease(t_scope* x){
    if(!x->x_edit)
        x->x_frozen = 0;
}

static void scope_draw_all(t_scope* x){
    x->x_cv = glist_getcanvas(x->x_glist);
    t_handle *sh = (t_handle *)x->x_handle;
    sprintf(sh->h_pathname, ".x%lx.h%lx", (unsigned long)x->x_cv, (unsigned long)sh);
    char buf[256];
    snprintf(buf, sizeof(buf),
        ".x%lx.c bind %s <ButtonRelease> {pdsend [concat %s _mouserelease \\;]}",
        (unsigned long)x->x_cv, x->x_tag_all, x->x_bindsym->s_name);
    pdgui_vmess(buf, NULL);
    int bufsize = x->x_bufsize;
    x->x_bufsize = x->x_lastbufsize;
    scope_draw(x, x->x_cv);
    x->x_bufsize = bufsize;
}

static void scope_vis(t_gobj *z, t_glist *glist, int vis){
    t_scope *x = (t_scope *)z;
    x->x_cv = glist_getcanvas(glist);
    t_handle *sh = (t_handle *)x->x_handle;
    if(x->x_edit && sh->h_pathname[0])// bug hack, destroying even though 'draw_handle' also destroys it
    // maybe it just should be with the 'else' "delete all" message
    // we can just destroy this even if it doesn;t exist anyway,
    // this was needed to avooid some tcl erros
        pdgui_vmess(0, "rs", "destroy", sh->h_pathname);
    if(vis){
        sprintf(sh->h_pathname, ".x%lx.h%lx", (unsigned long)x->x_cv, (unsigned long)sh);
        char buf[256];
        snprintf(buf, sizeof(buf),
            ".x%lx.c bind %s <ButtonRelease> {pdsend [concat %s _mouserelease \\;]}",
            (unsigned long)x->x_cv, x->x_tag_all, x->x_bindsym->s_name);
        pdgui_vmess(buf, NULL);
        int bufsize = x->x_bufsize;
        x->x_bufsize = x->x_lastbufsize;
        scope_draw(x, x->x_cv);
        x->x_bufsize = bufsize;
        scope_draw_handle(x, x->x_edit);
    }
    else
        pdgui_vmess(0, "crs", x->x_cv, "delete", x->x_tag_all);
}

static int scope_click(t_gobj *z, t_glist *glist, int xpix, int ypix, int shift, int alt, int dbl, int doit){
    t_scope *x = (t_scope *)z;
    glist = NULL, shift = xpix = ypix = alt = dbl = 0;
    if(doit)
        x->x_frozen = 1;
    return(1);
}

//------------------------------ METHODS ------------------------------------------------------------------
static void scope_bufsize(t_scope *x, t_floatarg f){
    int size = f < SCOPE_MINBUFSIZE ? SCOPE_MINBUFSIZE : f > SCOPE_MAXBUFSIZE ? SCOPE_MAXBUFSIZE : (int)f;
    if(x->x_bufsize != size){
        x->x_bufsize = size;
        pd_float((t_pd *)x->x_rightinlet, x->x_bufsize);
        x->x_phase = x->x_bufphase = x->x_precount = 0;
    }
}

static void scope_period(t_scope *x, t_floatarg f){
    int period = f < 2 ? 2 : f > 8192 ? 8192 : (int)f;
    if(x->x_period != period){
        x->x_period = period;
        x->x_phase = x->x_bufphase = x->x_precount = 0;
    }
}

static void scope_list(t_scope *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(!ac)
        return;
    int period = 0;
    int f1 = (int)atom_getfloatarg(0, ac, av);
    period = f1 < 2 ? 2 : f1 > 8192 ? 8192 : f1;
    if(x->x_period != period){
        x->x_period = period;
        x->x_phase = x->x_bufphase = x->x_precount = 0;
    }
    if(ac > 1){
        int f2 = (int)atom_getfloatarg(1, ac, av);
        int size = f2 < SCOPE_MINBUFSIZE ? SCOPE_MINBUFSIZE :
            f2 > SCOPE_MAXBUFSIZE ? SCOPE_MAXBUFSIZE : f2;
        if(x->x_bufsize != size){
            x->x_bufsize = size;
            pd_float((t_pd *)x->x_rightinlet, x->x_bufsize);
            x->x_phase = x->x_bufphase = x->x_precount = 0;
        }
    }
}

static void scope_range(t_scope *x, t_floatarg f1, t_floatarg f2){
    float min = f1, max = f2;
    if(min == max)
        return;
    if(min > max){
        max = f1;
        min = f2;
    }
    if(x->x_min != min || x->x_max != max)
        x->x_min = min, x->x_max = max;
}

static void scope_delay(t_scope *x, t_floatarg f){
    int delay = f < 0 ? 0 : (int)f;
    if(x->x_delay != delay)
        x->x_delay = delay;
}

static void scope_trigger(t_scope *x, t_floatarg f){
    int trig = f < 0 ? 0 : f > 2 ? 2 : (int)f;
    if(x->x_trigmode != trig){
        x->x_trigmode = trig;
        if(x->x_trigmode == 0) // no trigger
            x->x_retrigger = 0;
    }
}

static void scope_triglevel(t_scope *x, t_floatarg f){
    if(x->x_triglevel != f)
        x->x_triglevel = f;
}

static void scope_linewidth(t_scope *x, t_floatarg f){
    int lw = (int)f;
    if(lw < SCOPE_MINLINEWIDTH)
        lw = SCOPE_MINLINEWIDTH;
    if(lw > SCOPE_MAXLINEWIDTH)
        lw = SCOPE_MAXLINEWIDTH;
    if(x->x_linewidth != lw){
        x->x_linewidth = lw;
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
            pdgui_vmess(0, "crs ri", glist_getcanvas(x->x_glist), "itemconfigure",
                x->x_tag_fg, "-width", x->x_linewidth);
    }
}

static void scope_validate(t_scope *x, t_floatarg v, t_symbol *color, t_floatarg t){
    int valid = (int)v;
    int type = (int)t;
    switch(type){
        case 0:
            if(valid){
                x->x_bg = color;
                if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
                    pdgui_vmess(0, "crs rs", glist_getcanvas(x->x_glist), "itemconfigure",
                        x->x_tag_bg, "-fill", scope_get_bg(x)->s_name);
            }
            else
                pd_error(0, "[scope~] invalid background color '%s'", color->s_name);
            break;
        case 1:
            if(valid){
                x->x_gg = color;
                if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
                    pdgui_vmess(0, "crs rs", glist_getcanvas(x->x_glist), "itemconfigure",
                        x->x_tag_gr, "-fill", scope_get_gg(x)->s_name);
            }
            else
                pd_error(0, "[scope~] invalid grid color '%s'", color->s_name);
            break;
        case 2:
            if(valid){
                x->x_fg = color;
                if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
                    pdgui_vmess(0, "crs rs", glist_getcanvas(x->x_glist), "itemconfigure",
                        x->x_tag_fg, "-fill", scope_get_fg(x)->s_name);
            }
            else
                pd_error(0, "[scope~] invalid foreground color '%s'", color->s_name);
            break;
    }
}

static void scope_send_color_validate(t_scope *x, const char *color, int which){
    char buf[512];
    snprintf(buf, sizeof(buf), "set color_name {%s}", color);
    pdgui_vmess(buf, NULL);
    pdgui_vmess("set color_escaped [string map {{ } {\\ }} $color_name]", NULL);
    pdgui_vmess("set hex_color [color2hex $color_name]", NULL);
    snprintf(buf, sizeof(buf),
        "if {$hex_color eq \"\"} "
        "{pdsend \"%s _validate 0 $color_escaped %d\"} "
        "else {pdsend \"%s _validate 1 $hex_color %d\"}",
        x->x_bindsym->s_name, which,
        x->x_bindsym->s_name, which);
    pdgui_vmess(buf, NULL);
}

static void scope_bgcolor(t_scope *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(!ac)
        return;
    scope_send_color_validate(x, scope_getcolor(ac, av)->s_name, 0);
}

static void scope_setbgcolor(t_scope *x, t_symbol *s, int ac, t_atom *av){
    x->x_theme = 0;
    scope_bgcolor(x, s, ac, av);
}

static void scope_fgcolor(t_scope *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(!ac)
        return;
    scope_send_color_validate(x, scope_getcolor(ac, av)->s_name, 2);
}

static void scope_setfgcolor(t_scope *x, t_symbol *s, int ac, t_atom *av){
    x->x_theme = 0;
    scope_fgcolor(x, s, ac, av);
}

static void scope_gridcolor(t_scope *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(!ac)
        return;
    scope_send_color_validate(x, scope_getcolor(ac, av)->s_name, 1);
}

static void scope_setgridcolor(t_scope *x, t_symbol *s, int ac, t_atom *av){
    x->x_theme = 0;
    scope_gridcolor(x, s, ac, av);
}

static void scope_colors(t_scope *x, t_symbol *bg, t_symbol *mg, t_symbol *fg){
    x->x_theme = 0;
    t_atom at[1];
    SETSYMBOL(at, bg);
    scope_bgcolor(x, NULL, 1, at);
    SETSYMBOL(at, mg);
    scope_gridcolor(x, NULL, 1, at);
    SETSYMBOL(at, fg);
    scope_fgcolor(x, NULL, 1, at);
}

static void scope_theme(t_scope *x, t_floatarg f){
    int theme = (int)(f != 0);
    if(x->x_theme == theme)
        return;
    x->x_theme = theme;
    if(gobj_shouldvis((t_gobj *)x, x->x_glist) && glist_isvisible(x->x_glist)){
        pdgui_vmess(0, "crs", glist_getcanvas(x->x_glist), "delete", x->x_tag_all);
        scope_draw_all(x);
        scope_draw_handle(x, x->x_edit);
    }
}

static void scope_transparent(t_scope *x, t_floatarg f){
    int transparent = (int)(f != 0);
    if(x->x_transparent == transparent)
        return;
    x->x_transparent = transparent;
    if(gobj_shouldvis((t_gobj *)x, x->x_glist) && glist_isvisible(x->x_glist)){
        pdgui_vmess(0, "crs", glist_getcanvas(x->x_glist), "delete", x->x_tag_all);
        scope_draw_all(x);
        scope_draw_handle(x, x->x_edit);
    }
}

static void scope_dim(t_scope *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(ac != 2)
        return;
    int width = atom_getintarg(0, ac, av);
    int height = atom_getintarg(1, ac, av);
    if(width < SCOPE_MINSIZE)
        width = SCOPE_MINSIZE;
    if(height < SCOPE_MINSIZE)
        height = SCOPE_MINSIZE;
    if(x->x_width != width || x->x_height != height){
        x->x_width = width, x->x_height = height;
        pdgui_vmess(0, "crs", glist_getcanvas(x->x_glist), "delete", x->x_tag_all);
        if(gobj_shouldvis((t_gobj *)x, x->x_glist) && glist_isvisible(x->x_glist)){
            scope_draw_all(x);
            scope_draw_handle(x, x->x_edit);
        }
        canvas_fixlinesfor(x->x_glist, (t_text *)x);
    }
}

static void scope_receive(t_scope *x, t_symbol *s){
    if(s == gensym(""))
        s = gensym("empty");
    t_symbol *rcv = s == gensym("empty") ? &s_ : canvas_realizedollar(x->x_glist, s);
    if(rcv != x->x_receive){
        if(x->x_receive != &s_)
            pd_unbind(&x->x_obj.ob_pd, x->x_receive);
        x->x_rcv_set = 1;
        x->x_rcv_raw = s;
        x->x_receive = rcv;
        if(x->x_receive == &s_ && x->x_edit && glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
            scope_draw_inlets(x);
        else{
            pd_bind(&x->x_obj.ob_pd, x->x_receive);
            if(x->x_edit && glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
                pdgui_vmess(0, "crs", glist_getcanvas(x->x_glist), "delete", x->x_tag_inlets);
        }
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
            scope_draw_handle(p->p_cnv, edit);
            if(edit)
                scope_draw_inlets(p->p_cnv);
            else
                pdgui_vmess(0, "crs", glist_getcanvas(p->p_cnv->x_glist), "delete", p->p_cnv->x_tag_inlets);
        }
    }
}

// --------------------- handle ---------------------------------------------------
static int xclick, yclick, xmotion, ymotion;

static void handle__click_callback(t_handle *sh, t_floatarg f, t_floatarg xarg, t_floatarg yarg){
    int click = (int)f;
    t_scope *x = sh->h_master;
    if(sh->h_dragon && click == 0){
        pdgui_vmess(0, "crs", x->x_cv, "delete", sh->h_outlinetag);
        t_atom undo[2];
        SETFLOAT(undo+0, x->x_width);
        SETFLOAT(undo+1, x->x_height);
        t_atom redo[2];
        SETFLOAT(redo+0, x->x_width + xmotion - xclick);
        SETFLOAT(redo+1, x->x_height + ymotion - yclick);
        pd_undo_set_objectstate(x->x_glist, (t_pd*)x, gensym("dim"), 2, undo, 2, redo);
        scope_dim(x, NULL, 2, redo);
        scope_draw_handle(x, 1);
        scope_select((t_gobj *)x, x->x_glist, x->x_select);
        canvas_dirty(x->x_cv, 1);
    }
    else if(!sh->h_dragon && click){
        int x1, y1, x2, y2;
        scope_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        pdgui_vmess(0, "crr iiii rk ri rs",
            x->x_cv, "create", "rectangle", x1, y1, x2, y2,
            "-outline", THISGUI->i_selectcolor,
            "-width", SCOPE_SELBDWIDTH,
            "-tags", sh->h_outlinetag);
        xclick = xmotion = xarg;
        yclick = ymotion = yarg;
    }
    sh->h_dragon = click;
}

static void handle__motion_callback(t_handle *sh, t_floatarg xarg, t_floatarg yarg){
    if(sh->h_dragon){
        t_scope *x = sh->h_master;
        xmotion = xarg;
        ymotion = yarg;
        int dx = xmotion - xclick, dy = ymotion - yclick;
        int x1, y1, x2, y2;
        scope_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        int newx = x2 + dx, newy = y2 + dy;
        if(newx < x1 + SCOPE_MINSIZE)
            newx = x1 + SCOPE_MINSIZE;
        if(newy < y1 + SCOPE_MINSIZE)
            newy = y1 + SCOPE_MINSIZE;
        pdgui_vmess(0, "crs iiii", x->x_cv, "coords", sh->h_outlinetag, x1, y1, newx, newy);
    }
}

//------------------------------------------------------------
static t_int *scope_perform(t_int *w){
    t_scope *x = (t_scope *)(w[1]);
    int nblock = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    if(!x->x_xymode || x->x_frozen){ // do nothing
        return(w+6);
    }
    if(!gobj_shouldvis((t_gobj *)x, x->x_glist) || !glist_isvisible(x->x_glist)){
        return(w+6);
    }
    int bufphase = x->x_bufphase;
    int bufsize = x->x_bufsize;
    if(bufphase < bufsize){
        if(x->x_precount >= nblock)
            x->x_precount -= nblock;
        else{
            t_float *in;
            int phase = x->x_phase;
            int period = x->x_period;
            float freq = 1. / period;
            float *bp1;
            float *bp2;
            float currx = x->x_currx;
            float curry = x->x_curry;
            if(x->x_precount > 0){
                nblock -= x->x_precount;
                in1 += x->x_precount;
                in2 += x->x_precount;
                phase = 0;
                bufphase = 0;
                x->x_precount = 0;
            }
            if(x->x_trigmode && (x->x_xymode == 1 || x->x_xymode == 2)){
                if(x->x_xymode == 1)
                    in = in1;
                else
                    in = in2;
                while(x->x_retrigger){
                    float triglevel = x->x_triglevel;
                    if(x->x_trigmode == 1){ // Trigger Up
                        if(x->x_trigx < triglevel){
                            while(nblock--){
                                if(*in >= triglevel){
                                    x->x_retrigger = 0;
                                    phase = 0;
                                    bufphase = 0;
                                    break;
                                }
                                else in++;
                            }
                        }
                        else{
                            while(nblock--){
                                if(*in++ < triglevel){
                                    x->x_trigx = triglevel - 1.;
                                    break;
                                }
                            }
                        }
                    }
                    else{ // Trigger Down
                        if(x->x_trigx > triglevel){
                            while(nblock--){
                                if(*in <= triglevel){
                                    phase = 0;
                                    bufphase = 0;
                                    x->x_retrigger = 0;
                                    break;
                                }
                                else in++;
                            }
                        }
                        else{
                            while(nblock--){
                                if(*in++ > triglevel){
                                    x->x_trigx = triglevel + 1.;
                                    break;
                                }
                            }
                        }
                    }
                    if(nblock <= 0){
                        x->x_bufphase = bufphase;
                        x->x_phase = phase;
                        return(w+6);
                    }
                }
                if(x->x_xymode == 1)
                    in1 = in;
                else
                    in2 = in;
            }
            else if(x->x_retrigger)
                x->x_retrigger = 0;
            while(nblock--){
                bp1 = x->x_xbuffer + bufphase;
                bp2 = x->x_ybuffer + bufphase;
                if(phase){
                    t_float f1 = *in1++;
                    t_float f2 = *in2++;
                    if(x->x_xymode == 1){ // CHECKED
                        
                        if((currx<0 && (f1<currx || f1>-currx)) || (currx>0 && (f1>currx || f1<-currx)))
                                currx = f1;
        
                        curry = 0.;
                    }
                    else if(x->x_xymode == 2){
                        if((curry<0 && (f2<curry || f2>-curry)) || (curry>0 && (f2>curry || f2<-curry)))
                            curry = f2;

                        currx = 0.;
                    }
                    else{
                        currx += f1;
                        curry += f2;
                    }
                }
                else{
                    currx = *in1++;
                    curry = *in2++;
                }
                if(currx != currx)
                    currx = 0.;  // CHECKED NaNs bashed to zeros
                if(curry != curry)
                    curry = 0.;
                if(++phase >= period){
                    phase = 0;
                    if(x->x_xymode == 3){
                        currx *= freq;
                        curry *= freq;
                    }
                    if(++bufphase >= bufsize){
                        *bp1 = currx;
                        *bp2 = curry;
                        bufphase = 0;
                        x->x_lastbufsize = bufsize;
                        memcpy(x->x_xbuflast, x->x_xbuffer, bufsize * sizeof(*x->x_xbuffer));
                        memcpy(x->x_ybuflast, x->x_ybuffer, bufsize * sizeof(*x->x_ybuffer));
                        x->x_retrigger = (x->x_trigmode != 0);
                        x->x_trigx = x->x_triglevel;
                        clock_delay(x->x_clock, 0);
                    }
                    else{
                        *bp1 = currx;
                        *bp2 = curry;
                    }
                }
            }
            x->x_currx = currx;
            x->x_curry = curry;
            x->x_bufphase = bufphase;
            x->x_phase = phase;
        }
    }
    return(w+6);
}

static void scope_dsp(t_scope *x, t_signal **sp){
    x->x_ksr = sp[0]->s_sr * 0.001;
    int xfeeder = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    int yfeeder = else_magic_inlet_connection((t_object *)x, x->x_glist, 1, &s_signal);
    int xymode = xfeeder + 2 * yfeeder;
    if(xymode != x->x_xymode){
        x->x_xymode = xymode;
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist)){
            t_canvas *cv = glist_getcanvas(x->x_glist);
            int x1, y1, x2, y2;
            scope_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
            pdgui_vmess(0, "crs", cv, "delete", x->x_tag_fg);
            if(x->x_xymode)
                scope_drawfg(x, cv, x1, y1, x2, y2);
        }
        x->x_precount = 0;
    }
    dsp_add(scope_perform, 5, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void scope_tick(t_scope *x){
    if(glist_isvisible(x->x_glist)  && gobj_shouldvis((t_gobj *)x, x->x_glist) && x->x_xymode)
        scope_redraw(x, glist_getcanvas(x->x_glist));
    x->x_precount = (int)(x->x_delay * x->x_ksr);
}

static void scope_get_rcv(t_scope* x){
    if(!x->x_rcv_set){ // no receive set, search arguments
        t_binbuf *bb = x->x_obj.te_binbuf;
        int n_args = binbuf_getnatom(bb) - 1, i = 0; // number of arguments
        char buf[128];
        if(n_args > 0){ // we have arguments, let's search them
            if(x->x_flag){ // arguments are flags actually
                if(x->x_r_flag){ // we got a receive flag, let's get it
                    for(i = 0;  i <= n_args; i++){
                        atom_string(binbuf_getvec(bb) + i, buf, 128);
                        if(gensym(buf) == gensym("@receive")){
                            i++;
                            atom_string(binbuf_getvec(bb) + i, buf, 128);
                            x->x_rcv_raw = gensym(buf);
                            break;
                        }
                    }
                }
            }
            else{ // we got no flags, let's search for argument
                int arg_n = 22; // receive argument number
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

static void scope_save(t_gobj *z, t_binbuf *b){
    t_scope *x = (t_scope *)z;
    t_text *t = (t_text *)x;
    scope_get_rcv(x);
    binbuf_addv(b, "ssiisiiiiiffiiifsssisiii;", gensym("#X"), gensym("obj"), (int)t->te_xpix,
        (int)t->te_ypix, atom_getsymbol(binbuf_getvec(t->te_binbuf)), x->x_width,
        x->x_height, x->x_period, 3, x->x_bufsize, x->x_min, x->x_max, x->x_delay,
        0, x->x_trigmode, x->x_triglevel, x->x_fg, x->x_bg, x->x_gg, 0, x->x_rcv_raw,
        x->x_theme, x->x_transparent, x->x_linewidth);
}

static void scope_properties(t_gobj *z, t_glist *owner){
    (void)owner;
    t_scope *x = (t_scope *)z;
    scope_get_rcv(x);
    char pd_bg[32];
    snprintf(pd_bg, sizeof(pd_bg), "#%06X", THISGUI->i_backgroundcolor);
    char pd_grid[32];
    snprintf(pd_grid, sizeof(pd_grid), "#%06X", THISGUI->i_foregroundcolor);
    char pd_fg[32];
    snprintf(pd_fg, sizeof(pd_fg), "#%06X", THISGUI->i_gopcolor);
    pdgui_stub_vnew(&x->x_obj.ob_pd, "::dialog_scope::pdtk_scope_dialog", x,
        "ririr ririr rfrfr rir rir sr rirfr rii rii rii ri rii sss sss ii",
        "dim", x->x_width, "width:", x->x_height, "height:",
        "buf", x->x_period, "cal:", x->x_bufsize, "bfs:",
        "rng", x->x_min, "min:", x->x_max, "max:",
        "del", x->x_delay, "del:",
        "lw", x->x_linewidth, "lw:",
        x->x_rcv_raw->s_name, "rcv:",
        "trg", x->x_trigmode, "tmd:", x->x_triglevel, "tlv:",
        "dim_mins", SCOPE_MINSIZE, SCOPE_MINSIZE,
        "cal_min_max", SCOPE_MINPERIOD, SCOPE_MAXPERIOD,
        "bfs_min_max", SCOPE_MINBUFSIZE, SCOPE_MAXBUFSIZE,
        "del_mins", SCOPE_MINDELAY,
        "lw_min_max", SCOPE_MINLINEWIDTH, SCOPE_MAXLINEWIDTH,
        x->x_bg->s_name, x->x_gg->s_name, x->x_fg->s_name,
        pd_bg, pd_grid, pd_fg,
        x->x_theme, x->x_transparent);
}

static int scope_getcolorarg(int index, int ac, t_atom *av){
    if((av+index)->a_type == A_SYMBOL){
        t_symbol *s = atom_getsymbolarg(index, ac, av);
        if('#' == s->s_name[0])
            return(strtol(s->s_name+1, 0, 16));
    }
    return(atom_getintarg(index, ac, av));
}

static void scope_ok(t_scope *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_atom undo[16];
    SETFLOAT(undo+0, x->x_width);
    SETFLOAT(undo+1, x->x_height);
    SETFLOAT(undo+2, x->x_period);
    SETFLOAT(undo+3, x->x_bufsize);
    SETFLOAT(undo+4, x->x_min);
    SETFLOAT(undo+5, x->x_max);
    SETFLOAT(undo+6, x->x_delay);
    SETFLOAT(undo+7, x->x_linewidth);
    SETFLOAT(undo+8, x->x_trigmode);
    SETFLOAT(undo+9, x->x_triglevel);
    SETSYMBOL(undo+10, x->x_bg);
    SETSYMBOL(undo+11, x->x_gg);
    SETSYMBOL(undo+12, x->x_fg);
    SETSYMBOL(undo+13, x->x_receive);
    SETFLOAT(undo+14, x->x_theme);
    SETFLOAT(undo+15, x->x_transparent);
    pd_undo_set_objectstate(x->x_glist, (t_pd*)x, gensym("dialog"), 16, undo, ac, av);
    int width = atom_getintarg(0, ac, av);
    int height = atom_getintarg(1, ac, av);
    int period = atom_getintarg(2, ac, av);
    int bufsize = atom_getintarg(3, ac, av);
    float minval = (float)atom_getfloatarg(4, ac, av);
    float maxval = (float)atom_getfloatarg(5, ac, av);
    int delay = atom_getintarg(6, ac, av);
    int linewidth = atom_getintarg(7, ac, av);
    int trigmode = atom_getintarg(8, ac, av);
    int triglevel = atom_getintarg(9, ac, av);
    int bgcol = (int)scope_getcolorarg(10, ac, av);
    int grcol = (int)scope_getcolorarg(11, ac, av);
    int fgcol = (int)scope_getcolorarg(12, ac, av);
    t_symbol *rcv = atom_getsymbolarg(13, ac, av);
    int theme = atom_getintarg(14, ac, av);
    int transparent = atom_getintarg(15, ac, av);
    scope_period(x, period);
    scope_bufsize(x, bufsize);
    scope_range(x, minval, maxval);
    scope_delay(x, delay);
    scope_linewidth(x, linewidth);
    scope_receive(x, rcv);
    scope_trigger(x, trigmode);
    scope_triglevel(x, triglevel);
    t_atom c_at[1];
    SETSYMBOL(c_at, scope_color_from_int(bgcol));
    scope_bgcolor(x, NULL, 1, c_at);
    SETSYMBOL(c_at, scope_color_from_int(grcol));
    scope_gridcolor(x, NULL, 1, c_at);
    SETSYMBOL(c_at, scope_color_from_int(fgcol));
    scope_fgcolor(x, NULL, 1, c_at);
    scope_theme(x, theme);
    scope_transparent(x, transparent);
    t_atom dim[2];
    SETFLOAT(dim+0, width);
    SETFLOAT(dim+1, height);
    scope_dim(x, NULL, 2, dim);
    canvas_dirty(x->x_cv, 1);
}

static void edit_proxy_free(t_edit_proxy *p){
    pd_unbind(&p->p_obj.ob_pd, p->p_sym);
    clock_free(p->p_clock);
    pd_free(&p->p_obj.ob_pd);
}

static t_edit_proxy * edit_proxy_new(t_scope *x, t_symbol *s){
    t_edit_proxy *p = (t_edit_proxy*)pd_new(edit_proxy_class);
    p->p_cnv = x;
    pd_bind(&p->p_obj.ob_pd, p->p_sym = s);
    p->p_clock = clock_new(p, (t_method)edit_proxy_free);
    return(p);
}

static void scope_free(t_scope *x){
    if(x->x_clock)
        clock_free(x->x_clock);
    if(x->x_receive != &s_)
        pd_unbind(&x->x_obj.ob_pd, x->x_receive);
    pd_unbind(&x->x_obj.ob_pd, x->x_bindsym);
    if(x->x_handle){
        pd_unbind(x->x_handle, ((t_handle *)x->x_handle)->h_bindsym);
        pd_free(x->x_handle);
    }
    clock_delay(x->x_proxy->p_clock, 0);
    pdgui_stub_deleteforkey(x);
}

static void *scope_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_scope *x = (t_scope *)pd_new(scope_class);
    x->x_handle = pd_new(handle_class);
    t_handle *sh = (t_handle *)x->x_handle;
    sh->h_master = x;
    char hbuf[64];
    sprintf(hbuf, "_h%lx", (unsigned long)sh);
    pd_bind(x->x_handle, sh->h_bindsym = gensym(hbuf));
    sprintf(sh->h_outlinetag, "h%lx", (unsigned long)sh);
    x->x_glist = (t_glist*)canvas_getcurrent();
    x->x_cv = glist_getcanvas(x->x_glist);
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING-1, ".x%lx", (unsigned long)x->x_cv);
    buf[MAXPDSTRING-1] = 0;
    x->x_proxy = edit_proxy_new(x, gensym(buf));
    sprintf(buf, "#%lx", (long)x);
    pd_bind(&x->x_obj.ob_pd, x->x_bindsym = gensym(buf));
    sprintf(x->x_tag_all, "all%lx", (unsigned long)x);
    sprintf(x->x_tag_bg, "bg%lx", (unsigned long)x);
    sprintf(x->x_tag_fg, "fg%lx", (unsigned long)x);
    sprintf(x->x_tag_gr, "gr%lx", (unsigned long)x);
    sprintf(x->x_tag_inlets, "inlets%lx", (unsigned long)x);
    sprintf(x->x_tag_in1, "%lx_in1", (unsigned long)x);
    sprintf(x->x_tag_in2, "%lx_in2", (unsigned long)x);
    x->x_edit = x->x_cv ->gl_edit;
    t_symbol *rcv = x->x_receive = x->x_rcv_raw = &s_;
    x->x_bufsize = x->x_xymode = x->x_frozen = x->x_precount = sh->h_dragon = 0;
    x->x_flag = x->x_r_flag = x->x_rcv_set = x->x_select = 0;
    x->x_theme = x->x_transparent = 0;
    x->x_linewidth = 1;
    x->x_phase = x->x_bufphase = x->x_precount = 0;
    // def values
    float width = 200, height = 100, period = 8, bufsize = x->x_lastbufsize = 64;
    float minval = -1, maxval = 1, delay = 0, trigger = 0, triglevel = 0;
    t_symbol *bg_sym = gensym("#bebebe");
    t_symbol *fg_sym = gensym("#1e1e1e");
    t_symbol *gg_sym = gensym("#a0a0a0");
    if(ac){
        if(av->a_type == A_FLOAT){ // 1st Width
            width = av->a_w.w_float < SCOPE_MINSIZE ? SCOPE_MINSIZE : av->a_w.w_float;
            ac--; av++;
            if(ac && av->a_type == A_FLOAT){ // 2nd Height
                height = av->a_w.w_float < SCOPE_MINSIZE ? SCOPE_MINSIZE : av->a_w.w_float;
                ac--, av++;
                if(ac && av->a_type == A_FLOAT){ // 3rd period
                    period = av->a_w.w_float;
                    ac--; av++;
                    if(ac && av->a_type == A_FLOAT){ // 4th nothing really (argh/grrr)
                        ac--; av++;
                        if(ac && av->a_type == A_FLOAT){ // 5th bufsize
                            bufsize = av->a_w.w_float;
                            ac--; av++;
                            if(ac && av->a_type == A_FLOAT){ // 6th minval
                                minval = av->a_w.w_float;
                                ac--; av++;
                                if(ac && av->a_type == A_FLOAT){ // 7th maxval
                                    maxval = av->a_w.w_float;
                                    ac--; av++;
                                    if(ac && av->a_type == A_FLOAT){ // 8th delay
                                        delay = av->a_w.w_float;
                                        ac--; av++;
                                        if(ac && av->a_type == A_FLOAT){ // 9th ? nobody knows...
                                            ac--; av++;
                                            if(ac && av->a_type == A_FLOAT){ // 10th trigger
                                                trigger = av->a_w.w_float;
                                                ac--; av++;
                                                if(ac && av->a_type == A_FLOAT){ // 11th triglevel
                                                    triglevel = av->a_w.w_float;
                                                    ac--; av++;
                                                    // colors: either 3 symbols (new) or 9 floats (old)
                                                    if(ac >= 3 && av->a_type == A_SYMBOL){
                                                        // new format: fg, bg, gg
                                                        fg_sym = atom_getsymbol(av); ac--; av++;
                                                        if(ac && av->a_type == A_SYMBOL){
                                                            bg_sym = atom_getsymbol(av); ac--; av++;
                                                            if(ac && av->a_type == A_SYMBOL){
                                                                gg_sym = atom_getsymbol(av); ac--; av++;
                                                            }
                                                        }
                                                    }
                                                    else if(ac >= 9 && av->a_type == A_FLOAT){
                                                        // old format: fgRGB bgRGB ggRGB
                                                        unsigned char fr = (unsigned char)av->a_w.w_float;
                                                        ac--; av++;
                                                        unsigned char fg_ = (unsigned char)av->a_w.w_float;
                                                        ac--; av++;
                                                        unsigned char fb = (unsigned char)av->a_w.w_float;
                                                        ac--; av++;
                                                        unsigned char br = (unsigned char)av->a_w.w_float;
                                                        ac--; av++;
                                                        unsigned char bg_ = (unsigned char)av->a_w.w_float;
                                                        ac--; av++;
                                                        unsigned char bb = (unsigned char)av->a_w.w_float;
                                                        ac--; av++;
                                                        unsigned char gr = (unsigned char)av->a_w.w_float;
                                                        ac--; av++;
                                                        unsigned char gg_ = (unsigned char)av->a_w.w_float;
                                                        ac--; av++;
                                                        unsigned char gb = (unsigned char)av->a_w.w_float;
                                                        ac--; av++;
                                                        sprintf(buf, "#%.2x%.2x%.2x", fr, fg_, fb);
                                                        fg_sym = gensym(buf);
                                                        sprintf(buf, "#%.2x%.2x%.2x", br, bg_, bb);
                                                        bg_sym = gensym(buf);
                                                        sprintf(buf, "#%.2x%.2x%.2x", gr, gg_, gb);
                                                        gg_sym = gensym(buf);
                                                    }
                                                    // continue with "empty", receive, theme...
                                                    if(ac && av->a_type == A_FLOAT){ // 21st empty
                                                        ac--; av++;
                                                        if(ac && av->a_type == A_SYMBOL){ // 22nd receive
                                                            if(av->a_w.w_symbol == gensym("empty")) // ignore empty symbol
                                                                ac--, av++;
                                                            else{
                                                                rcv = av->a_w.w_symbol;
                                                                ac--, av++;
                                                            }
                                                            if(ac && av->a_type == A_FLOAT){ // 23rd theme
                                                                x->x_theme = (int)av->a_w.w_float;
                                                                ac--, av++;
                                                                if(ac && av->a_type == A_FLOAT){ // 24th transparent
                                                                    x->x_transparent = (int)av->a_w.w_float;
                                                                    ac--, av++;
                                                                    if(ac && av->a_type == A_FLOAT){ // 25th linewidth
                                                                        int lw = (int)av->a_w.w_float;
                                                                        x->x_linewidth = lw < SCOPE_MINLINEWIDTH ? SCOPE_MINLINEWIDTH : lw > SCOPE_MAXLINEWIDTH ? SCOPE_MAXLINEWIDTH : lw;
                                                                        ac--, av++;
                                                                    }
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
        else if(av->a_type == A_SYMBOL){
            t_symbol *sym = atom_getsymbolarg(0, ac, av);
            if(sym == gensym("-nsamples") && ac >= 2){
                x->x_flag = 1;
                period = atom_getfloatarg(1, ac, av);
                ac-=2, av+=2;
            }
            else if(sym == gensym("-nlines") && ac >= 2){
                x->x_flag = 1;
                bufsize = atom_getfloatarg(1, ac, av);
                ac-=2, av+=2;
            }
            else if(sym == gensym("-range") && ac >= 3){
                x->x_flag = 1;
                minval = atom_getfloatarg(1, ac, av);
                maxval = atom_getfloatarg(2, ac, av);
                ac-=3, av+=3;
            }
            else if(sym == gensym("-dim") && ac >= 3){
                x->x_flag = 1;
                height = atom_getfloatarg(1, ac, av);
                width = atom_getfloatarg(2, ac, av);
                ac-=3, av+=3;
            }
            else if(sym == gensym("-rate") && ac >= 2){
                x->x_flag = 1;
                delay = atom_getfloatarg(1, ac, av);
                ac-=2, av+=2;
            }
            else if(sym == gensym("-trigger") && ac >= 2){
                x->x_flag = 1;
                trigger = atom_getfloatarg(1, ac, av);
                ac-=2, av+=2;
            }
            else if(sym == gensym("-triglevel") && ac >= 2){
                x->x_flag = 1;
                triglevel = atom_getfloatarg(1, ac, av);
                ac-=2, av+=2;
            }
            else if(sym == gensym("-fgcolor") && ac >= 2){
                x->x_flag = 1;
                if(av[1].a_type == A_SYMBOL){
                    fg_sym = atom_getsymbol(av+1);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(sym == gensym("-bgcolor") && ac >= 2){
                x->x_flag = 1;
                if(av[1].a_type == A_SYMBOL){
                    bg_sym = atom_getsymbol(av+1);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(sym == gensym("-gridcolor") && ac >= 2){
                x->x_flag = 1;
                if(av[1].a_type == A_SYMBOL){
                    gg_sym = atom_getsymbol(av+1);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(sym == gensym("-receive") && ac >= 2){
                x->x_flag = x->x_r_flag = 1;
                rcv = atom_getsymbolarg(1, ac, av);
                ac-=2, av+=2;
            }
            else if(sym == gensym("-theme")){
                x->x_flag = 1;
                x->x_theme = 1;
                ac--, av++;
            }
            else if(sym == gensym("-transparent")){
                x->x_flag = 1;
                x->x_transparent = 1;
                ac--, av++;
            }
            else if(sym == gensym("-linewidth") && ac >= 2){
                x->x_flag = 1;
                int lw = (int)atom_getfloatarg(1, ac, av);
                x->x_linewidth = lw < SCOPE_MINLINEWIDTH ? SCOPE_MINLINEWIDTH : lw > SCOPE_MAXLINEWIDTH ? SCOPE_MAXLINEWIDTH : lw;
                ac-=2, av+=2;
            }
            else
                goto errstate;
        }
        else
            goto errstate;

    }
    x->x_bg = bg_sym;
    x->x_fg = fg_sym;
    x->x_gg = gg_sym;
    x->x_receive = canvas_realizedollar(x->x_glist, x->x_rcv_raw = rcv);
    if(x->x_receive != &s_)
        pd_bind(&x->x_obj.ob_pd, x->x_receive);
    else
        x->x_rcv_raw = gensym("empty");
    x->x_rightinlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    x->x_width = (int)width, x->x_height = (int)height;
    x->x_period = period < 2 ? 2 : period > 8192 ? 8192 : (int)period;
    x->x_bufsize = bufsize < SCOPE_MINBUFSIZE ? SCOPE_MINBUFSIZE : bufsize > SCOPE_MAXBUFSIZE ? SCOPE_MAXBUFSIZE : (int)bufsize;
    pd_float((t_pd *)x->x_rightinlet, x->x_bufsize);
    x->x_signalscalar = obj_findsignalscalar((t_object *)x, 1);
    if(minval > maxval)
        x->x_max = minval, x->x_min = maxval;
    else
        x->x_min = minval, x->x_max = maxval;
    x->x_delay = delay < 0 ? 0 : delay;
    x->x_triglevel = triglevel;
    x->x_trigmode = trigger < 0 ? 0 : trigger > 2 ? 2 : (int)trigger;
    if(x->x_trigmode == 0) // no trigger
        x->x_retrigger = 0;
    x->x_clock = clock_new(x, (t_method)scope_tick);
    return(x);
errstate:
    pd_error(x, "[scope~]: improper creation arguments");
    return(NULL);
}

void scope_tilde_setup(void){
    scope_class = class_new(gensym("scope~"), (t_newmethod)scope_new,
        (t_method)scope_free, sizeof(t_scope), 0, A_GIMME, 0);
    class_addmethod(scope_class, nullfn, gensym("signal"), 0);
    class_addmethod(scope_class, (t_method) scope_dsp, gensym("dsp"), A_CANT, 0);
    class_addfloat(scope_class, (t_method)scope_period);
    class_addlist(scope_class, (t_method)scope_list);
    class_addmethod(scope_class, (t_method)scope_period, gensym("nsamples"), A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_bufsize, gensym("nlines"), A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_dim, gensym("dim"), A_GIMME, 0);
    class_addmethod(scope_class, (t_method)scope_range, gensym("range"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_delay, gensym("rate"), A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_trigger, gensym("trigger"), A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_triglevel, gensym("triglevel"), A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_linewidth, gensym("linewidth"), A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_setfgcolor, gensym("fgcolor"), A_GIMME, 0);
    class_addmethod(scope_class, (t_method)scope_setbgcolor, gensym("bgcolor"), A_GIMME, 0);
    class_addmethod(scope_class, (t_method)scope_setgridcolor, gensym("gridcolor"), A_GIMME, 0);
    class_addmethod(scope_class, (t_method)scope_colors, gensym("colors"), A_SYMBOL, A_SYMBOL, A_SYMBOL, 0);
    class_addmethod(scope_class, (t_method)scope_theme, gensym("theme"), A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_transparent, gensym("transparent"), A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_receive, gensym("receive"), A_SYMBOL, 0);
    class_addmethod(scope_class, (t_method)scope_validate, gensym("_validate"),
        A_FLOAT, A_SYMBOL, A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_ok, gensym("dialog"), A_GIMME, 0);
    class_addmethod(scope_class, (t_method)scope_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(scope_class, (t_method)scope_mouserelease, gensym("_mouserelease"), 0);
    edit_proxy_class = class_new(0, 0, 0, sizeof(t_edit_proxy), CLASS_NOINLET | CLASS_PD, 0);
    class_addanything(edit_proxy_class, edit_proxy_any);
    handle_class = class_new(gensym("_handle"), 0, 0, sizeof(t_handle), CLASS_PD, 0);
    class_addmethod(handle_class, (t_method)handle__click_callback, gensym("_click"), A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(handle_class, (t_method)handle__motion_callback, gensym("_motion"),
        A_FLOAT, A_FLOAT, 0);
    class_setsavefn(scope_class, scope_save);
    class_setpropertiesfn(scope_class, scope_properties);
    class_setwidget(scope_class, &scope_widgetbehavior);
    scope_widgetbehavior.w_getrectfn  = scope_getrect;
    scope_widgetbehavior.w_displacefn = scope_displace;
    scope_widgetbehavior.w_selectfn   = scope_select;
    scope_widgetbehavior.w_deletefn   = scope_delete;
    scope_widgetbehavior.w_visfn      = scope_vis;
    scope_widgetbehavior.w_clickfn    = (t_clickfn)scope_click;
    #include "../Extra/else_tcl_utils.h"
    pdgui_vmess(else_tcl_utils, NULL);
    #include "../Extra/scope~_dialog.h" // properties in tcl/tk
    pdgui_vmess(scope_dialog_tcl, NULL);
}
