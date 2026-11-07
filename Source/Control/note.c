// Based on our work in comment from cyclone, which is now also based on this?

#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <m_pd.h>
#include <g_canvas.h>
#include "s_elseutf8.h"
#include "else_alloca.h"

#define NOTE_MINSIZE       8
#define NOTE_HANDLE_WIDTH  4

static t_class *note_class, *notesink_class, *handle_class, *edit_proxy_class;
static t_widgetbehavior note_widgetbehavior;

static t_pd *notesink = 0;

static void note_getrect(t_gobj *z, t_glist *gl, int *xp1, int *yp1, int *xp2, int *yp2);

typedef struct _edit_proxy{
    t_object      p_obj;
    t_symbol     *p_sym;
    t_clock      *p_clock;
    struct _note *p_cnv;
}t_edit_proxy;

typedef struct _note{
    t_object        x_obj;
    t_edit_proxy   *x_proxy;
    t_glist        *x_glist;
    t_canvas       *x_cv;
    t_binbuf       *x_binbuf;
    char           *x_buf;      // text buf
    int             x_bufsize;  // text buf size
    int             x_keynum;
    int             x_init;
    int             x_resized;
    int             x_changed;
    int             x_edit;
    int             x_max_pixwidth;
    int             x_text_width;
    int             x_width;
    int             x_height;
    int             x_bbset;
    int             x_bbpending;
    int             x_x1;
    int             x_y1;
    int             x_x2;
    int             x_y2;
    int             x_newx2;
    int             x_dragon;
    int             x_select;
    int             x_fontsize;
    int             x_shift;
    int             x_selstart;
    int             x_start_ndx;
    int             x_end_ndx;
    int             x_selend;
    int             x_active;
    unsigned char   x_red;
    unsigned char   x_green;
    unsigned char   x_blue;
    unsigned char   x_bg[3]; // background color
    t_symbol       *x_color;    // text color (canonical hex symbol)
    t_symbol       *x_bgcolor;  // background color (canonical hex symbol)
    t_symbol       *x_keysym;
    t_symbol       *x_bindsym;
    t_symbol       *x_bindname; // for _validate pdsend callback
    t_symbol       *x_fontname;
    t_symbol       *x_receive;
    t_symbol       *x_rcv_raw;
    int             x_rcv_set;
    int             x_flag;
    int             x_r_flag;
//    int             x_old;
    int             x_text_flag;
    int             x_text_n;
    int             x_text_size;
    int             x_fontface;
    int             x_bold;
    int             x_italic;
    int             x_underline;
    int             x_transparent;  // 1 = transparent, 0 = filled (was x_bg_flag)
    int             x_textjust; // 0: left, 1: center, 2: right
    int             x_outline;
    int             x_theme;     // 1 = use Pd's fg/bg colors (default)
    int             x_theme_arg; // 1 = theme was found as the 15th creation arg (text starts after it)
    t_pd           *x_handle;
    char            x_tag_all[32];     // all%lx
    char            x_tag_txt[32];     // txt%lx (also used by the Tcl procs)
    char            x_tag_bg[32];      // bg%lx
    char            x_tag_outline[32]; // %lx_outline
    char            x_tag_in[32];      // %lx_in
    char            x_tag_handle[32];  // handle%lx
}t_note;

typedef struct _handle{
    t_pd        h_pd;
    t_note     *h_master;
    t_symbol   *h_bindsym;
    char        h_pathname[64];
    int         h_clicked;
    int         h_xclick;  // drag start (canvas coords, unzoomed)
    int         h_xmotion; // current drag position (canvas coords, unzoomed)
}t_handle;

// helper functions
static int note_vis_check(t_note *x){
    return(gobj_shouldvis((t_gobj *)x, x->x_glist) && glist_isvisible(x->x_glist));
}

// effective colors: Pd's theme colors or the note's own colors
static void note_get_fg(t_note *x, char *buf, size_t n){
    if(x->x_theme)
        snprintf(buf, n, "#%06x", THISGUI->i_foregroundcolor);
    else
        snprintf(buf, n, "%s", x->x_color->s_name);
}

static void note_get_bg(t_note *x, char *buf, size_t n){
    if(x->x_theme)
        snprintf(buf, n, "#%06x", THISGUI->i_backgroundcolor);
    else
        snprintf(buf, n, "%s", x->x_bgcolor->s_name);
}

// repaint existing canvas items with the effective colors (no full redraw)
static void note_config_colors(t_note *x){
    if(!note_vis_check(x))
        return;
    char fg[32], bg[32];
    note_get_fg(x, fg, sizeof(fg));
    note_get_bg(x, bg, sizeof(bg));
    if(!x->x_transparent){
        char line[32];
        snprintf(line, sizeof(line), "#%06x", THISGUI->i_foregroundcolor);
        pdgui_vmess(0, "crs rs rs", x->x_cv, "itemconfigure", x->x_tag_bg,
            "-outline", x->x_outline ? line : bg, "-fill", bg);
    }
    if(x->x_select)
        pdgui_vmess(0, "crs rk", x->x_cv, "itemconfigure", x->x_tag_txt,
            "-fill", THISGUI->i_selectcolor);
    else
        pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_txt, "-fill", fg);
}

// turns the theme off when a color is explicitly set (same as knob)
static void note_theme_off(t_note *x){
    if(x->x_theme){
        x->x_theme = 0;
        note_config_colors(x);
    }
}

static void note_initialize(t_note *x){
//    post("initialize");
    t_binbuf *bb = x->x_obj.te_binbuf;
    int n_args = binbuf_getnatom(bb) - 1; // number of arguments
    if(x->x_text_flag){ // let's get the text from the attribute
        int n = x->x_text_n, ac = x->x_text_size;
        t_atom* av = ALLOCA(t_atom, ac);
        char buf[128];
        for(int i = 0;  i < ac; i++){
            atom_string(binbuf_getvec(bb) + n + 1 + i, buf, 128);
            SETSYMBOL(av+i, gensym(buf));
        }
        binbuf_clear(x->x_binbuf);
        binbuf_restore(x->x_binbuf, ac, av);
        FREEA(av, t_atom, ac);
    }
    else{
        int n = x->x_theme_arg ? 15 : 14; // 14 args + theme (15th) when present
        if(n_args > n){
            int ac = n_args - n;
            t_atom* av = ALLOCA(t_atom, ac);
            char buf[128];
            for(int i = 0;  i < ac; i++){
                atom_string(binbuf_getvec(bb) + n + 1 + i, buf, 128);
                SETSYMBOL(av+i, gensym(buf));
            }
            binbuf_clear(x->x_binbuf);
            binbuf_restore(x->x_binbuf, ac, av);
            FREEA(av, t_atom, ac);
        }
    }
    binbuf_gettext(x->x_binbuf, &x->x_buf, &x->x_bufsize);
    x->x_init = 1;
}

static void note_draw_outline(t_note *x){
    if(x->x_bbset && (x->x_edit || x->x_outline)){
        int x1, y1, x2, y2;
        note_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        char *tags_outline[] = {x->x_tag_outline, x->x_tag_all};
        pdgui_vmess(0, "crr iiii ri rk rS", x->x_cv, "create", "rectangle",
            x1, y1, x2 + 2, y2 + 2,
            "-width", 1,
            "-outline", x->x_select ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor,
            "-tags", 2, tags_outline);
    }
}

static void note_draw_handle(t_note *x){
    t_handle *ch = (t_handle *)x->x_handle;
    if(ch->h_pathname[0])
        pdgui_vmess(0, "rs", "destroy", ch->h_pathname); // always destroy, bad hack, improve
    if(x->x_edit){
        int x1, y1, x2, y2;
        note_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        if(x->x_resized)
            x2 = x1 + x->x_max_pixwidth;
        pdgui_vmess(0, "rs riri rkrkrk ri rr",
            "canvas", ch->h_pathname,
            "-width", NOTE_HANDLE_WIDTH, "-height", x->x_height,
            "-bg", x->x_select ? THISGUI->i_selectcolor : THISGUI->i_gopcolor,
            "-highlightcolor", THISGUI->i_foregroundcolor,
            "-highlightbackground", x->x_select ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor,
            "-highlightthickness", 1,
            "-cursor", "sb_h_double_arrow");
        char buf[512];
        // canvasx/canvasy make the coordinates zoom-aware (unzoomed units)
        snprintf(buf, sizeof(buf),
            "bind %s <Button> {pdsend [concat %s _click 1 [.x%lx.c canvasx %%x] [.x%lx.c canvasy %%y] \\;]}",
            ch->h_pathname, ch->h_bindsym->s_name, (unsigned long)x->x_cv, (unsigned long)x->x_cv);
        pdgui_vmess(buf, NULL);
        snprintf(buf, sizeof(buf),
            "bind %s <ButtonRelease> {pdsend [concat %s _click 0 [.x%lx.c canvasx %%x] [.x%lx.c canvasy %%y] \\;]}",
            ch->h_pathname, ch->h_bindsym->s_name, (unsigned long)x->x_cv, (unsigned long)x->x_cv);
        pdgui_vmess(buf, NULL);
        snprintf(buf, sizeof(buf),
            "bind %s <Motion> {pdsend [concat %s _motion [.x%lx.c canvasx %%x] [.x%lx.c canvasy %%y] \\;]}",
            ch->h_pathname, ch->h_bindsym->s_name, (unsigned long)x->x_cv, (unsigned long)x->x_cv);
        pdgui_vmess(buf, NULL);
        char *tags_handle[] = {x->x_tag_handle, x->x_tag_all};
        pdgui_vmess(0, "crr ii rs ri ri rs rS", x->x_cv, "create", "window",
            x2 + 2, y1,
            "-anchor", "nw",
            "-width", NOTE_HANDLE_WIDTH + 2,
            "-height", x->x_height + 1 + 2,
            "-window", ch->h_pathname,
            "-tags", 2, tags_handle);
    }
}

static void note_draw_inlet(t_note *x){
    if(note_vis_check(x)){
        if(x->x_edit &&  x->x_receive == &s_){
            t_canvas *cv = glist_getcanvas(x->x_glist);
            int xpos = text_xpix(&x->x_obj, x->x_glist), ypos = text_ypix(&x->x_obj, x->x_glist);
            int color = x->x_select ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor;
            char *tags_in[] = {x->x_tag_in, x->x_tag_all};
            pdgui_vmess(0, "crr iiii rk rk rS", cv, "create", "rectangle",
                xpos, ypos, xpos+IOWIDTH, ypos+IHEIGHT-1,
                "-outline", color,
                "-fill", color,
                "-tags", 2, tags_in);
        }
    }
}

static void note_adjust_justification(t_note *x){
    int move = 0;
    if(x->x_textjust && x->x_resized){
        move = x->x_max_pixwidth - x->x_text_width;
        if(x->x_textjust == 1) // center
            move/=2;
    }
    if(move){
        int x1, y1, x2, y2;
        note_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        // getrect
        pdgui_vmess(0, "crs ii", x->x_cv, "moveto", x->x_tag_txt, x1+move, y1);
    }
}

static void note_draw(t_note *x){
    x->x_cv = glist_getcanvas(x->x_glist);
    if(!x->x_transparent && x->x_bbset){ // draw bg only if initialized
        int x1, y1, x2, y2;
        note_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        char fg[32], bg[32];
        snprintf(fg, sizeof(fg), "#%06x", THISGUI->i_foregroundcolor);
        note_get_bg(x, bg, sizeof(bg));
        char *tags_bg[] = {x->x_tag_bg, x->x_tag_all};
        pdgui_vmess(0, "crr iiii rs rs rS", x->x_cv, "create", "rectangle",
            text_xpix((t_text *)x, x->x_glist),
            text_ypix((t_text *)x, x->x_glist),
            x2 + 2,
            y2 + 2,
            "-outline", x->x_outline ? fg : bg,
            "-fill", bg,
            "-tags", 2, tags_bg);
    }
    char txt[32];
    note_get_fg(x, txt, sizeof(txt));
    char sel[32] = "";
    if(x->x_select)
        snprintf(sel, sizeof(sel), "#%06x", THISGUI->i_selectcolor);
    x->x_bbpending = 1; // bbox pending
    // Tcl proc: note_draw <bindsym> <cv> <txt tag> <all tag> <x> <y> <font> <-size> <color> <text> <width> <weight> <slant> <just> <underline>
    pdgui_vmess("note_draw", "rcrr ii si r p i rrr i",
        x->x_bindsym->s_name,                   // bindsym
        x->x_cv,                                // .x%lx.c
        x->x_tag_txt,                           // txt%lx
        x->x_tag_all,                           // all%lx
        text_xpix((t_text *)x, x->x_glist) + 1, // x
        text_ypix((t_text *)x, x->x_glist) + 1, // y
        x->x_fontname->s_name,                  // font name
        -x->x_fontsize,                         // negative = pixels
        x->x_select ? sel : txt,                // text color
        x->x_bufsize, x->x_buf,                 // text (pascalstring)
        x->x_max_pixwidth,                      // width
        x->x_bold ? "bold" : "normal",
        x->x_italic ? "italic" : "roman",
        x->x_textjust == 0 ? "left" : x->x_textjust == 1 ? "center" : "right",
        x->x_underline);                        // 0 or 1
    note_draw_handle(x); // fix weird bug????
    note_draw_inlet(x);
    note_draw_outline(x);
    note_adjust_justification(x);
}

static void note_update(t_note *x){
//    post("update");
    t_canvas *cv = x->x_cv;
    pdgui_vmess("note_update", "cs p i", cv, x->x_tag_txt,
        x->x_bufsize, x->x_buf, x->x_max_pixwidth);
    if(x->x_active){
        if(x->x_selend > x->x_selstart){ // <= TEXT SELECTION!!!!
            pdgui_vmess(0, "crrs i", cv, "select", "from", x->x_tag_txt, x->x_start_ndx);
            pdgui_vmess(0, "crrs i", cv, "select", "to", x->x_tag_txt, x->x_selend);
            pdgui_vmess(0, "crs", cv, "focus", "");
        }
        else{
            pdgui_vmess(0, "crr", cv, "select", "clear");
            pdgui_vmess(0, "crs i", cv, "icursor", x->x_tag_txt, x->x_start_ndx);
            pdgui_vmess(0, "crs", cv, "focus", x->x_tag_txt);
        }
    }
    x->x_bbpending = 1;
    pdgui_vmess("note_bbox", "rcs", x->x_bindsym->s_name, cv, x->x_tag_txt);
    note_adjust_justification(x);
}

static void note_erase(t_note *x){
    t_handle *ch = (t_handle *)x->x_handle;
    pdgui_vmess(0, "crs", x->x_cv, "delete", x->x_tag_all);
    if(ch->h_pathname[0])
        pdgui_vmess(0, "rs", "destroy", ch->h_pathname);
}

static void note_redraw(t_note *x){ // <= improve, not necessary for all cases
    if(note_vis_check(x)){
        note_erase(x);
        note_draw(x);
    }
}

static void note_grabbedkey(void *z, t_floatarg f){
    z = NULL, f = 0; // LATER think about replacing #key binding to float method with "grabbing"
}

static void note_dograb(t_note *x){
//    post("do grab");
// LATER investigate grab feature. Used here to prevent backspace erasing text.
// Done also when already active, because after clicked we lost our previous grab.
    glist_grab(x->x_glist, (t_gobj *)x, 0, (t_glistkeyfn)note_grabbedkey, 0, 0);
}

static void note_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2){
    t_note *x = (t_note *)z;
    float x1, y1, x2, y2;
    x1 = text_xpix((t_text *)x, glist);
    y1 = text_ypix((t_text *)x, glist);
    if(x->x_resized)
        x->x_width = x->x_max_pixwidth;
    int min_size = NOTE_MINSIZE;
    if(x->x_width < min_size)
        x->x_width = min_size;
    if(x->x_height < min_size)
        x->x_height = min_size;
    x2 = x1 + x->x_width;
    y2 = y1 + x->x_height;
    *xp1 = x1;
    *yp1 = y1;
    *xp2 = x2;
    *yp2 = y2;
//    post("getrect x1(%g) x2(%g) y1(%g) y2(%g) | width(%d) height(%d)", x1, x2, y1, y2, x->x_width, x->x_height);
}

static void note_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_note *x = (t_note *)z;
    if(!x->x_active && !x->x_dragon){  // ???
        t_text *t = (t_text *)z;
        t->te_xpix += dx, x->x_x1 += dx, x->x_x2 += dx;
        t->te_ypix += dy, x->x_y1 += dy, x->x_y2 += dy;
        pdgui_vmess(0, "crs ii", x->x_cv, "move", x->x_tag_all, dx, dy);
        canvas_fixlinesfor(glist, t);
    }
}

static void note_activate(t_gobj *z, t_glist *glist, int state){
    glist = NULL;
    t_note *x = (t_note *)z;
    if(state){
        note_dograb(x);
        if(x->x_active)
            return;
        pdgui_vmess(0, "crs", x->x_cv, "focus", x->x_tag_txt);
        x->x_selstart = x->x_start_ndx = x->x_start_ndx = 0;
        x->x_selend = x->x_bufsize;
        x->x_active = 1;
        pd_bind((t_pd *)x, gensym("#key"));
        pd_bind((t_pd *)x, gensym("#keyname"));
    }
    else{
        if(!x->x_active)
            return;
        pd_unbind((t_pd *)x, gensym("#key"));
        pd_unbind((t_pd *)x, gensym("#keyname"));
        pdgui_vmess(0, "rr c", "selection", "clear", x->x_cv);
        pdgui_vmess(0, "crs", x->x_cv, "focus", "");
        x->x_active = 0;
    }
    note_update(x);
}

static void note_select(t_gobj *z, t_glist *glist, int state){
    t_note *x = (t_note *)z;
    x->x_select = state;
    if(!state && x->x_active)
        note_activate(z, glist, 0);
    if(state)
        pdgui_vmess(0, "crs rk", x->x_cv, "itemconfigure", x->x_tag_txt,
            "-fill", THISGUI->i_selectcolor);
    else{
        char fg[32];
        note_get_fg(x, fg, sizeof(fg));
        pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_txt,
            "-fill", fg);
    }
    pdgui_vmess(0, "crs ri rk", x->x_cv, "itemconfigure", x->x_tag_outline,
        "-width", 1,
        "-outline", state ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor);
    if(x->x_edit){
        t_handle *ch = (t_handle *)x->x_handle;
        int fg = THISGUI->i_foregroundcolor, gop = THISGUI->i_gopcolor;
        int sel = THISGUI->i_selectcolor;
        // inlet takes the select color
        if(x->x_receive == &s_)
            pdgui_vmess(0, "crs rk rk", x->x_cv, "itemconfigure", x->x_tag_in,
                "-fill", state ? sel : fg,
                "-outline", state ? sel : fg);
        // resize handle
        if(ch->h_pathname[0])
            pdgui_vmess(0, "rr rk rk", ch->h_pathname, "configure",
                "-highlightbackground", state ? sel : fg,
                "-bg", state ? sel : gop);
    }
// A regular rtext should set 'canvas_editing' variable to its canvas, we don't do it coz
// we get keys via global binding to "#key" (and coz 'canvas_editing' isn't exported).
}

static void note_delete(t_gobj *z, t_glist *glist){
    canvas_deletelinesfor(glist, (t_text *)z);
}

static void note_vis(t_gobj *z, t_glist *glist, int vis){
    t_note *x = (t_note *)z;
    x->x_cv = glist_getcanvas(x->x_glist = glist);
    if(!x->x_init)
        note_initialize(x);
    if(vis){
        t_handle *ch = (t_handle *)x->x_handle;
        sprintf(ch->h_pathname, ".x%lx.h%lx", (unsigned long)x->x_cv, (unsigned long)ch);
        note_draw(x);
    }
    else
        note_erase(x);
}

static void note__bbox_callback(t_note *x, t_symbol *bindsym,
t_floatarg x1, t_floatarg y1, t_floatarg x2, t_floatarg y2){
    bindsym = NULL;
    if(!x->x_bbset || (x->x_height != (y2-y1)) || x->x_text_width != (x2-x1)){ // redraw
        x->x_text_width = x2-x1;
        x->x_height = y2-y1;
        x->x_y1 = y1;
        x->x_y2 = y2;
        if(x->x_resized){
            x->x_width = x->x_max_pixwidth;
            x->x_x2 = x1 + x->x_max_pixwidth;
        }
        else
            x->x_width = x2-x1, x->x_x2 = x2;
        x->x_x1 = x1;
        x->x_bbset = 1;
        note_redraw(x);
    }
    x->x_bbpending = 0;
}

static void note__click_callback(t_note *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    int xx, ndx;
    if(ac == 8 && av[0].a_type == A_SYMBOL && av[1].a_type == A_FLOAT
       && av[2].a_type == A_FLOAT && av[3].a_type == A_FLOAT
       && av[4].a_type == A_FLOAT && av[5].a_type == A_FLOAT
       && av[6].a_type == A_FLOAT && av[7].a_type == A_FLOAT){
            xx = (int)av[1].a_w.w_float;
            ndx = (int)av[3].a_w.w_float;
            note__bbox_callback(x, av[0].a_w.w_symbol, av[4].a_w.w_float, av[5].a_w.w_float, av[6].a_w.w_float, av[7].a_w.w_float);
    }
    else{
        post("bug [note]: note__click_callback");
        return;
    }
    unsigned long cv = (unsigned long)x->x_cv;
    if(x->x_glist->gl_edit){
        if(x->x_active){
            if(ndx >= 0 && ndx <= x->x_bufsize){
                x->x_start_ndx = x->x_end_ndx = ndx;
                int byte_ndx = 0;
                for(int i = 0; i < ndx; i++)
                    else_u8_inc(x->x_buf, &byte_ndx);
                x->x_selstart = x->x_selend = byte_ndx;
                note_dograb(x);
                note_update(x);
// set selection, LATER shift-click and drag
            }
        }
        else if(xx > x->x_x2 - NOTE_HANDLE_WIDTH){ // start resizing
            char buf[512];
            snprintf(buf, sizeof(buf),
                ".x%lx.c bind %s <Motion> {pdsend [concat %s _motion %s [.x%lx.c canvasx %%x] [.x%lx.c canvasy %%y]]}",
                cv, x->x_tag_txt, x->x_bindsym->s_name, x->x_bindsym->s_name, cv, cv);
            pdgui_vmess(buf, NULL);
            x->x_newx2 = x->x_x2;
            x->x_dragon = 1;
        }
    }
}

static void handle__click_callback(t_handle *ch, t_floatarg f, t_floatarg xarg, t_floatarg yarg){
    yarg = 0; // unused
    int click = (int)f;
    t_note *x = ch->h_master;
    if(ch->h_clicked && click == 0){ // Released the handle
        if(ch->h_xmotion != ch->h_xclick){
            int x1, y1, x2, y2;
            note_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
            x2 += ch->h_xmotion - ch->h_xclick; // all in unzoomed units
            x->x_resized = 1;
            t_atom undo[1];
            SETFLOAT(undo+0, x->x_max_pixwidth);
            int pixwidth = x2 - x1;
            if(pixwidth < 8)
                pixwidth = 8; // min width
            t_atom redo[1];
            SETFLOAT(redo+0, pixwidth);
            pd_undo_set_objectstate(x->x_glist, (t_pd*)x, gensym("width"), 1, undo, 1, redo);
            x->x_changed = 1;
            x->x_max_pixwidth = pixwidth;
            x->x_x1 = x1;
            x->x_x2 = x1 + pixwidth;
            canvas_dirty(x->x_glist, 1);
            note_redraw(x); // needed to call bbox callback
        }
    }
    if(click){
        x->x_bbset = 0; // arm bbox callback redraw
        ch->h_xclick = ch->h_xmotion = (int)xarg;
    }
    ch->h_clicked = click;
}

static void handle__motion_callback(t_handle *ch, t_floatarg xarg, t_floatarg yarg){
    yarg = 0; // unused
    if(ch->h_clicked){ // dragging handle
        t_note *x = ch->h_master;
        ch->h_xmotion = (int)xarg;
        int x1, y1, x2, y2;
        note_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        int newx2 = x2 + ch->h_xmotion - ch->h_xclick;
        if(newx2 < x1 + NOTE_MINSIZE)
            newx2 = x1 + NOTE_MINSIZE;
        // update outline
        pdgui_vmess(0, "crs iiii", x->x_cv, "coords", x->x_tag_outline,
            x1, y1, newx2 + 2, y2 + 2);
    }
}

static void notesink__bbox_callback(t_pd *x, t_symbol *bindsym){
    if(bindsym->s_thing == x)
        pd_unbind(x, bindsym);  // if note gone, unbind
}

static void notesink_anything(t_pd *x, t_symbol *s, int ac, t_atom *av){ // nop: avoid warnings
    (void)x, (void)s, (void)ac, (void)av;
}

static void note_get_rcv(t_note* x){
    if(!x->x_rcv_set){ // no receive set, search arguments
        t_binbuf *bb = x->x_obj.te_binbuf;
        int n_args = binbuf_getnatom(bb) - 1, i = 0; // number of arguments
        char buf[128];
        if(n_args > 0){ // we have arguments, let's search them
            if(x->x_flag){ // arguments are flags actually
                if(x->x_r_flag){ // we got a receive flag, let's get it
                    for(i = 0;  i <= n_args; i++){
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
                int arg_n = 4; // receive argument number
                if(n_args >= arg_n){ // we have it, get it
                    atom_string(binbuf_getvec(bb) + arg_n, buf, 128);
                    x->x_rcv_raw = gensym(buf);
                }
            }
        }
    }
    if(x->x_rcv_raw == &s_ || x->x_rcv_raw == gensym("?"))
        x->x_rcv_raw = gensym("empty");
}
    
static void note_save(t_gobj *z, t_binbuf *b){
    t_note *x = (t_note *)z;
    if(!x->x_init) // hack???
        note_initialize(x);
    t_binbuf *bb = x->x_obj.te_binbuf;
    note_get_rcv(x);
    binbuf_addv(b, "ssiisiissiiiiiiiiiii",
        gensym("#X"),
        gensym("obj"),
        (int)x->x_obj.te_xpix,
        (int)x->x_obj.te_ypix,
        atom_getsymbol(binbuf_getvec(bb)),
        x->x_resized ? x->x_max_pixwidth : 0,
        x->x_fontsize,
        x->x_fontname,
        x->x_rcv_raw,
        x->x_fontface,
        (int)x->x_red,
        (int)x->x_green,
        (int)x->x_blue,
        x->x_underline,
        (int)x->x_bg[0],
        (int)x->x_bg[1],
        (int)x->x_bg[2],
        !x->x_transparent, // saved as old bg flag (1=filled, 0=transparent) for backwards compat
        x->x_textjust,
        x->x_theme); // 15th: theme (saved last, right before the note text)
    binbuf_addbinbuf(b, x->x_binbuf); // the actual note
    binbuf_addv(b, ";");
}

static void note_key(t_note *x){
    if(!x->x_active){
        post("key bug");
        return;
    }
    int i, newsize, ndel, n = x->x_keynum;
    if(n){
//        post("note_float => input character = [%c], n = %d", n, n);
        if (n == '\r')
            n = '\n';
        if (n == '\b'){  // backspace
            // LATER delete the box if all text is selected...
            // this causes reentrancy problems now.
            // if ((!x->x_selstart) && (x->x_selend == x->x_bufsize)){}
            if(x->x_selstart && (x->x_selstart == x->x_selend)){
                else_u8_dec(x->x_buf, &x->x_selstart);
                x->x_start_ndx--, x->x_end_ndx--;
            }
        }
        else if(n == 127){    // delete
            if(x->x_selend < x->x_bufsize && (x->x_selstart == x->x_selend))
                else_u8_inc(x->x_buf, &x->x_selend);
        }
        ndel = x->x_selend - x->x_selstart;
        for(i = x->x_selend; i < x->x_bufsize; i++)
            x->x_buf[i-ndel] = x->x_buf[i];
        newsize = x->x_bufsize - ndel;
        x->x_buf = resizebytes(x->x_buf, x->x_bufsize, newsize);
        x->x_bufsize = newsize;
        if(n == '\n' || (n > 31 && n < 127)){ // accepted ascii characters
            newsize = x->x_bufsize+1;
            x->x_buf = resizebytes(x->x_buf, x->x_bufsize, newsize);
            for(i = x->x_bufsize; i > x->x_selstart; i--)
                x->x_buf[i] = x->x_buf[i-1];
            x->x_buf[x->x_selstart] = n;
            x->x_bufsize = newsize;
            x->x_selstart++, x->x_start_ndx++, x->x_end_ndx++;
        }
        else if(n > 127){ // check for unicode codepoints beyond 7-bit ASCII
            int ch_nbytes = else_u8_wc_nbytes(n);
            newsize = x->x_bufsize + ch_nbytes;
            x->x_buf = resizebytes(x->x_buf, x->x_bufsize, newsize);
            for(i = newsize-1; i > x->x_selstart; i--)
                x->x_buf[i] = x->x_buf[i-ch_nbytes];
            x->x_bufsize = newsize;
            // assume canvas_key() has encoded keysym as UTF-8
            strncpy(x->x_buf+x->x_selstart, x->x_keysym->s_name, ch_nbytes);
            x->x_selstart = x->x_selstart + ch_nbytes;
            x->x_start_ndx++, x->x_end_ndx++;
        }
        x->x_selend = x->x_selstart;
        x->x_glist->gl_editor->e_textdirty = 1;
    }
    else if(x->x_keysym == gensym("Home")){
        if(x->x_selend == x->x_selstart)
            x->x_selend = x->x_selstart = x->x_start_ndx = x->x_end_ndx = 0;
        else // selection
            x->x_selstart = x->x_start_ndx = x->x_end_ndx = 0;
    }
    else if(x->x_keysym == gensym("End")){
        if(x->x_selend == x->x_selstart){
            while(x->x_selstart < x->x_bufsize){
                else_u8_inc(x->x_buf, &x->x_selstart);
                x->x_start_ndx++, x->x_end_ndx++;
            }
            x->x_selend = x->x_selstart = x->x_bufsize;
        }
        else // selection
            x->x_selend = x->x_bufsize;
    }
    else if(x->x_keysym == gensym("Up")){ // go to start and deselect
        if(x->x_selstart){
            else_u8_dec(x->x_buf, &x->x_selstart);
            x->x_start_ndx--, x->x_end_ndx--;
        }
        while(x->x_selstart > 0 && x->x_buf[x->x_selstart] != '\n'){
            else_u8_dec(x->x_buf, &x->x_selstart);
            x->x_start_ndx--, x->x_end_ndx--;
        }
        x->x_selend = x->x_selstart;
    }
    else if(x->x_keysym == gensym("Down")){ // go to end and deselect
        while(x->x_selend < x->x_bufsize && x->x_buf[x->x_selend] != '\n'){
            else_u8_inc(x->x_buf, &x->x_selend);
            x->x_start_ndx++, x->x_end_ndx++;
        }
        if(x->x_selend < x->x_bufsize){
            else_u8_inc(x->x_buf, &x->x_selend);
            x->x_start_ndx++, x->x_end_ndx++;
        }
        x->x_selstart = x->x_selend;
    }
    else if(x->x_keysym == gensym("Right")){
        if(x->x_selend == x->x_selstart && x->x_selstart < x->x_bufsize){
            else_u8_inc(x->x_buf, &x->x_selstart);
            x->x_start_ndx++, x->x_end_ndx++;
            x->x_selend = x->x_selstart;
        }
        else{
//            x->x_selstart = x->x_selend;
//            post("else: x->x_selstart = x->x_selend = %d", x->x_selstart);
            while(x->x_selstart < x->x_selend){
                else_u8_inc(x->x_buf, &x->x_selstart);
                x->x_start_ndx++, x->x_end_ndx++;
            }
        }
    }
    else if(x->x_keysym == gensym("Left")){
        if(x->x_selend == x->x_selstart && x->x_selstart > 0){
            else_u8_dec(x->x_buf, &x->x_selstart);
            x->x_start_ndx--, x->x_end_ndx--;
            x->x_selend = x->x_selstart;
        }
        else
            x->x_selend = x->x_selstart;
    }
    else if(x->x_keysym == gensym("F5")){
        t_text *t = (t_text *)x;
        t_binbuf *bb = binbuf_new();
        int argc = binbuf_getnatom(x->x_binbuf);
        binbuf_addv(bb, "ii", (int)t->te_xpix + 5, (int)t->te_ypix + 5);
        binbuf_add(bb, argc, binbuf_getvec(x->x_binbuf));
        canvas_setcurrent(x->x_glist);
        typedmess((t_pd *)x->x_glist, gensym("text"), argc + 2, binbuf_getvec(bb));
        canvas_unsetcurrent(x->x_glist);
        binbuf_free(bb);
        return;
    }
    canvas_dirty(x->x_glist, 1);
    binbuf_text(x->x_binbuf, x->x_buf, x->x_bufsize);
//        post("x->x_selend = x->x_selstart = %d", x->x_selend);
    note_update(x);
}

static void note_float(t_note *x, t_float f){
    x->x_keynum = (int)f;
//    post("x->x_keynum = %d", x->x_keynum);
}

static void note_list(t_note *x, t_symbol *s, int ac, t_atom *av){
    t_symbol *dummy = s;
    dummy = NULL;
    if(ac > 1 && av->a_type == A_FLOAT && av[1].a_type == A_SYMBOL){
        int press = (int)av->a_w.w_float;
//        post("press = %d, symbol = %s", press, av[1].a_w.w_symbol->s_name);
        if(av[1].a_w.w_symbol == gensym("Shift_L")){
            x->x_shift = press;
//            post("shift = %d", press);
        }
        if(press){
            x->x_keysym = av[1].a_w.w_symbol;
            note_key(x);
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
            if(edit){
                note_draw_handle(p->p_cnv); // fix weird bug????
                note_draw_inlet(p->p_cnv);
                if(!p->p_cnv->x_outline)
                    note_draw_outline(p->p_cnv);
            }
            else{
                t_canvas *cv = glist_getcanvas(p->p_cnv->x_glist);
                t_handle *ch = (t_handle *)p->p_cnv->x_handle;
                pdgui_vmess(0, "crs", cv, "delete", p->p_cnv->x_tag_in);
                if(!p->p_cnv->x_outline)
                    pdgui_vmess(0, "crs", cv, "delete", p->p_cnv->x_tag_outline);
                if(ch->h_pathname[0])
                    pdgui_vmess(0, "rs", "destroy", ch->h_pathname); // always destroy, bad hack, improve
            }
        }
    }
}

//---------------------------- METHODS ----------------------------------------
static void note_receive(t_note *x, t_symbol *s){
    if(s == gensym(""))
        s = gensym("empty");
    t_symbol *rcv = s == gensym("empty") ? &s_ : canvas_realizedollar(x->x_glist, s);
    if(rcv != x->x_receive){
        x->x_changed = 1;
        if(x->x_receive != &s_)
            pd_unbind(&x->x_obj.ob_pd, x->x_receive);
        x->x_rcv_set = 1;
        x->x_rcv_raw = s;
        x->x_receive = rcv;
        if(x->x_receive == &s_){
            if(x->x_edit)
                note_draw_inlet(x);
        }
        else{
            pd_bind(&x->x_obj.ob_pd, x->x_receive);
            if(x->x_edit && note_vis_check(x))
                pdgui_vmess(0, "crs", glist_getcanvas(x->x_glist), "delete", x->x_tag_in);
        }
    }
}

static void note_set(t_note *x, t_symbol *s, int ac, t_atom * av){
    s = NULL;
    if(!x->x_init) // hack???
        note_initialize(x);
    binbuf_clear(x->x_binbuf);
    binbuf_restore(x->x_binbuf, ac, av);
    binbuf_gettext(x->x_binbuf, &x->x_buf, &x->x_bufsize);
    x->x_bbset = 0;
    note_redraw(x);
}

static void note_append(t_note *x, t_symbol *s, int ac, t_atom * av){
    s = NULL;
    if(!x->x_init) // hack???
        note_initialize(x);
    if(ac){
        int n = binbuf_getnatom(x->x_binbuf); // number of arguments
        t_atom *at = ALLOCA(t_atom, (n+ac));
        char buf[128];
        int i = 0;
        for(i = 0;  i < n; i++){
            atom_string(binbuf_getvec(x->x_binbuf) + i, buf, 128);
            SETSYMBOL(at+i, gensym(buf));
        }
        for(int j = 0; j < ac; j++)
            at[i+j] = av[j];
        binbuf_clear(x->x_binbuf);
        binbuf_restore(x->x_binbuf, n+ac, at);
        binbuf_gettext(x->x_binbuf, &x->x_buf, &x->x_bufsize);
        x->x_bbset = 0;
        note_redraw(x);
        FREEA(at, t_atom, (n+ac));
    }
}

static void note_prepend(t_note *x, t_symbol *s, int ac, t_atom * av){
//    post("prepend");
    s = NULL;
    if(!x->x_init) // hack???
        note_initialize(x);
    if(ac){
        int n = binbuf_getnatom(x->x_binbuf); // number of arguments
        t_atom* at = ALLOCA(t_atom, (n+ac));
        char buf[128];
        int i = 0;
        for(i = 0; i < ac; i++)
            at[i] = av[i];
        for(int j = 0;  j < n; j++){
            atom_string(binbuf_getvec(x->x_binbuf) + j, buf, 128);
            SETSYMBOL(at+i+j, gensym(buf));
        }
        binbuf_clear(x->x_binbuf);
        binbuf_restore(x->x_binbuf, n+ac, at);
        binbuf_gettext(x->x_binbuf, &x->x_buf, &x->x_bufsize);
        x->x_bbset = 0;
        note_redraw(x);
        FREEA(at, t_atom, (n+ac));
    }
}

// ---------------------- colors (knob-style) ----------------------
static t_symbol *note_getcolor(int ac, t_atom *av){
    if(ac < 1)
        return(gensym("#ffffff"));
    if(av->a_type == A_SYMBOL)
        return(atom_getsymbol(av));
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

// internal setter: RGB + symbol + repaint
static void note_settextcolor(t_note *x, unsigned char r, unsigned char g, unsigned char b){
    if(x->x_red != r || x->x_green != g || x->x_blue != b){
        char colorbuf[8];
        sprintf(colorbuf, "#%.2x%.2x%.2x", x->x_red = r, x->x_green = g, x->x_blue = b);
        x->x_color = gensym(colorbuf);
        if(note_vis_check(x)){
            char fg[32];
            note_get_fg(x, fg, sizeof(fg));
            pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure",
                x->x_tag_txt, "-fill", fg);
        }
    }
}

// internal setter: RGB + symbol + repaint (un-hides if transparent)
static void note_setbgcolor(t_note *x, unsigned char r, unsigned char g, unsigned char b){
    if(x->x_transparent){ // setting bg un-hides (same as old behaviour)
        x->x_transparent = 0;
        char colorbuf[8];
        sprintf(colorbuf, "#%.2x%.2x%.2x", x->x_bg[0] = r, x->x_bg[1] = g, x->x_bg[2] = b);
        x->x_bgcolor = gensym(colorbuf);
        x->x_bbset = 0;
        note_redraw(x);
    }
    else if(x->x_bg[0] != r || x->x_bg[1] != g || x->x_bg[2] != b){
        char colorbuf[8];
        sprintf(colorbuf, "#%.2x%.2x%.2x", x->x_bg[0] = r, x->x_bg[1] = g, x->x_bg[2] = b);
        x->x_bgcolor = gensym(colorbuf);
        char fg[32], bg[32];
        snprintf(fg, sizeof(fg), "#%06x", THISGUI->i_foregroundcolor);
        note_get_bg(x, bg, sizeof(bg));
        if(note_vis_check(x))
            pdgui_vmess(0, "crs rs rs", x->x_cv, "itemconfigure", x->x_tag_bg,
                "-outline", x->x_outline ? fg : bg,
                "-fill", bg);
    }
}

static void note_send_color_validate(t_note *x, const char *color, int which){
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

static void note_validate(t_note *x, t_floatarg v, t_symbol *color, t_floatarg t){
    int valid = (int)v;
    int type = (int)t;
    if(!valid){
        if(type == 0)
            pd_error(0, "[note] invalid background color '%s'", color->s_name);
        else
            pd_error(0, "[note] invalid text color '%s'", color->s_name);
        return;
    }
    unsigned char r = 0, g = 0, b = 0;
    if(color->s_name[0] == '#'){
        long rgb = strtol(color->s_name + 1, NULL, 16);
        r = (unsigned char)((rgb >> 16) & 0xFF);
        g = (unsigned char)((rgb >> 8) & 0xFF);
        b = (unsigned char)(rgb & 0xFF);
    }
    if(type == 0)
        note_setbgcolor(x, r, g, b);
    else
        note_settextcolor(x, r, g, b);
}

static void note_textcolor(t_note *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(!ac)
        return;
    note_theme_off(x);
    note_send_color_validate(x, note_getcolor(ac, av)->s_name, 1);
}

static void note_bgcolor(t_note *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(!ac)
        return;
    note_theme_off(x);
    note_send_color_validate(x, note_getcolor(ac, av)->s_name, 0);
}

static void note_colors(t_note *x, t_symbol *bg, t_symbol *fg){
    note_theme_off(x);
    t_atom at[1];
    SETSYMBOL(at, bg);
    note_bgcolor(x, NULL, 1, at);
    SETSYMBOL(at, fg);
    note_textcolor(x, NULL, 1, at);
}

static void note_fontname(t_note *x, t_symbol *name){
    if(name != x->x_fontname){
        x->x_fontname = name;
        x->x_bbset = 0;
        note_redraw(x);
    }
}

static void note_fontsize(t_note *x, t_floatarg f){
    int size = (int)f < 5 ? 5 : (int)f;
    if(x->x_fontsize != size){
        x->x_fontsize = size;
        x->x_bbset = 0;
        note_redraw(x);
    }
}

static void note_italic(t_note *x, t_float f){
    int italic = f != 0;
    if(italic != x->x_italic){
        x->x_italic = italic;
        x->x_bbset = 0;
        note_redraw(x);
        x->x_fontface = x->x_bold + 2 * x->x_italic + 4 * x->x_outline;
    }
}

static void note_bold(t_note *x, t_float f){
    int bold = f != 0;
    if(bold != x->x_bold){
        x->x_bold = bold;
        x->x_bbset = 0;
        note_redraw(x);
        x->x_fontface = x->x_bold + 2 * x->x_italic + 4 * x->x_outline;
    }
}

static void note_width(t_note *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(ac != 1)
        return;
    int width = atom_getintarg(0, ac, av);
    if(width <= 0){
        if(x->x_resized){
            x->x_resized = 0;
            x->x_max_pixwidth = 425;
            x->x_width = x->x_text_width; // ????
            note_redraw(x);
        }
    }
    else{
        if(width < 8)
            width = 8; // min width
        if(x->x_max_pixwidth != width){
            x->x_max_pixwidth = width;
            x->x_resized = 1;
            note_redraw(x);
        }
    }
}

static void note_outline(t_note *x, t_floatarg outline){
    if(outline != x->x_outline){
        x->x_outline = outline;
        x->x_fontface = x->x_bold + 2 * x->x_italic + 4 * x->x_outline;
        if(note_vis_check(x)){
            if(x->x_outline || x->x_edit){
                note_draw_outline(x);
                if(!x->x_transparent)
                    pdgui_vmess(0, "crs rk", x->x_cv, "itemconfigure", x->x_tag_bg,
                        "-outline", THISGUI->i_foregroundcolor);
            }
            else{
                pdgui_vmess(0, "crs", x->x_cv, "delete", x->x_tag_outline);
                if(!x->x_transparent){
                    char bg[32];
                    note_get_bg(x, bg, sizeof(bg));
                    pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_bg,
                        "-outline", bg);
                }
            }
        }
    }
}

static void note_transp(t_note *x, t_float f){
    int transp = (int)(f != 0);
    if(x->x_transparent != transp){
        x->x_transparent = transp;
        x->x_bbset = 0;
        note_redraw(x);
    }
}

static void note_underline(t_note *x, t_float f){
    int underline = (int)(f != 0);
    if(x->x_underline != underline){
        x->x_underline = underline;
        x->x_bbset = 0;
        note_redraw(x); // itemconfigure?
    }
}

static void note_just(t_note *x, t_float f){
    int just = f < 0 ? 0 : (f > 2 ? 2 : (int)f);
//    if(just != x->x_textjust){
        x->x_textjust = just;
        x->x_bbset = 0;
        note_redraw(x); // itemconfigure?
//    }
}

static void note_theme(t_note *x, t_floatarg f){
    int theme = (int)(f != 0);
    if(x->x_theme != theme){
        x->x_theme = theme;
        note_config_colors(x);
    }
}

//------------------- Properties --------------------------------------------------------
void note_properties(t_gobj *z, t_glist *gl){
    note_select(z, gl, 0);
    t_note *x = (t_note *)z;
    note_get_rcv(x);
    char pd_bg[32], pd_fg[32];
    snprintf(pd_bg, sizeof(pd_bg), "#%06X", THISGUI->i_backgroundcolor);
    snprintf(pd_fg, sizeof(pd_fg), "#%06X", THISGUI->i_foregroundcolor);
    pdgui_stub_vnew(&x->x_obj.ob_pd, "note_properties", x,
        "si iiiii i s ss ii ss",
        x->x_fontname->s_name,
        x->x_fontsize,
        x->x_resized ? x->x_max_pixwidth : 0,
        x->x_bold,
        x->x_italic,
        x->x_textjust,
        x->x_underline,
        x->x_transparent,
        x->x_rcv_raw->s_name,
        x->x_bgcolor->s_name,
        x->x_color->s_name,
        x->x_outline,
        x->x_theme,
        pd_bg,
        pd_fg);
}

static void note_ok(t_note *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_atom undo[13];
    SETSYMBOL(undo+0, x->x_fontname);
    SETFLOAT(undo+1, x->x_fontsize);
    SETFLOAT(undo+2, x->x_max_pixwidth);
    SETFLOAT(undo+3, x->x_bold);
    SETFLOAT(undo+4, x->x_italic);
    t_symbol *justification = NULL;
    if(x->x_textjust == 0)
        justification = gensym("Left");
    else if(x->x_textjust == 1)
        justification = gensym("Center");
    else
        justification = gensym("Right");
    SETSYMBOL(undo+5, justification);
    SETFLOAT(undo+6, x->x_underline);
    SETFLOAT(undo+7, x->x_transparent);
    SETSYMBOL(undo+8, x->x_bgcolor);
    SETSYMBOL(undo+9, x->x_color);
    SETFLOAT(undo+10, x->x_outline);
    SETSYMBOL(undo+11, x->x_rcv_raw);
    SETFLOAT(undo+12, x->x_theme);
    pd_undo_set_objectstate(x->x_glist, (t_pd*)x, gensym("ok"), 13, undo, ac, av);
    x->x_changed = 0;
    t_float temp_f;
    if(atom_getsymbolarg(0, ac, av) != x->x_fontname){
        x->x_changed = 1;
        x->x_fontname = atom_getsymbolarg(0, ac, av);
    }
    temp_f = atom_getfloatarg(1, ac, av);
    if(temp_f < 5)
        temp_f = 5;
    if(temp_f != x->x_fontsize){
        x->x_changed = 1;
        x->x_fontsize = (int)temp_f;
    }
    int width = atom_getfloatarg(2, ac, av);
    if(width <= 0){
        if(x->x_resized){
            x->x_changed = 1;
            x->x_resized = 0;
            x->x_max_pixwidth = 425;
            x->x_width = x->x_text_width;
        }
    }
    else{
        if(width < 8)
            width = 8;
        if(x->x_max_pixwidth != width){
            x->x_changed = 1;
            x->x_max_pixwidth = width;
            x->x_resized = 1;
        }
    }
    int bold = atom_getfloatarg(3, ac, av);
    if(bold != x->x_bold){
        x->x_changed = 1;
        x->x_bold = bold;
    }
    int italic = atom_getfloatarg(4, ac, av);
    if(italic != x->x_italic){
        x->x_changed = 1;
        x->x_italic = italic;
    }
    t_symbol *just_sym = atom_getsymbolarg(5, ac, av);
    int just = 0;
    if(just_sym == gensym("Center"))
        just = 1;
    else if(just_sym == gensym("Right"))
        just = 2;
    if(just != x->x_textjust){
        x->x_changed = 1;
        x->x_textjust = just;
    }
    int underline = (int)(atom_getfloatarg(6, ac, av) != 0);
    if(x->x_underline != underline){
        x->x_changed = 1;
        x->x_underline = underline;
    }
    int transp = (int)(atom_getfloatarg(7, ac, av) != 0);
    if(x->x_transparent != transp){
        x->x_transparent = transp;
        x->x_changed = 1;
    }
    t_symbol *bg_color = atom_getsymbolarg(8, ac, av);
    if(x->x_bgcolor != bg_color){
        x->x_bgcolor = bg_color;
        x->x_changed = 1;
        if(bg_color->s_name[0] == '#'){
            long rgb = strtol(bg_color->s_name + 1, NULL, 16);
            x->x_bg[0] = (unsigned char)((rgb >> 16) & 0xFF);
            x->x_bg[1] = (unsigned char)((rgb >> 8) & 0xFF);
            x->x_bg[2] = (unsigned char)(rgb & 0xFF);
        }
    }
    t_symbol *fg_color = atom_getsymbolarg(9, ac, av);
    if(x->x_color != fg_color){
        x->x_color = fg_color;
        x->x_changed = 1;
        if(fg_color->s_name[0] == '#'){
            long rgb = strtol(fg_color->s_name + 1, NULL, 16);
            x->x_red = (unsigned char)((rgb >> 16) & 0xFF);
            x->x_green = (unsigned char)((rgb >> 8) & 0xFF);
            x->x_blue = (unsigned char)(rgb & 0xFF);
        }
    }
    int outline = atom_getfloatarg(10, ac, av);
    if(x->x_outline != outline)
        note_outline(x, outline);
    note_receive(x, atom_getsymbolarg(11, ac, av));
    int theme = (int)atom_getintarg(12, ac, av);
    if(x->x_theme != theme){
        x->x_theme = theme;
        x->x_changed = 1;
    }
    if(x->x_changed){
        canvas_dirty(x->x_glist, 1);
        note_redraw(x);
    }
    x->x_fontface = bold + 2 * italic + 4 * outline;
}

//-------------------------------------------------------------------------------------
static void edit_proxy_free(t_edit_proxy *p){
    pd_unbind(&p->p_obj.ob_pd, p->p_sym);
    clock_free(p->p_clock);
    pd_free(&p->p_obj.ob_pd);
}

static t_edit_proxy *edit_proxy_new(t_note *x, t_symbol *s){
    t_edit_proxy *p = (t_edit_proxy*)pd_new(edit_proxy_class);
    p->p_cnv = x;
    pd_bind(&p->p_obj.ob_pd, p->p_sym = s);
    p->p_clock = clock_new(p, (t_method)edit_proxy_free);
    return(p);
}

static void note_free(t_note *x){
    if(x->x_active){
        pd_unbind((t_pd *)x, gensym("#key"));
        pd_unbind((t_pd *)x, gensym("#keyname"));
    }
    if(x->x_receive != &s_)
        pd_unbind(&x->x_obj.ob_pd, x->x_receive);
    pd_unbind(&x->x_obj.ob_pd, x->x_bindname);
    if(x->x_bindsym){
        pd_unbind((t_pd *)x, x->x_bindsym);
        if(!x->x_bbpending){
//            post("!x->x_bbpending: pd_unbind(notesink)");
            pd_unbind(notesink, x->x_bindsym);
        }
    }
//    if(x->x_binbuf) // init?
        binbuf_free(x->x_binbuf);
    if(x->x_handle){
        pd_unbind(x->x_handle, ((t_handle *)x->x_handle)->h_bindsym);
        pd_free(x->x_handle);
    }
    if(x->x_buf)
        freebytes(x->x_buf, x->x_bufsize);
    x->x_proxy->p_cnv = NULL;
    pdgui_stub_deleteforkey(x);
}

static void *note_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_note *x = (t_note *)pd_new(note_class);
    x->x_handle = pd_new(handle_class);
    t_handle *ch = (t_handle *)x->x_handle;
    ch->h_master = x;
    char hbuf[64];
    char colorbuf[8];
    sprintf(hbuf, "_h%lx", (unsigned long)ch);
    pd_bind(x->x_handle, ch->h_bindsym = gensym(hbuf));
    sprintf(x->x_tag_all, "all%lx", (unsigned long)x);
    sprintf(x->x_tag_txt, "txt%lx", (unsigned long)x);
    sprintf(x->x_tag_bg, "bg%lx", (unsigned long)x);
    sprintf(x->x_tag_outline, "%lx_outline", (unsigned long)x);
    sprintf(x->x_tag_in, "%lx_in", (unsigned long)x);
    sprintf(x->x_tag_handle, "handle%lx", (unsigned long)x);
    t_text *t = (t_text *)x;
    t->te_type = T_TEXT;
    x->x_glist = canvas_getcurrent();
    x->x_cv = canvas_getcurrent();
    x->x_fontname = gensym(sys_font);
    x->x_edit = x->x_glist->gl_edit;
    x->x_buf = 0;
    x->x_keynum = 0;
    x->x_keysym = NULL;
    x->x_rcv_set = x->x_flag = x->x_r_flag = 0; // x->x_old = 0;
    x->x_text_n = x->x_text_size = x->x_text_width = 0;
    x->x_max_pixwidth = 0;
    x->x_width = x->x_height = 0;
    x->x_fontsize = 0;
    x->x_bbpending = 0;
    x->x_textjust = x->x_fontface = x->x_bold = x->x_italic = x->x_outline = 0;
    x->x_red = x->x_green = x->x_blue = x->x_bufsize = 0;
    x->x_theme = 0;
    x->x_theme_arg = 0;
    x->x_transparent = 0;
    x->x_changed = x->x_init = x->x_resized = 0;
    x->x_bg[0] = x->x_bg[1] = x->x_bg[2] = 255;
    sprintf(colorbuf, "#%.2x%.2x%.2x", x->x_bg[0], x->x_bg[1], x->x_bg[2]);
    x->x_bgcolor = gensym(colorbuf);
    x->x_color = gensym("#000000");
    x->x_bbset = x->x_select = x->x_dragon = x->x_shift = 0;
    t_symbol *rcv = x->x_receive = x->x_rcv_raw = &s_;
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING-1, ".x%lx", (unsigned long)x->x_cv);
    buf[MAXPDSTRING-1] = 0;
    x->x_proxy = edit_proxy_new(x, gensym(buf));
    sprintf(buf, "#%lx", (long)x);
    pd_bind(&x->x_obj.ob_pd, x->x_bindname = gensym(buf));
    char symbuf[32];
    sprintf(symbuf, "note%lx", (unsigned long)x);
    x->x_bindsym = gensym(symbuf);
    pd_bind((t_pd *)x, x->x_bindsym);
    if(!notesink){
//        post("new: !notesink");
        notesink = pd_new(notesink_class);
    }
    pd_bind(notesink, x->x_bindsym);
    x->x_binbuf = binbuf_new();
    t_atom at;
    SETSYMBOL(&at, gensym("note"));
    binbuf_restore(x->x_binbuf, 1, &at);
////////////////////////////////// GET ARGS ///////////////////////////////////////////
    if(ac){
        if(ac && av->a_type == A_FLOAT){ // 1ST Width
            x->x_max_pixwidth = (int)av->a_w.w_float;
            ac--, av++;
            if(ac && av->a_type == A_FLOAT){ // 2ND Size
                x->x_fontsize = (int)av->a_w.w_float;
                ac--, av++;
                if(ac && av->a_type == A_SYMBOL){ // 3RD type
                    x->x_fontname = av->a_w.w_symbol;
                    ac--, av++;
                    if(ac && av->a_type == A_SYMBOL){ // 4TH RECEIVE
                        if(av->a_w.w_symbol != gensym("empty")){
                            //  'empty' sets empty receive symbol
                            rcv = av->a_w.w_symbol;
                            ac--, av++;
                        }
                        else // ignore floats
                            ac--, av++;
                        if(ac && av->a_type == A_FLOAT){ // 5th face
                            x->x_fontface = (int)av->a_w.w_float;
                            ac--, av++;
                            if(ac && av->a_type == A_FLOAT){ // 6th R
                                x->x_red = (unsigned char)av->a_w.w_float;
                                ac--, av++;
                                if(ac && av->a_type == A_FLOAT){ // 7th G
                                    x->x_green = (unsigned char)av->a_w.w_float;
                                    ac--, av++;
                                    if(ac && av->a_type == A_FLOAT){ // 8th B
                                        x->x_blue = (unsigned char)av->a_w.w_float;
                                        ac--, av++;
                                        if(ac && av->a_type == A_FLOAT){ // 9th Underline
                                            x->x_underline = (int)(av->a_w.w_float != 0);
                                            ac--, av++;
                                            if(ac && av->a_type == A_FLOAT){ // 10th R
                                                x->x_bg[0] = (unsigned int)av->a_w.w_float;
                                                ac--, av++;
                                                if(ac && av->a_type == A_FLOAT){ // 11th G
                                                    x->x_bg[1] = (unsigned int)av->a_w.w_float;
                                                    ac--, av++;
                                                    if(ac && av->a_type == A_FLOAT){ // 12th B
                                                        x->x_bg[2] = (unsigned int)av->a_w.w_float;
                                                        ac--, av++;
                                                        if(ac && av->a_type == A_FLOAT){ // 13th bg flag (old compat: 1=filled, 0=transparent)
                                                            x->x_transparent = (int)(av->a_w.w_float == 0);
                                                            ac--, av++;
                                                            if(ac && av->a_type == A_FLOAT){ // 14th Justification
                                                                int textjust = (int)(av->a_w.w_float);
                                                                x->x_textjust = textjust < 0 ? 0 : textjust > 2 ? 2 : textjust;
                                                                ac--, av++;
                                                                if(ac && av->a_type == A_FLOAT){ // 15th theme (saved last)
                                                                    x->x_theme = (int)(av->a_w.w_float != 0);
                                                                    x->x_theme_arg = 1;
                                                                }
                                                                else // old file without theme: keep its saved colors
                                                                    x->x_theme = 0;
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
        else{ // 1st is not a float, so we must be dealing with attributes/flags!!!
            int i, comlen = 0; // length of note list
            for(i = 0; i < ac; i++){
                if(av[i].a_type == A_SYMBOL){
                    t_symbol * sym = av[i].a_w.w_symbol;
                    if(sym == gensym("-size")){
                        if((ac-i) >= 2){
                            x->x_flag = 1, i++;
                            if(av[i].a_type == A_FLOAT)
                                x->x_fontsize = (int)av[i].a_w.w_float;
                            else
                                goto errstate;
                        }
                        else
                            goto errstate;
                    }
                    else if(sym == gensym("-font")){
                        if((ac-i) >= 2){
                            x->x_flag = 1, i++;
                            if(av[i].a_type == A_SYMBOL)
                                x->x_fontname = av[i].a_w.w_symbol;
                            else
                                goto errstate;
                        }
                        else
                            goto errstate;
                    }
                    else if(sym == gensym("-receive")){
                        if((ac-i) >= 2){
                            x->x_flag = x->x_r_flag = 1, i++;
                            if(av[i].a_type == A_SYMBOL)
                                rcv = atom_getsymbolarg(i, ac, av);
                            else
                                goto errstate;
                        }
                        else
                            goto errstate;
                    }
                    else if(sym == gensym("-textcolor")){
                        if((ac-i) >= 4){
                            x->x_flag = 1, i++;
                            if(av[i].a_type == A_FLOAT)
                                x->x_red = (unsigned char)av[i].a_w.w_float, i++;
                            if(av[i].a_type == A_FLOAT)
                                x->x_green = (unsigned char)av[i].a_w.w_float, i++;
                            if(av[i].a_type == A_FLOAT)
                                x->x_blue = (unsigned char)av[i].a_w.w_float;
                            else
                                goto errstate;
                        }
                        else
                            goto errstate;
                    }
                    else if(sym == gensym("-bgcolor")){
                        if((ac-i) >= 4){
                            x->x_flag = 1, i++;
                            if(av[i].a_type == A_FLOAT)
                                x->x_bg[0] = (unsigned char)av[i].a_w.w_float, i++;
                            if(av[i].a_type == A_FLOAT)
                                x->x_bg[1] = (unsigned char)av[i].a_w.w_float, i++;
                            if(av[i].a_type == A_FLOAT)
                                x->x_bg[2] = (unsigned char)av[i].a_w.w_float;
                            else
                                goto errstate;
                        }
                        else
                            goto errstate;
                    }
                    else if(sym == gensym("-just")){
                        if((ac-i) >= 2){
                            x->x_flag = 1, i++;
                            if(av[i].a_type == A_FLOAT){
                                int textjust = (int)av[i].a_w.w_float;
                                x->x_textjust = textjust < 0 ? 0 : textjust > 2 ? 2 : textjust;
                            }
                            else
                                goto errstate;
                        }
                        else
                            goto errstate;
                    }
                    else if(sym == gensym("-bold"))
                        x->x_flag = 1, x->x_bold = 1;
                    else if(sym == gensym("-italic"))
                        x->x_flag = 1, x->x_italic = 1;
                    else if(sym == gensym("-underline"))
                        x->x_flag = 1, x->x_underline = 1;
                    else if(sym == gensym("-outline"))
                        x->x_flag = 1, x->x_outline = 1;
                    else if(sym == gensym("-transparent"))
                        x->x_flag = 1, x->x_transparent = 1;
                    else if(sym == gensym("-theme"))
                        x->x_flag = 1, x->x_theme = 1;
                    else if(sym == gensym("-note")){
                        if((ac-i) >= 2){
                            x->x_flag = x->x_text_flag = 1;
                            i++, comlen++;
                            x->x_text_n = i;
                        };
                    }
                    else if(x->x_text_flag){ // treat it as a part of comlist
                        comlen++;
                    }
                    else
                        goto errstate;
                }
                else if(av[i].a_type == A_FLOAT){
                    if(x->x_text_flag)
                        comlen++;
                    else
                        goto errstate;
                }
            }
            x->x_text_size = comlen;
        }
    }
    if(x->x_fontsize < 1)
        x->x_fontsize = glist_getfont(x->x_glist);
    if(x->x_max_pixwidth != 0)
        x->x_resized = 1;
    else
        x->x_max_pixwidth = 425;
    x->x_fontface = x->x_fontface < 0 ? 0 : (x->x_fontface > 7 ? 7 : x->x_fontface);
    if(x->x_fontface){
        x->x_bold = x->x_fontface == 1 || x->x_fontface == 3 || x->x_fontface == 5 || x->x_fontface == 7;
        x->x_italic = x->x_fontface == 2 || x->x_fontface == 3 || x->x_fontface >= 6;
        x->x_outline = x->x_fontface >= 4;
    }
    x->x_red = x->x_red > 255 ? 255 : x->x_red < 0 ? 0 : x->x_red;
    x->x_green = x->x_green > 255 ? 255 : x->x_green < 0 ? 0 : x->x_green;
    x->x_blue = x->x_blue > 255 ? 255 : x->x_blue < 0 ? 0 : x->x_blue;
    sprintf(colorbuf, "#%.2x%.2x%.2x", x->x_red, x->x_green, x->x_blue);
    x->x_color = gensym(colorbuf);
    x->x_bg[0] = x->x_bg[0] > 255 ? 255 : x->x_bg[0] < 0 ? 0 : x->x_bg[0];
    x->x_bg[1] = x->x_bg[1] > 255 ? 255 : x->x_bg[1] < 0 ? 0 : x->x_bg[1];
    x->x_bg[2] = x->x_bg[2] > 255 ? 255 : x->x_bg[2] < 0 ? 0 : x->x_bg[2];
    sprintf(colorbuf, "#%.2x%.2x%.2x", x->x_bg[0], x->x_bg[1], x->x_bg[2]);
    x->x_bgcolor = gensym(colorbuf);
    x->x_receive = canvas_realizedollar(x->x_glist, x->x_rcv_raw = rcv);
    if(x->x_receive != &s_)
        pd_bind(&x->x_obj.ob_pd, x->x_receive);
    return(x);
    errstate:
        pd_error(x, "[note]: improper creation arguments");
        return(NULL);
}

void note_setup(void){
    note_class = class_new(gensym("note"), (t_newmethod)note_new, (t_method)note_free,
        sizeof(t_note), CLASS_DEFAULT, A_GIMME, 0);
    class_addfloat(note_class, note_float);
    class_addlist(note_class, note_list);
    class_addmethod(note_class, (t_method)note_width, gensym("width"), A_GIMME, 0);
    class_addmethod(note_class, (t_method)note_outline, gensym("outline"), A_FLOAT, 0);
    class_addmethod(note_class, (t_method)note_fontname, gensym("font"), A_SYMBOL, 0);
    class_addmethod(note_class, (t_method)note_receive, gensym("receive"), A_SYMBOL, 0);
    class_addmethod(note_class, (t_method)note_fontsize, gensym("size"), A_FLOAT, 0);
    class_addmethod(note_class, (t_method)note_set, gensym("set"), A_GIMME, 0);
    class_addmethod(note_class, (t_method)note_append, gensym("append"), A_GIMME, 0);
    class_addmethod(note_class, (t_method)note_prepend, gensym("prepend"), A_GIMME, 0);
    class_addmethod(note_class, (t_method)note_bold, gensym("bold"), A_FLOAT, 0);
    class_addmethod(note_class, (t_method)note_italic, gensym("italic"), A_FLOAT, 0);
    class_addmethod(note_class, (t_method)note_underline, gensym("underline"), A_FLOAT, 0);
    class_addmethod(note_class, (t_method)note_just, gensym("just"), A_FLOAT, 0);
    class_addmethod(note_class, (t_method)note_bgcolor, gensym("bgcolor"), A_GIMME, 0);
    class_addmethod(note_class, (t_method)note_textcolor, gensym("textcolor"), A_GIMME, 0);
    class_addmethod(note_class, (t_method)note_colors, gensym("colors"), A_SYMBOL, A_SYMBOL, 0);
    class_addmethod(note_class, (t_method)note_validate, gensym("_validate"),
        A_FLOAT, A_SYMBOL, A_FLOAT, 0);
    class_addmethod(note_class, (t_method)note_transp, gensym("transparent"), A_FLOAT, 0);
    class_addmethod(note_class, (t_method)note_theme, gensym("theme"), A_FLOAT, 0);
    class_addmethod(note_class, (t_method)note_ok, gensym("ok"), A_GIMME, 0);
    class_addmethod(note_class, (t_method)note__bbox_callback, gensym("_bbox"),
        A_SYMBOL, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(note_class, (t_method)note__click_callback, gensym("_click"), A_GIMME, 0);
    class_setwidget(note_class, &note_widgetbehavior);
    note_widgetbehavior.w_getrectfn  = note_getrect;
    note_widgetbehavior.w_displacefn = note_displace;
    note_widgetbehavior.w_selectfn   = note_select;
    note_widgetbehavior.w_activatefn = note_activate;
    note_widgetbehavior.w_deletefn   = note_delete;
    note_widgetbehavior.w_visfn      = note_vis;
    class_setsavefn(note_class, note_save);
    class_setpropertiesfn(note_class, note_properties);
    edit_proxy_class = class_new(0, 0, 0, sizeof(t_edit_proxy), CLASS_NOINLET | CLASS_PD, 0);
    class_addanything(edit_proxy_class, edit_proxy_any);
    notesink_class = class_new(gensym("_notesink"), 0, 0, sizeof(t_pd), CLASS_PD, 0);
    class_addanything(notesink_class, notesink_anything);
    class_addmethod(notesink_class, (t_method)notesink__bbox_callback, gensym("_bbox"), A_SYMBOL, 0); // <= ?????
    handle_class = class_new(gensym("_handle"), 0, 0, sizeof(t_handle), CLASS_PD, 0);
    class_addmethod(handle_class, (t_method)handle__click_callback, gensym("_click"), A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(handle_class, (t_method)handle__motion_callback, gensym("_motion"), A_FLOAT, A_FLOAT, 0);
    #include "../Extra/else_tcl_utils.h"
    pdgui_vmess(else_tcl_utils, NULL);
    #include "../Extra/note_dialog.h"
    pdgui_vmess(note_dialog_tcl, NULL);
}
