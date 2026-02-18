// By Porres and Tim Schoen
// Primarily based on the knob proposal for Vanilla by Ant1, Porres and others

#include <m_pd.h>
#include <g_canvas.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include "mouse_gui.h"

#ifdef _MSC_VER
#define snprintf _snprintf
#endif

#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#define HALF_PI (M_PI / 2)
#define MAX_NUMBOX_LEN  32
#define MIN_SIZE        16
#define HANDLE_SIZE     12
#define KNOB_SELBDWIDTH 2
#define NAN_V           0x7FFFFFFFul
#define POS_INF         0x7F800000ul
#define NEG_INF         0xFF800000ul

#if __APPLE__
char def_font[100] = "Menlo";
#else
char def_font[100] = "DejaVu Sans Mono";
#endif

typedef struct _edit_proxy{
    t_object      p_obj;
    t_symbol     *p_sym;
    t_clock      *p_clock;
    struct _knob *p_cnv;
}t_edit_proxy;

typedef struct _knob{
    t_object        x_obj;
    t_edit_proxy   *x_proxy;
    t_glist        *x_glist;
    t_canvas       *x_cv;
    int             x_ctrl;
    int             x_size;
    double          x_pos;          // 0-1 normalized position
    t_float         x_exp;
    int             x_expmode;
    int             x_log;
    t_float         x_load;         // value when loading patch
    t_float         x_arcstart;     // arc start value
    t_float         x_radius;
    float           x_start_angle;
    float           x_drag_start_pos;
    int             x_arcstart_angle;
    int             x_end_angle;
    int             x_angle_range;
    int             x_angle_offset;
    int             x_steps;
    int             x_square;
    int             x_setted;
    double          x_lower;
    double          x_upper;
    int             x_select;
    int             x_clicked;
    int             x_typing;
    int             x_shownum;
    int             x_number_mode;
    int             x_ticks;
    int             n_size;
    int             x_xpos;
    int             x_ypos;
    int             x_sel;
    int             x_shift;
    int             x_edit;
    int             x_hover;
    int             x_jump;
    int             x_readonly;
    int             x_theme;
    int             x_transparent;
    double          x_fval;
    double          x_lastval;
    t_symbol       *x_fg;
    t_symbol       *x_mg;
    t_symbol       *x_bg;
    t_symbol       *x_param;
    t_symbol       *x_var;
    t_symbol       *x_var_raw;
    t_symbol       *x_cname;
    int            x_var_set;
    int            x_savestate;
    int            x_lb;
    t_symbol       *x_snd;
    t_symbol       *x_snd_raw;
    int             x_flag;
    int             x_r_flag;
    int             x_s_flag;
    int             x_v_flag;
    int             x_rcv_set;
    int             x_snd_set;
    t_symbol       *x_rcv;
    t_symbol       *x_rcv_raw;
    int             x_circular;
    int             x_arc;
    int             x_zoom;
    int             x_discrete;
    char            x_tag_fg[32];
    char            x_tag_obj[32];
    char            x_tag_handle[32];
    char            x_tag_base_circle[32];
    char            x_tag_bg_arc[32];
    char            x_tag_arc[32];
    char            x_tag_center_circle[32];
    char            x_tag_wiper[32];
    char            x_tag_wpr_c[32];
    char            x_tag_ticks[32];
    char            x_tag_outline[32];
    char            x_tag_square[32];
    char            x_tag_hover[32];
    char            x_tag_in[32];
    char            x_tag_IO[32];
    char            x_tag_out[32];
    char            x_tag_sel[32];
    char            x_tag_number[32];
    char            x_buf[MAX_NUMBOX_LEN]; // number buffer
    t_symbol       *x_ignore;
    int             x_ignore_int;
    t_symbol       *x_bindname;
// handle
    t_pd           *x_handle;
}t_knob;

typedef struct _handle{
    t_pd            h_pd;
    t_knob         *h_master;
    t_symbol       *h_bindsym;
    char            h_pathname[64], h_outlinetag[64];
    int             h_dragon, h_drag_delta;
//    int             h_dragx, h_dragy;
}t_handle; // resizing handle

t_widgetbehavior knob_widgetbehavior;
static t_class *knob_class, *handle_class, *edit_proxy_class;

// ---------------------- Helper functions ----------------------

static t_float knob_wrap(t_knob *x, t_float f){
    float result;
    float min = x->x_lower, max = x->x_upper;
    if(min == max)
        result = min;
    else if(f < max && f >= min)
        result = f; // if f range, = in
    else{ // wrap
        float range = max - min;
        if(f < min){
            result = f;
            while(result < min)
                result += range;
        }
        else
            result = fmod(f - min, range) + min;
    }
    return(result);
}

static int knob_vis_check(t_knob *x){
    return(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist));
}

static void knob_get_var(t_knob* x){
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
                int arg_n = 21; // var argument number
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

static void knob_get_snd(t_knob* x){
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
                int arg_n = 6; // send argument number
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

static void knob_get_rcv(t_knob* x){
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

static void get_cname(t_knob *x, t_floatarg depth){
    t_canvas *canvas = canvas_getrootfor(canvas_getcurrent());
    while(depth-- && canvas->gl_owner)
        canvas = canvas_getrootfor(canvas->gl_owner);
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING, ".x%lx-link", (long unsigned int)canvas);
    x->x_cname = gensym(buf);
}

static char* knob_get_number(t_knob *x) {
    static char nbuf[16];  // Make nbuf static so it persists after the function returns
    float absv = fabs(x->x_fval);
    if(absv < 10)
        sprintf(nbuf, "% .3f", x->x_fval);
    else if(absv < 100)
        sprintf(nbuf, "% .2f", x->x_fval);
    else if(absv < 1000)
        sprintf(nbuf, "% .1f", x->x_fval);
    else
        sprintf(nbuf, "% d", (int)x->x_fval);
    return(nbuf);  // Return the pointer to the string
}

static void knob_update_number(t_knob *x){
    if(knob_vis_check(x))
        pdgui_vmess(0, "crs rs", glist_getcanvas(x->x_glist),
            "itemconfigure", x->x_tag_number, "-text", knob_get_number(x));
}

// get value from motion/position
static t_float knob_getfval(t_knob *x){
    double fval;
    double pos = x->x_pos;
    if(x->x_discrete){ // later just 1 tick case
        t_float steps = (x->x_steps < 2 ? 2 : (float)x->x_steps) - 1;
        pos = rint(pos * steps) / steps;
    }
    if(x->x_log == 1){ // log
        if((x->x_lower <= 0 && x->x_upper >= 0) || (x->x_lower >= 0 && x->x_upper <= 0)){
            pd_error(x, "[knob]: range can't contain '0' in log mode");
            fval = x->x_lower;
        }
        else
            fval = exp(pos * log(x->x_upper / x->x_lower)) * x->x_lower;
    }
    else{
        if(x->x_exp != 0){
            if(x->x_exp > 0)
                pos = pow(pos, x->x_exp);
            else
                pos = 1 - pow(1 - pos, -x->x_exp);
        }
        fval = pos * (x->x_upper - x->x_lower) + x->x_lower;
    }
    if((fval < 1.0e-10) && (fval > -1.0e-10))
        fval = 0.0;
    return(fval);
}

// get position from value
static t_float knob_getpos(t_knob *x, t_floatarg fval){
    double pos;
    if(x->x_circular == 2)
        fval = knob_wrap(x, fval);
    if(x->x_log == 1){ // log
        if((x->x_lower <= 0 && x->x_upper >= 0) || (x->x_lower >= 0 && x->x_upper <= 0))
            pos = 0;
        else
            pos = (double)log(fval/x->x_lower) / (double)log(x->x_upper/x->x_lower);
    }
    else{
        pos = (double)(fval - x->x_lower) / (double)(x->x_upper - x->x_lower);
        if(x->x_exp != 0){
            if(x->x_exp > 0)
                pos = pow(pos, 1.0/x->x_exp);
            else
                pos = 1-pow(1-pos, 1.0/(-x->x_exp));
        }
    }
    if(x->x_discrete){
        t_float steps = (float)(x->x_steps) -1;
        if(steps <= 0)
            pos = (x->x_load - x->x_lower) / (x->x_upper - x->x_lower); // ?????
        else
            pos = rint(pos * steps) / steps;
    }
    return(pos);
}

// get clipped float value
static t_float knob_clipfloat(t_knob *x, t_floatarg f){
    if(x->x_upper < x->x_lower)
        f = f < x->x_upper ? x->x_upper : f > x->x_lower ? x->x_lower : f;
    else
        f = f > x->x_upper ? x->x_upper : f < x->x_lower ? x->x_lower : f;
    return(f);
}

// ---------------------- Configure / Update GUI ----------------------

// configure colors
static void knob_config_fg(t_knob *x){
    t_canvas *cv = glist_getcanvas(x->x_glist);
    char fg[32];
    if(x->x_theme)
        snprintf(fg, sizeof(fg), "#%06x", THISGUI->i_foregroundcolor);
    else
        snprintf(fg, sizeof(fg), "%s", x->x_fg->s_name);
    pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_fg, "-fill", fg);
    pdgui_vmess(0, "crs rsrs", cv, "itemconfigure", x->x_tag_wpr_c,
        "-outline", fg, "-fill", fg);
    pdgui_vmess(0, "crs rsrs", cv, "itemconfigure", x->x_tag_arc,
        "-outline", fg, "-fill", fg);
    pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_IO,
        "-outline", fg);
}

static void knob_config_mg(t_knob *x){
    t_canvas *cv = glist_getcanvas(x->x_glist);
    char mg[32];
    if(x->x_theme)
        snprintf(mg, sizeof(mg), "#%06x", THISGUI->i_foregroundcolor);
    else
        snprintf(mg, sizeof(mg), "%s", x->x_mg->s_name);
    pdgui_vmess(0, "crs rsrs", cv, "itemconfigure",  x->x_tag_bg_arc,
        "-outline", mg, "-fill", mg);
    pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_base_circle,
        "-outline", x->x_transparent > 0 ? mg : x->x_arc ? "" : mg);
}

static void knob_config_bg(t_knob *x){
    t_canvas *cv = glist_getcanvas(x->x_glist);
    char bg[32];
    if(x->x_theme)
        snprintf(bg, sizeof(bg), "#%06x", THISGUI->i_backgroundcolor);
    else
        snprintf(bg, sizeof(bg), "%s", x->x_bg->s_name);
    pdgui_vmess(0, "crs rsrsrs", cv, "itemconfigure", x->x_tag_center_circle,
        "-outline", bg, "-fill", bg, "-state", x->x_transparent > 0 ? "hidden" : "normal");
    pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_base_circle, "-fill",
        x->x_transparent > 0 ? "" : bg);
    if(x->x_square)
        pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_square, "-fill",
            x->x_transparent > 0 ? "" : bg);
}

static void knob_config_number(t_knob *x){ // show or hide number value
    t_atom at[2];
    SETSYMBOL(at, gensym(def_font));
    SETFLOAT(at+1, -x->n_size*x->x_zoom);
    t_canvas *cv = glist_getcanvas(x->x_glist);
    pdgui_vmess(0, "crs rA", cv, "itemconfigure", x->x_tag_number, "-font", 2, at);
    int x1 = text_xpix(&x->x_obj, x->x_glist), y1 = text_ypix(&x->x_obj, x->x_glist);
    pdgui_vmess(0, "crs ii", cv, "moveto", x->x_tag_number,
        x1 + x->x_xpos*x->x_zoom, y1 + x->x_ypos*x->x_zoom);
}

static void show_number(t_knob *x, int force){ // show or hide number value
    if(x->x_number_mode == 1 || (x->x_number_mode == 2 && x->x_clicked))
        x->x_shownum = 1; // mode 1 or mode 2 and clicked
    else if(x->x_number_mode == 3 && x->x_typing)
        x->x_shownum = 1; // mode 3 and typing
    else if(x->x_number_mode == 4 // mode 4 (hovering)
    && (x->x_hover || x->x_clicked)
    && !x->x_edit){
        x->x_shownum = 1;
    }
    else
        x->x_shownum = 0;
    if((knob_vis_check(x)) || force){
        pdgui_vmess(0, "crs rs", glist_getcanvas(x->x_glist), "itemconfigure",
            x->x_tag_number, "-state", x->x_shownum ? "normal" : "hidden");
    }
}

// configure size
static void knob_config_size(t_knob *x){
    int z = x->x_zoom;
    int x1 = text_xpix(&x->x_obj, x->x_glist);
    int y1 = text_ypix(&x->x_obj, x->x_glist);
    int x2 = x1 + x->x_size * z;
    int y2 = y1 + x->x_size * z;
    int circle_width = x->x_size * (x->x_square ? x->x_radius : 1);
    int offset = (x->x_size - circle_width) * 0.5;
    t_canvas *cv = glist_getcanvas(x->x_glist);
// inlet, outlet, square/outline
    pdgui_vmess(0, "crs iiii", cv, "coords", x->x_tag_in,
        x1, y1, x1 + IOWIDTH*z, y1 + IHEIGHT*z);
    pdgui_vmess(0, "crs iiii", cv, "coords", x->x_tag_out,
        x1, y2 - OHEIGHT*z, x1 + IOWIDTH*z, y2);
    pdgui_vmess(0, "crs iiii", cv, "coords", x->x_tag_square,
        x1, y1, x2, y2);
    pdgui_vmess(0, "crs iiii", cv, "coords", x->x_tag_outline,
        x1, y1, x2, y2);
    pdgui_vmess(0, "crs iiii", cv, "coords", x->x_tag_hover,
        x1, y1, x2, y2);
    pdgui_vmess(0, "crs ri", cv, "itemconfigure", x->x_tag_outline,
        "-width", z);
// wiper's width, wiper's center circle
    int w_width = circle_width * z / 20; // wiper width is 1/20 of circle size
    pdgui_vmess(0, "crs ri", cv, "itemconfigure", x->x_tag_wiper,
        "-width", w_width < z ? z : w_width); // wiper width
    int half_size = x->x_size * z / 2; // half_size
    int xc = x1 + half_size, yc = y1 + half_size; // center coords
    int wx1 = rint(xc - w_width), wy1 = rint(yc - w_width);
    int wx2 = rint(xc + w_width), wy2 = rint(yc + w_width);
    pdgui_vmess(0, "crs iiii", cv, "coords", x->x_tag_wpr_c,
        wx1, wy1, wx2, wy2); // wiper center circle
// knob circle stuff
    // knob arc
    pdgui_vmess(0, "crs iiii", cv, "coords", x->x_tag_bg_arc,
        x1 + offset + z, y1 + offset + z,
        x2 - offset - z, y2 - offset - z);
    pdgui_vmess(0, "crs iiii", cv, "coords", x->x_tag_arc,
        x1 + offset + z, y1 + offset + z,
        x2 - offset - z, y2 - offset - z);
    // knob circle (base)
    pdgui_vmess(0, "crs iiii", cv, "coords", x->x_tag_base_circle,
        x1 + offset, y1 + offset, x2 - offset, y2 - offset);
    pdgui_vmess(0, "crs ri", cv, "itemconfigure", x->x_tag_base_circle,
        "-width", z);
    // knob center
    int arcwidth = circle_width * z * 0.1; // arc width is 1/10 of circle
    if(arcwidth < 1)
        arcwidth = 1;
    pdgui_vmess(0, "crs iiii", cv, "coords", x->x_tag_center_circle,
        x1 + arcwidth + offset * z, y1 + arcwidth + offset * z,
        x2 - arcwidth - offset * z, y2 - arcwidth - offset * z);
}

// configure wiper center
static void knob_config_wcenter(t_knob *x){
    t_canvas *cv = glist_getcanvas(x->x_glist);
    pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_wpr_c,
        "-state", x->x_clicked ? "normal" : "hidden");
}

// configure inlet outlet and outline/square
static void knob_config_io(t_knob *x){
    int inlet = (x->x_rcv == &s_ || x->x_rcv == gensym("empty")) && x->x_edit;
    t_canvas *cv = glist_getcanvas(x->x_glist);
    pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_in,
        "-state", inlet ? "normal" : "hidden");
    int outlet = (x->x_snd == &s_ || x->x_snd == gensym("empty")) && x->x_edit;
    pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_out,
        "-state", outlet ? "normal" : "hidden");
    int outline = x->x_edit || x->x_square;
    pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_outline,
        "-state", outline ? "normal" : "hidden");
    pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_square,
        "-state", x->x_square ? "normal" : "hidden");
}

// configure arc
static void knob_config_arc(t_knob *x){
    t_canvas *cv = glist_getcanvas(x->x_glist);
    pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_arc,
        "-state", x->x_arc && x->x_transparent == 0 ? "normal" : "hidden");
    pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_bg_arc,
        "-state", x->x_arc && x->x_transparent == 0 ? "normal" : "hidden");
}

// Update Arc/Wiper according to position
static void knob_update(t_knob *x){
    t_float pos = x->x_pos;
    t_canvas *cv = glist_getcanvas(x->x_glist);
//    post("knob_update cv = glist_getcanvas(x->x_glist) = %d", cv);
//    t_canvas *cv = x->x_cv;
    if(x->x_discrete){ // later just 1 tick case
        t_float steps = (x->x_steps < 2 ? 2 : (float)x->x_steps) -1;
        pos = rint(pos * steps) / steps;
    }
    float start = (x->x_arcstart_angle / 90.0 - 1) * HALF_PI;
    float range = x->x_angle_range / 180.0 * M_PI;
    float angle = start + pos * range; // pos angle
// configure arc
//    knob_config_arc(x);
    pdgui_vmess(0, "crs sf sf", cv, "itemconfigure", x->x_tag_bg_arc,
        "-start", start * -180.0 / M_PI,
        "-extent", range * -179.99 / M_PI);
    start += (knob_getpos(x, x->x_arcstart) * range);
    pdgui_vmess(0, "crs sf sf", cv, "itemconfigure", x->x_tag_arc,
        "-start", start * -180.0 / M_PI,
        "-extent", (angle - start) * -179.99 / M_PI);
// set wiper
    int radius = (int)(x->x_size*x->x_zoom / 2.0);
    int x0 = text_xpix(&x->x_obj, x->x_glist);
    int y0 = text_ypix(&x->x_obj, x->x_glist);
    int xc = x0 + radius, yc = y0 + radius; // center coords
    if(x->x_square)
        radius = (radius * x->x_radius);
    radius += x->x_zoom;
    int xp = xc + rint(radius * cos(angle)); // circle point x coordinate
    int yp = yc + rint(radius * sin(angle)); // circle point x coordinate
    pdgui_vmess(0, "crs iiii", cv, "coords", x->x_tag_wiper, xc, yc, xp, yp);
}

//---------------------- DRAW STUFF ----------------------------//

static void knob_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2){
    t_knob *x = (t_knob *)z;
    *xp1 = text_xpix(&x->x_obj, glist);
    *yp1 = text_ypix(&x->x_obj, glist);
    *xp2 = text_xpix(&x->x_obj, glist) + x->x_size*x->x_zoom;
    *yp2 = text_ypix(&x->x_obj, glist) + x->x_size*x->x_zoom;
}

static void knob_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_knob *x = (t_knob *)z;
    x->x_obj.te_xpix += dx, x->x_obj.te_ypix += dy;
    dx *= x->x_zoom, dy *= x->x_zoom;
    pdgui_vmess(0, "crs ii", glist_getcanvas(glist), "move", x->x_tag_obj, dx, dy);
    canvas_fixlinesfor(glist, (t_text*)x);
}

static void knob_select(t_gobj *z, t_glist *glist, int sel){
    t_knob* x = (t_knob*)z;
//    t_handle *sh = (t_handle *)x->x_handle;
    t_canvas *cv = glist_getcanvas(glist);
    x->x_select = sel;
    if(x->x_select){
        pdgui_vmess(0, "crs rk", cv, "itemconfigure",
            x->x_tag_sel, "-outline", THISGUI->i_selectcolor);
        pdgui_vmess(0, "crs rk",  cv, "itemconfigure",
            x->x_tag_number, "-fill", THISGUI->i_selectcolor);
        pdgui_vmess(0, "crs rk",  cv, "itemconfigure",
            x->x_tag_wiper, "-fill", THISGUI->i_selectcolor);
        pdgui_vmess(0, "crs rk",  cv, "itemconfigure",
            x->x_tag_arc, "-fill", THISGUI->i_selectcolor);
        pdgui_vmess(0, "crs rk rk",  cv, "itemconfigure",
            x->x_tag_IO, "-outline", THISGUI->i_selectcolor,
            "-fill", THISGUI->i_selectcolor);
//        pdgui_vmess(0, "rr rk",  sh->h_pathname, "configure",
//            "-highlightcolor", THISGUI->i_selectcolor);
    }
    else{
        char fg[32];
        if(x->x_theme)
            snprintf(fg, sizeof(fg), "#%06x", THISGUI->i_foregroundcolor);
        else
            snprintf(fg, sizeof(fg), "%s", x->x_fg->s_name);
        pdgui_vmess(0, "crs rs", cv, "itemconfigure",
            x->x_tag_number, "-fill", fg);
        pdgui_vmess(0, "crs rs",  cv, "itemconfigure",
            x->x_tag_wiper, "-fill", fg);
        pdgui_vmess(0, "crs rs rs",  cv, "itemconfigure",
            x->x_tag_IO, "-outline", fg, "-fill", fg);
        pdgui_vmess(0, "crs rs",  cv, "itemconfigure",
            x->x_tag_arc, "-fill", fg);
        pdgui_vmess(0, "crs rk", cv, "itemconfigure",
            x->x_tag_sel, "-outline", THISGUI->i_foregroundcolor);
//        pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_base_circle,
//            "-outline", fg);
//        pdgui_vmess(0, "rr rk",  sh->h_pathname, "configure",
//            "-highlightcolor", THISGUI->i_foregroundcolor);
    }
}

static void knob_draw_handle(t_knob *x, int state){
    t_handle *sh = (t_handle *)x->x_handle;

    pdgui_vmess(0, "rs", "destroy", sh->h_pathname);

    if(state){
        pdgui_vmess(0, "rsdddddd",
            "canvas_new",
            sh->h_pathname,
            HANDLE_SIZE,
            HANDLE_SIZE,
            THISGUI->i_selectcolor,
            THISGUI->i_foregroundcolor,
            THISGUI->i_gopcolor,
            2*x->x_zoom);

        int x1, y1, x2, y2;
        knob_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);

        pdgui_vmess(0, "rcddddss",
            "canvas_create_window",
            x->x_cv, /* canvas pointer */
            x2 - HANDLE_SIZE*x->x_zoom + 1,
            y2 - HANDLE_SIZE*x->x_zoom + 1,
            HANDLE_SIZE*x->x_zoom,
            HANDLE_SIZE*x->x_zoom,
            sh->h_pathname,
            x->x_tag_handle,
            x->x_tag_obj);

        pdgui_vmess(0, "rss",
            "bind_click",
            sh->h_pathname,
            sh->h_bindsym->s_name);

        pdgui_vmess(0, "rss",
            "bind_release",
            sh->h_pathname,
            sh->h_bindsym->s_name);

        pdgui_vmess(0, "rss",
            "bind_motion",
            sh->h_pathname,
            sh->h_bindsym->s_name);

        pdgui_vmess(0, "rs", "focus", sh->h_pathname);
    }
}


// Draw ticks
static void knob_draw_ticks(t_knob *x){
    t_canvas *cv = glist_getcanvas(x->x_glist);
    char fg[32];
    if(x->x_theme)
        snprintf(fg, sizeof(fg), "#%06x", THISGUI->i_foregroundcolor);
    else
        snprintf(fg, sizeof(fg), "%s", x->x_fg->s_name);
    pdgui_vmess(0, "crs", cv, "delete", x->x_tag_ticks);
    if(!x->x_steps || !x->x_ticks)
        return;
    int z = x->x_zoom;
    int divs = x->x_steps;
    if((divs > 1) && ((x->x_angle_range + 360) % 360 != 0)) // ????
        divs = divs - 1;
    float delta_w = x->x_angle_range / (float)divs;
    int x0 = text_xpix(&x->x_obj, x->x_glist), y0 = text_ypix(&x->x_obj, x->x_glist);
    float half_size = (float)x->x_size*z / 2.0; // half_size
    int xc = x0 + half_size, yc = y0 + half_size; // center coords
    int start = x->x_arcstart_angle - 90.0;
    if(x->x_square)
        half_size *= x->x_radius;
    if(x->x_steps == 1){
        int width = (x->x_size / 40);
        if(width < 1)
            width = 1;
        width *= 1.5;
        double pos = knob_getpos(x, x->x_arcstart) * x->x_angle_range;
        float w = pos + start; // tick angle
        w *= M_PI/180.0; // in radians
        float dx = half_size * cos(w), dy = half_size * sin(w);
        int x1 = xc + (int)(dx);
        int y1 = yc + (int)(dy);
        int x2 = xc + (int)(dx * 0.65);
        int y2 = yc + (int)(dy * 0.65);
        char *tags_ticks[] = {x->x_tag_ticks, x->x_tag_fg, x->x_tag_obj};
        pdgui_vmess(0, "crr iiii ri rs rS",
            cv, "create", "line",
            x1, y1, x2, y2,
            "-width", width * z,
            "-fill", fg,
            "-tags", 3, tags_ticks);
    }
    else for(int t = 1; t <= x->x_steps; t++){
        int thicker = (t == 1 || t == x->x_steps);
        int width = (x->x_size / 40);
        if(width < 1)
            width = 1;
        if(thicker)
            width *= 1.5;
        float w = (t-1)*delta_w + start; // tick angle
        w *= M_PI/180.0; // in radians
        float dx = half_size * cos(w), dy = half_size * sin(w);
        int x1 = xc + (int)(dx);
        int y1 = yc + (int)(dy);
        int x2 = xc + (int)(dx * (thicker ? 0.65 : 0.75));
        int y2 = yc + (int)(dy * (thicker ? 0.65 : 0.75));
        char *tags_ticks[] = {x->x_tag_ticks, x->x_tag_obj};
        pdgui_vmess(0, "crr iiii ri rs rS",
            cv, "create", "line",
            x1, y1, x2, y2,
            "-width", width * z,
            "-fill", fg,
            "-tags", 2, tags_ticks);
    }
}

// draw all and initialize stuff
static void knob_draw_new(t_knob *x, t_glist *glist){
    int z = x->x_zoom;
    int x1 = text_xpix(&x->x_obj, glist);
    int y1 = text_ypix(&x->x_obj, glist);
    int x2 = x1 + x->x_size * z;
    int y2 = y1 + x->x_size * z;
    int circle_width = x->x_size * (x->x_square ? x->x_radius : 1);
    int offset = (x->x_size - circle_width) * 0.5;
    int half_size = x->x_size * z / 2; // half_size
    int xc = x1 + half_size, yc = y1 + half_size; // center coords
    int w_width = circle_width * z / 20; // wiper width is 1/20 of circle size
    int wx1 = rint(xc - w_width), wy1 = rint(yc - w_width);
    int wx2 = rint(xc + w_width), wy2 = rint(yc + w_width);
    int arcwidth = circle_width * z * 0.1; // arc width is 1/10 of circle
    if(arcwidth < 1)
        arcwidth = 1;
    float pos = x->x_pos;
    if(x->x_discrete){ // later just 1 tick case
        t_float steps = (x->x_steps < 2 ? 2 : (float)x->x_steps) -1;
        pos = rint(pos * steps) / steps;
    }
    float start = (x->x_arcstart_angle / 90.0 - 1) * HALF_PI;
    float range = x->x_angle_range / 180.0 * M_PI;
    float angle = start + pos * range; // pos angle
    // set wiper
    int radius = (int)(x->x_size*z / 2.0);
    if(x->x_square)
        radius = (radius * x->x_radius);
    radius += z;
    int xp = xc + rint(radius * cos(angle)); // circle point x coordinate
    int yp = yc + rint(radius * sin(angle)); // circle point x coordinate
    
    t_canvas *cv = glist_getcanvas(glist);
// square
    char *tags_square[] = {x->x_tag_square, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rS", cv, "create", "rectangle",
        x1, y1, x2, y2, "-tags", 2, tags_square);
// outline
    char *tags_outline[] = {x->x_tag_outline, x->x_tag_obj, x->x_tag_sel};
    pdgui_vmess(0, "crr iiii ri rS", cv, "create", "rectangle",
        x1, y1, x2, y2, "-width", z, "-tags", 3, tags_outline);
// base circle
    char *tags_circle[] = {x->x_tag_base_circle, x->x_tag_obj, x->x_tag_sel};
    pdgui_vmess(0, "crr iiii ri rS", cv, "create", "oval",
        x1 + offset, y1 + offset, x2 - offset, y2 - offset,
        "-width", z, "-tags", 2, tags_circle); // removed x->x_tag_sel
// arc
    char *tags_arc_bg[] = {x->x_tag_bg_arc, x->x_tag_obj};
        pdgui_vmess(0, "crr iiii rS", cv, "create", "arc",
        x1 + offset + z, y1 + offset + z,
        x2 - offset - z, y2 - offset - z,
        "-tags", 2, tags_arc_bg);
    char *tags_arc[] = {x->x_tag_arc, x->x_tag_obj};
        pdgui_vmess(0, "crr iiii rS", cv, "create", "arc",
        x1 + offset + z, y1 + offset + z,
        x2 - offset - z, y2 - offset - z,
        "-tags", 2, tags_arc);
// center circle on top of arc
    char *tags_center[] = {x->x_tag_center_circle, x->x_tag_obj};
        pdgui_vmess(0, "crr iiii rS", cv, "create", "oval",
        x1 + arcwidth + offset * z, y1 + arcwidth + offset * z,
        x2 - arcwidth - offset * z, y2 - arcwidth - offset * z,
        "-tags", 2, tags_center);
// wiper and wiper center
    char *tags_wiper_center[] = {x->x_tag_wpr_c, x->x_tag_obj};
        pdgui_vmess(0, "crr iiii rS", cv, "create", "oval",
        wx1, wy1, wx2, wy2,
        "-tags", 2, tags_wiper_center);
    char *tags_wiper[] = {x->x_tag_wiper, x->x_tag_fg, x->x_tag_obj};
        pdgui_vmess(0, "crr iiii ri rS", cv, "create", "line",
        xc, yc, xp, yp,
        "-width", w_width < z ? z : w_width,
        "-tags", 3, tags_wiper);
// number
    t_atom at[2];
    SETSYMBOL(at, gensym(def_font));
    SETFLOAT(at+1, -x->n_size*z);
    char *tags_number[] = {x->x_tag_number, x->x_tag_fg, x->x_tag_obj};
    pdgui_vmess(0, "crr ii rs rs rA rS", cv, "create", "text",
        x1 + x->x_xpos*x->x_zoom, y1 + x->x_ypos*x->x_zoom,
        "-text", knob_get_number(x),
        "-anchor", "w",
        "-font", 2, at,
        "-tags", 3, tags_number);
// inlet
    char *tags_in[] = {x->x_tag_in, x->x_tag_fg, x->x_tag_IO, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rs rs rS", cv, "create", "rectangle",
        x1, y1, x1 + IOWIDTH*z, y1 + IHEIGHT*z,
        "-outline", x->x_fg->s_name,
        "-fill", x->x_fg->s_name,
        "-tags", 4, tags_in);
// outlet
    char *tags_out[] = {x->x_tag_out, x->x_tag_fg, x->x_tag_IO, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rs rs rS", cv, "create", "rectangle",
        x1, y2 - OHEIGHT*z, x1 + IOWIDTH*z, y2,
        "-outline", x->x_fg->s_name,
        "-fill", x->x_fg->s_name,
        "-tags", 4, tags_out);
// hover area
    char *tags_hover[] = {x->x_tag_hover, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rs rs rS", cv, "create", "rectangle",
        x1, y1, x2, y2,
        "-outline", "", "-fill", "",
        "-tags", 2, tags_hover);
// config and set
    knob_draw_ticks(x);
//    knob_config_size(x);
    knob_config_io(x);
    knob_config_bg(x);
    knob_config_mg(x);
    knob_config_arc(x);
    knob_config_fg(x);
    knob_config_wcenter(x);
//    knob_config_number(x);
    knob_update(x);
    show_number(x, 1);
}

void knob_vis(t_gobj *z, t_glist *glist, int vis){
    t_knob* x = (t_knob*)z;
    t_canvas *cv = glist_getcanvas(glist);
    x->x_cv = glist_getcanvas(glist);
    t_handle *sh = (t_handle *)x->x_handle;
    if(x->x_edit)
        pdgui_vmess(0, "rs", "destroy", sh->h_pathname);
    if(vis){
        knob_draw_new(x, glist);
        char buf[256];
        snprintf(buf, sizeof(buf),
            ".x%lx.c bind %s <Enter> {pdsend [concat %s _mouse_enter \\;]}",
            (unsigned long)cv, x->x_tag_hover, x->x_bindname->s_name);
        pdgui_vmess(buf, NULL);
        snprintf(buf, sizeof(buf),
            ".x%lx.c bind %s <Leave> {pdsend [concat %s _mouse_leave \\;]}",
            (unsigned long)cv, x->x_tag_hover, x->x_bindname->s_name);
        pdgui_vmess(buf, NULL);
        // handle
        sprintf(sh->h_pathname, ".x%lx.h%lx",
            (unsigned long)cv, (unsigned long)sh);
        knob_draw_handle(x, x->x_edit);
    }
    else
        pdgui_vmess(0, "crs", cv, "delete", x->x_tag_obj);
}


static void knob_delete(t_gobj *z, t_glist *glist){
    canvas_deletelinesfor(glist, (t_text *)z);
}

static void knob_save(t_gobj *z, t_binbuf *b){
    t_knob *x = (t_knob *)z;
    binbuf_addv(b, "ssiis",
        gensym("#X"),
        gensym("obj"),
        (t_int)x->x_obj.te_xpix,
        (t_int)x->x_obj.te_ypix,
        atom_getsymbol(binbuf_getvec(x->x_obj.te_binbuf)));
    knob_get_snd(x);
    knob_get_var(x);
    knob_get_rcv(x);
    if(x->x_savestate)
        x->x_load = x->x_fval;
    binbuf_addv(b, "iffffsssssiiiiiiiifssiiiiiiiiii", // 31 args
        x->x_size, // 01: i SIZE
        (float)x->x_lower, // 02: f lower
        (float)x->x_upper, // 03: f upper
        x->x_log ? 1 : x->x_exp, // 04: f exp
        x->x_load, // 05: f load
        x->x_snd_raw, // 06: s snd
        x->x_rcv_raw, // 07: s rcv
        x->x_bg, // 08: s bgcolor
        x->x_mg, // 09: s arccolor
        x->x_fg, // 10: s fgcolor
        x->x_square, // 11: i square
        x->x_circular, // 12: i circular
        x->x_steps, // 13: i steps
        x->x_discrete, // 14: i discrete
        x->x_arc, // 15: i arc
        x->x_angle_range, // 16: i range
        x->x_angle_offset, // 17: i offset
        x->x_jump, // 18: i offset
        x->x_arcstart, // 19: f start
        x->x_param, // 20: s param
        x->x_var_raw, // 21: s var
        x->x_number_mode, // 22: i number
        x->n_size, // 23: i number
        x->x_xpos, // 24: i number
        x->x_ypos, // 25: i number
        x->x_savestate, // 26: i savestate
        x->x_lb, // 27: i loadbang
        x->x_ticks, // 28: i show ticks
        x->x_readonly, // 29: i read only
        x->x_theme, // 30: i color theme
        x->x_transparent); // 31: i transparent background
    binbuf_addv(b, ";");
}

// ------------------------ knob methods -----------------------------

static void knob_set(t_knob *x, t_floatarg f){
    if(isnan(f) || isinf(f))
        return;
    x->x_setted = 1;
    double old = x->x_pos;
    x->x_fval = x->x_circular == 2 ? f : knob_clipfloat(x, f);
    x->x_pos = knob_getpos(x, x->x_fval);
    if(x->x_pos != old){
        if(knob_vis_check(x))
            knob_update(x);
        knob_update_number(x);
    }
    
}

static void knob_ping(t_knob *x){
    if(x->x_cname->s_thing && x->x_snd_raw != gensym("empty")){
        t_atom at[1];
        SETSYMBOL(at, x->x_snd_raw);
        typedmess(x->x_cname->s_thing, x->x_cname, 1, at);
    }
}

static void knob_bang(t_knob *x){
    knob_get_snd(x);
    knob_ping(x);
    if(x->x_circular == 2 && !x->x_setted){ // infinite
        double delta = x->x_fval - x->x_lastval;
        double range = x->x_upper - x->x_lower;
        double halfrange = range * 0.5;
        if(fabs(delta) > halfrange){
            delta = fmod(delta + halfrange, range);
            if(delta < 0)
                delta += range;
            delta -= halfrange;
        }
        x->x_fval = x->x_lastval + delta;
    }
    if(x->x_param != gensym("empty")){
        t_atom at[1];
        SETFLOAT(at, x->x_fval);
        outlet_anything(x->x_obj.ob_outlet, x->x_param, 1, at);
        if(x->x_snd == gensym("empty") || x->x_snd == &s_)
            goto empty;
        if(x->x_snd->s_thing)
            typedmess(x->x_snd->s_thing, x->x_param, 1, at);
    }
    else{
        outlet_float(x->x_obj.ob_outlet, x->x_fval);
        if(x->x_snd == gensym("empty") || x->x_snd == &s_)
            goto empty;
        if(x->x_snd->s_thing){
            if(x->x_snd == x->x_rcv)
                pd_error(x, "[knob]: same send/receive names (infinite loop)");
            else
                pd_float(x->x_snd->s_thing, x->x_fval);
        }
    }
empty:
    if(x->x_var != gensym("empty"))
        value_setfloat(x->x_var, x->x_fval);
    x->x_lastval = x->x_fval;
    x->x_setted = 0;
}

static void knob_float(t_knob *x, t_floatarg f){
    if(isnan(f) || isinf(f))
        return;
    knob_set(x, f);
    knob_bang(x);
}

static void knob_load(t_knob *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(!ac)
        x->x_load = x->x_fval;
    else if(ac == 1 && av->a_type == A_FLOAT){
        float f = atom_getfloat(av);
        x->x_load = x->x_circular == 2 ? f : knob_clipfloat(x, f);
    }
}

static void knob_reload(t_knob *x){
    knob_float(x, x->x_load);
}

static void knob_arcstart(t_knob *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(!ac)
        x->x_arcstart = x->x_fval;
    else if(ac == 1 && av->a_type == A_FLOAT){
        float f = atom_getfloat(av);
        x->x_arcstart = knob_clipfloat(x, f);
    }
    else
        return;
    if(knob_vis_check(x)){
        knob_update(x);
        if(x->x_steps == 1)
            knob_draw_ticks(x);
    }
}

// set start/end angles
static void knob_start_end(t_knob *x, t_floatarg f1, t_floatarg f2){
    int range = f1 > 360 ? 360 : f1 < 0 ? 0 : (int)f1;
    int offset = f2 > 360 ? 360 : f2 < 0 ? 0 : (int)f2;
    if(x->x_angle_range == range && x->x_angle_offset == offset)
        return;
    x->x_angle_range = range;
    x->x_angle_offset = offset;
    int start = -(x->x_angle_range/2) + x->x_angle_offset;
    int end = x->x_angle_range/2 + x->x_angle_offset;
    float tmp;
    while(start < -360)
        start = -360;
    while (start > 360)
        start = 360;
    while(end < -360)
        end = -360;
    while(end > 360)
        end = 360;
    if(end < start){
        tmp = start;
        start = end;
        end = tmp;
    }
    if((end - start) > 360)
        end = start + 360;
    if(end == start)
        end = start + 1;
    x->x_arcstart_angle = start;
    x->x_end_angle = end;
    knob_set(x, x->x_fval);
    if(knob_vis_check(x)){
        knob_draw_ticks(x);
        knob_update(x);
    }
    x->x_setted = 0;
}

static void knob_angle(t_knob *x, t_floatarg f){
    knob_start_end(x, f, x->x_angle_offset);
}

static void knob_offset(t_knob *x, t_floatarg f){
    knob_start_end(x, x->x_angle_range, f);
}

static void knob_size(t_knob *x, t_floatarg f){
    float size = f < MIN_SIZE ? MIN_SIZE : f;
    if(x->x_size != size){
        x->x_size = size;
        if(knob_vis_check(x)){
            knob_config_size(x);
            knob_draw_ticks(x);
            knob_update(x);
            canvas_fixlinesfor(x->x_glist, (t_text *)x);
            knob_draw_handle(x, x->x_edit);
        }
    }
}

/*static void knob_proportion(t_knob *x, t_floatarg f){
    float r = (f < 50 ? 50 : f > 100 ? 100 : f) / 100;
    if(x->x_radius != r){
        x->x_radius = r;
        if(knob_vis_check(x))
            knob_config_size(x);
            knob_update(x);
    }
}*/

static void knob_arc(t_knob *x, t_floatarg f){
    int arc = f != 0;
    if(x->x_arc != arc){
        x->x_arc = arc;
        if(knob_vis_check(x))
            knob_config_arc(x);
            knob_config_mg(x);
    }
}

static void knob_circular(t_knob *x, t_floatarg f){
    x->x_circular = f < 0 ? 0 : f > 2 ? 2 : (int)f;
}

static void knob_discrete(t_knob *x, t_floatarg f){
    x->x_discrete = (f != 0);
}

static void knob_ticks(t_knob *x, t_floatarg f){
    int ticks = f != 0;
    if(ticks != x->x_ticks){
        x->x_ticks = ticks;
        if(knob_vis_check(x))
            knob_draw_ticks(x);
    }
}

static void knob_steps(t_knob *x, t_floatarg f){
    int steps = f < 0 ? 0 : (int)f;
    if(x->x_steps != steps){
        x->x_steps = steps;
        if(knob_vis_check(x))
            knob_draw_ticks(x);
    }
}

static t_symbol *knob_getcolor(int ac, t_atom *av){
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

static void knob_validate(t_knob *x, t_floatarg v, t_symbol *color, t_floatarg t){
    int valid = (int)v;
    int type = (int)t;
    switch (type) {
        case 0:
            if (valid){
                x->x_bg = color;
                knob_config_bg(x);
            }
            else
                pd_error(0, "[knob] invalid background color '%s'", color->s_name);
            break;

        case 1:
            if (valid){
                x->x_mg = color;
                knob_config_mg(x);
            }
            else
                pd_error(0, "[knob] invalid arc color '%s'", color->s_name);
            break;

        case 2:
            if (valid){
                x->x_fg = color;
                knob_config_fg(x);
            }
            else
                pd_error(0, "[knob] invalid background color '%s'", color->s_name);
            break;
    }
}

static void knob_send_color_validate(t_knob *x, const char *color, int which){
    char buf[512];
    snprintf(buf, sizeof(buf),
        "set color_name {%s}", color);
    pdgui_vmess(buf, NULL);
    pdgui_vmess(
        "set color_escaped [string map {{ } {\\ }} $color_name]",
        NULL);
    pdgui_vmess(
        "set hex_color [color2hex $color_name]",
        NULL);
    snprintf(buf, sizeof(buf),
        "if {$hex_color eq \"\"} "
        "{pdsend \"%s _validate 0 $color_escaped %d\"} "
        "else {pdsend \"%s _validate 1 $hex_color %d\"}",
        x->x_bindname->s_name, which,
        x->x_bindname->s_name, which);
    pdgui_vmess(buf, NULL);
}

static void knob_bgcolor(t_knob *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(!ac)
        return;
    knob_send_color_validate(x, knob_getcolor(ac, av)->s_name, 0);
}

static void knob_setbgcolor(t_knob *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    x->x_theme = 0;
    knob_bgcolor(x, NULL, ac, av);
}

static void knob_arccolor(t_knob *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(!ac)
        return;
    knob_send_color_validate(x, knob_getcolor(ac, av)->s_name, 1);
}

static void knob_setarccolor(t_knob *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    x->x_theme = 0;
    knob_arccolor(x, NULL, ac, av);
}

static void knob_fgcolor(t_knob *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(!ac)
        return;
    knob_send_color_validate(x, knob_getcolor(ac, av)->s_name, 2);
}

static void knob_setfgcolor(t_knob *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    x->x_theme = 0;
    knob_fgcolor(x, NULL, ac, av);
}

static void knob_colors(t_knob *x, t_symbol *bg, t_symbol *mg, t_symbol *fg){
    x->x_theme = 0;
    t_atom at[1];
    SETSYMBOL(at, bg);
    knob_bgcolor(x, NULL, 1, at);
    SETSYMBOL(at, mg);
    knob_arccolor(x, NULL, 1, at);
    SETSYMBOL(at, fg);
    knob_fgcolor(x, NULL, 1, at);
}

static void knob_param(t_knob *x, t_symbol *s){
    if(s == gensym("") || s == &s_)
        x->x_param = gensym("empty");
    else
        x->x_param = s;
}

static void knob_var(t_knob *x, t_symbol *s){
    if(s == gensym("") || s == &s_)
        s = gensym("empty");
    t_symbol *var = s == gensym("empty") ? &s_ : canvas_realizedollar(x->x_glist, s); // ????
    if(var != x->x_var){
        x->x_var_set = 1;
        x->x_var_raw = s;
        x->x_var = var;
    }
}

static void knob_send(t_knob *x, t_symbol *s){
    if(s == gensym(""))
        s = gensym("empty");
    t_symbol *snd = s == gensym("empty") ? &s_ : canvas_realizedollar(x->x_glist, s);
    if(snd != x->x_snd){
        x->x_snd_set = 1;
        x->x_snd_raw = s;
        x->x_snd = snd;
        knob_config_io(x);
    }
}

static void knob_receive(t_knob *x, t_symbol *s){
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
        knob_config_io(x);
    }
}

static void knob_range(t_knob *x, t_floatarg f1, t_floatarg f2){
    if(f1 == f2){
        pd_error(x, "[knob]: lower and upper values can't be the same");
        return;
    }
    x->x_lower = (double)f1;
    x->x_upper = (double)f2;
    if(x->x_circular != 2){
        x->x_fval = knob_clipfloat(x, x->x_fval);
        x->x_load = knob_clipfloat(x, x->x_load);
    }
    x->x_arcstart = knob_clipfloat(x, x->x_arcstart);
    if(x->x_lower == x->x_upper)
        x->x_fval = x->x_load = x->x_arcstart = x->x_lower;
    if(x->x_log){
        if((x->x_lower <= 0 && x->x_upper >= 0) || (x->x_lower >= 0 && x->x_upper <= 0))
            pd_error(x, "[knob]: range can't contain '0' in log mode");
    }
    x->x_pos = knob_getpos(x, x->x_fval);
    if(knob_vis_check(x))
        knob_update(x);
}

static void knob_log(t_knob *x, t_floatarg f){
    x->x_log = (f != 0);
    if(x->x_log)
        x->x_expmode = 1; // log
    else{
        if(x->x_exp == 0)
            x->x_expmode = 0; // lin
        else
            x->x_expmode = 2; // exp
    }
    x->x_pos = knob_getpos(x, x->x_fval);
    if(knob_vis_check(x))
        knob_update(x);
}

static void knob_jump(t_knob *x, t_floatarg f){
    x->x_jump = (f != 0);
}

static void knob_exp(t_knob *x, t_floatarg f){
    x->x_exp = f;
    if(fabs(x->x_exp) == 1)
        x->x_exp = 0; // lin
    if(x->x_log)
        x->x_expmode = 1; // log
    else{
        if(x->x_exp == 0)
            x->x_expmode = 0; // lin
        else
            x->x_expmode = 2; // exp
    }
    x->x_pos = knob_getpos(x, x->x_fval);
    if(knob_vis_check(x))
        knob_update(x);
}

static void knob_savestate(t_knob *x, t_floatarg f){
    x->x_savestate = f != 0;
}

static void knob_lb(t_knob *x, t_floatarg f){
    x->x_lb = f != 0;
}

static void knob_number_mode(t_knob *x, t_floatarg f){
    x->x_number_mode = f < 0 ? 0 : f > 4 ? 4 : (int)f;
    show_number(x, 0);
}

static void knob_nsize(t_knob *x, t_floatarg f){
    x->n_size = f < 8 ? 8 : (int)f;
    knob_config_number(x);
}

static void knob_numberpos(t_knob *x, t_floatarg xpos, t_floatarg ypos){
    x->x_xpos = (int)xpos, x->x_ypos = (int)ypos;
    knob_config_number(x);
}

static void knob_readonly(t_knob *x, t_floatarg f){
    x->x_readonly = (int)(f != 0);
}

static void knob_theme(t_knob *x, t_floatarg f){
    x->x_theme = (int)(f != 0);
    if(knob_vis_check(x)){
        knob_config_bg(x);
        knob_config_arc(x);
        knob_config_mg(x);
        knob_config_fg(x);
    }
}

static void knob_transparent(t_knob *x, t_floatarg f){
    x->x_transparent = (int)f;
    if(knob_vis_check(x)){
        knob_config_bg(x);
        knob_config_arc(x);
        knob_config_mg(x);
    }
}

static void knob_square(t_knob *x, t_floatarg f){
    int square = (int)f;
    if(x->x_square != square){
        x->x_square = (int)f;
        if(knob_vis_check(x)){
            knob_config_io(x);
            if(x->x_radius < 1.0){
                knob_config_size(x);
                knob_update(x);
            }
            knob_config_bg(x);
            knob_draw_ticks(x);
        }
    }
}

static void knob_zoom(t_knob *x, t_floatarg f){
    x->x_zoom = (int)f;
}

static t_symbol *get_nmode(t_knob *x){
    if(x->x_number_mode == 0)
        return(gensym("Never"));
    else if(x->x_number_mode == 1)
        return(gensym("Always"));
    else if(x->x_number_mode == 2)
        return(gensym("Active"));
    else if(x->x_number_mode == 3)
        return(gensym("Typing"));
    else // x->x_number_mode == 4
        return(gensym("Hovering"));
}

static t_symbol * get_cmode(t_knob *x){
    if(x->x_circular == 0)
        return(gensym("No"));
    else if(x->x_circular == 1)
        return(gensym("Loop"));
    else // x->x_circular == 2
        return(gensym("Infinite"));
}

// --------------- properties stuff --------------------
static void knob_properties(t_gobj *z, t_glist *owner){
    (void)owner;
    t_knob *x = (t_knob *)z;
    knob_get_rcv(x);
    knob_get_snd(x);
    knob_get_var(x);
    t_symbol *n_mode = get_nmode(x);
    t_symbol *c_mode = get_cmode(x);
    pdgui_stub_vnew(&x->x_obj.ob_pd, "knob_dialog", x,
        "ii if iif iii ii ffif iis siii ss ss sss ii",
        x->x_size, x->x_square, // ii
        x->x_arc, x->x_arcstart, // if
        x->x_lb, x->x_savestate, x->x_load, // iif
        x->x_discrete, x->x_ticks, x->x_steps, // iii
        x->x_angle_range, x->x_angle_offset, // ii
        x->x_lower, x->x_upper, x->x_expmode, x->x_exp, // ffif
        x->x_readonly, x->x_jump, c_mode->s_name, // iis
        n_mode->s_name, x->n_size, x->x_xpos, x->x_ypos, // siii
        x->x_rcv_raw->s_name, x->x_snd_raw->s_name, // ss
        x->x_param->s_name, x->x_var_raw->s_name, // ss
        x->x_bg->s_name, x->x_mg->s_name, x->x_fg->s_name, // sss
        x->x_theme, x->x_transparent); // ii
}

static void knob_apply(t_knob *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    t_atom undo[32];
    SETFLOAT(undo+0, x->x_size);
    SETFLOAT(undo+1, x->x_square);
    SETFLOAT(undo+2, x->x_arc);
    SETFLOAT(undo+3, x->x_arcstart);
    SETFLOAT(undo+4, x->x_lb);
    SETFLOAT(undo+5, x->x_savestate);
    SETFLOAT(undo+6, x->x_load);
    SETFLOAT(undo+7, x->x_discrete);
    SETFLOAT(undo+8, x->x_ticks);
    SETFLOAT(undo+9, x->x_steps);
    SETFLOAT(undo+10, x->x_angle_range);
    SETFLOAT(undo+11, x->x_angle_offset);
    SETFLOAT(undo+12, x->x_lower);
    SETFLOAT(undo+13, x->x_upper);
    SETFLOAT(undo+14, x->x_expmode);
    SETFLOAT(undo+15, x->x_log ? 1 : x->x_exp);
    SETFLOAT(undo+16, x->x_readonly);
    SETFLOAT(undo+17, x->x_jump);
    SETSYMBOL(undo+18, get_cmode(x));
    SETSYMBOL(undo+19, get_nmode(x));
    SETFLOAT(undo+20, x->n_size);
    SETFLOAT(undo+21, x->x_xpos);
    SETFLOAT(undo+22, x->x_ypos);
    SETSYMBOL(undo+23, x->x_rcv);
    SETSYMBOL(undo+24, x->x_snd);
    SETSYMBOL(undo+25, x->x_param);
    SETSYMBOL(undo+26, x->x_var);
    SETSYMBOL(undo+27, x->x_bg);
    SETSYMBOL(undo+28, x->x_mg);
    SETSYMBOL(undo+29, x->x_fg);
    SETFLOAT(undo+30, x->x_theme);
    SETFLOAT(undo+31, x->x_transparent);
    pd_undo_set_objectstate(x->x_glist, (t_pd*)x, gensym("dialog"), 32, undo, ac, av);
    int size = (int)atom_getintarg(0, ac, av);
    int square = atom_getintarg(1, ac, av);
    int arc = atom_getintarg(2, ac, av) != 0;
    float arcstart = atom_getfloatarg(3, ac, av);
    x->x_lb = atom_getintarg(4, ac, av);
    x->x_savestate = atom_getintarg(5, ac, av);
    double load = atom_getfloatarg(6, ac, av);
    x->x_discrete = atom_getintarg(7, ac, av);
    int ticks = atom_getintarg(8, ac, av);
    int steps = atom_getintarg(9, ac, av);
    int range = atom_getintarg(10, ac, av);
    int offset = atom_getintarg(11, ac, av);
    float min = atom_getfloatarg(12, ac, av);
    float max = atom_getfloatarg(13, ac, av);
    int expmode = atom_getintarg(14, ac, av);
    float exp = atom_getfloatarg(15, ac, av);
    x->x_readonly = atom_getintarg(16, ac, av);
    x->x_jump = atom_getintarg(17, ac, av);
    t_symbol *cmode = atom_getsymbolarg(18, ac, av);
    t_symbol *nmode = atom_getsymbolarg(19, ac, av);
    int nsize = atom_getintarg(20, ac, av);
    int xpos = atom_getintarg(21, ac, av);
    int ypos = atom_getintarg(22, ac, av);
    t_symbol* rcv = atom_getsymbolarg(23, ac, av);
    t_symbol *snd = atom_getsymbolarg(24, ac, av);
    t_symbol *param = atom_getsymbolarg(25, ac, av);
    t_symbol *var = atom_getsymbolarg(26, ac, av);
    t_symbol *bg = atom_getsymbolarg(27, ac, av);
    t_symbol *mg = atom_getsymbolarg(28, ac, av);
    t_symbol *fg = atom_getsymbolarg(29, ac, av);
    x->x_theme = atom_getintarg(30, ac, av);
    x->x_transparent = atom_getintarg(31, ac, av);
    knob_config_io(x); // for outline/square
    if(expmode == 0){
        knob_log(x, 0);
        knob_exp(x, 0.0f);
    }
    if(expmode == 1)
        knob_log(x, 1);
    if(expmode == 2){
        knob_log(x, 0);
        knob_exp(x, exp);
    }
    knob_range(x, min, max);
    knob_steps(x, steps);
    knob_ticks(x, ticks);
    t_atom at[1];
    SETSYMBOL(at, bg);
    knob_bgcolor(x, NULL, 1, at);
    SETSYMBOL(at, fg);
    knob_fgcolor(x, NULL, 1, at);
    SETSYMBOL(at, mg);
    knob_arccolor(x, NULL, 1, at);
    knob_start_end(x, (float)range, (float)offset);
    knob_arc(x, (float)arc);
    knob_size(x, (float)size);
    knob_send(x, snd);
    knob_receive(x, rcv);
    knob_square(x, square);
    if(x->x_load != load){
        SETFLOAT(at, load);
        knob_load(x, NULL, 1, at);
    }
    if(x->x_arcstart != arcstart){
        SETFLOAT(at, arcstart);
        knob_arcstart(x, NULL, 1, at);
    }
    knob_param(x, param);
    knob_var(x, var);
    knob_nsize(x, nsize);
    knob_numberpos(x, xpos, ypos);
    int n_mode = 0;
    if(nmode == gensym("Always"))
        n_mode = 1;
    else if(nmode == gensym("Active"))
        n_mode = 2;
    else if(nmode == gensym("Typing"))
        n_mode = 3;
    else if(nmode == gensym("Hovering"))
        n_mode = 4;
    x->x_circular = 0;
    if(cmode == gensym("Loop"))
        x->x_circular = 1;
    else if(cmode == gensym("Infinite"))
        x->x_circular = 2;
    knob_number_mode(x, n_mode);
    knob_config_fg(x); // rethink
    knob_config_mg(x);
    knob_config_arc(x);
    knob_config_bg(x);
    canvas_dirty(x->x_glist, 1);
}

// --------------- click + motion stuff --------------------

static void knob_activecheck(t_knob *x){
    show_number(x, 0);
    knob_config_wcenter(x);
    char namebuf[512];
    if(x->x_var != gensym("empty") && x->x_var != &s_){
        sprintf(namebuf, "%s-active", x->x_var->s_name);
        t_symbol *snd = gensym(namebuf);
        if(snd->s_thing)
            pd_float(snd->s_thing, x->x_clicked);
    }
    if(x->x_snd != gensym("empty") && x->x_snd != &s_){
        sprintf(namebuf, "%s-active", x->x_snd->s_name);
        t_symbol *snd = gensym(namebuf);
        if(snd->s_thing)
            pd_float(snd->s_thing, x->x_clicked);
    }
}

static int xm, ym;

static void knob_arrow_motion(t_knob *x, int dir){
    float old = x->x_pos, pos;
    if(x->x_discrete){
        t_float ticks = (x->x_steps < 2 ? 2 : (float)x->x_steps) - 1;
        old = rint(old * ticks) / ticks;
        pos = old + dir/ticks;
        if(x->x_circular){
            if(pos > 1)
                pos = 0;
            if(pos < 0)
                pos = 1;
        }
        else
            pos = pos > 1 ? 1 : pos < 0 ? 0 : pos;
    }
    else{
        float delta = dir;
        delta /= (float)(x->x_size) * x->x_zoom * 2; // 2 is 'sensitivity'
        if(x->x_shift)
            delta *= 0.01;
        pos = x->x_pos + delta;
        if(x->x_circular){
            if(pos > 1)
                pos -= 1;
            if(pos < 0)
                pos += 1;
        }
        else
            pos = pos > 1 ? 1 : pos < 0 ? 0 : pos;
    }
    x->x_pos = pos;
    x->x_fval = knob_getfval(x);
    if(x->x_lastval != x->x_fval){
        knob_bang(x);
        knob_update_number(x);
    }
    if(old != x->x_pos){
        if(knob_vis_check(x))
            knob_update(x);
    }
}

static void knob_up(t_knob *x){
    knob_arrow_motion(x, 1);
}

static void knob_down(t_knob *x){
    knob_arrow_motion(x, -1);
}

static void knob_shift(t_knob *x, t_floatarg f){
    knob_arrow_motion(x, (int)f);
}

static void knob_dowheel(t_knob *x, t_floatarg delta, t_floatarg h){
    if(x->x_clicked && !x->x_edit){
        #ifdef __APPLE__
        if(!h)
            delta = -delta;
        #else
            delta /= 120.0;
        #endif
        knob_arrow_motion(x, delta);
    }
}

void knob_mouse_hover(t_knob *x, int h){
    if(x->x_edit || x->x_readonly)
        return;
    char namebuf[512];
    if(x->x_var != gensym("empty") && x->x_var != &s_){
        sprintf(namebuf, "%s-hover", x->x_var->s_name);
        t_symbol *snd = gensym(namebuf);
        if(snd->s_thing)
            pd_float(snd->s_thing, h);
    }
    if(x->x_snd != gensym("empty") && x->x_snd != &s_){
        sprintf(namebuf, "%s-hover", x->x_snd->s_name);
        t_symbol *snd = gensym(namebuf);
        if(snd->s_thing)
            pd_float(snd->s_thing, h);
    }
    show_number(x, 0);
}

void knob_mouse_enter(t_knob *x){
    if(!x->x_edit)
        knob_mouse_hover(x, x->x_hover = 1);
}

void knob_mouse_leave(t_knob *x){
    if(!x->x_edit)
        knob_mouse_hover(x, x->x_hover = 0);
}

static void knob_doup(t_knob *x, t_floatarg up){
    (void)x;
    (void)up;
}
static void knob_getscreen(t_knob *x, t_floatarg f1,  t_floatarg f2){
    (void)x;
    (void)f1;
    (void)f2;
}
static void knob_dobang(t_knob *x, t_floatarg f1,  t_floatarg f2){
    (void)x;
    (void)f1;
    (void)f2;
}
static void knob_dozero(t_knob *x, t_floatarg f1,  t_floatarg f2){
    (void)x;
    (void)f1;
    (void)f2;
}

static void knob_motion(t_knob *x, t_floatarg dx, t_floatarg dy, t_floatarg up){
    if((dx == 0 && dy == 0) || up == 1)
        return;
    float old = x->x_pos, pos;
    if(x->x_circular){ // get pos based on mouse coords
        if(x->x_jump){
            xm += dx, ym += dy; // update coords
            int xc = text_xpix(&x->x_obj, x->x_glist) + x->x_size / 2; // center x
            int yc = text_ypix(&x->x_obj, x->x_glist) + x->x_size / 2; // center y
            float alphacenter = (x->x_end_angle + x->x_arcstart_angle) / 2; // arc center angle
            // should be stored
            float alpha = atan2(xm - xc, -ym + yc) * 180.0 / M_PI; // angle of mouse position
            float alphadif = alpha - alphacenter; // difference to arc center
            alphadif = ((int)((alphadif + 180.0 + 360.0) * 100.0) % 36000) * 0.01; // wrap
            pos = alphadif + (alphacenter - x->x_arcstart_angle - 180.0);
            pos /= x->x_angle_range; // normalized 0-1
        }
        else{
            int xc = text_xpix(&x->x_obj, x->x_glist) + x->x_size / 2; // center x
            int yc = text_ypix(&x->x_obj, x->x_glist) + x->x_size / 2; // center y
            // should be stored
//            float alphacenter = (x->x_end_angle + x->x_arcstart_angle) / 2; // arc center angle
            // should also be stored
            float lastalpha = atan2(xm - xc, -ym + yc) * 180.0 / M_PI; // last angle
            xm += dx, ym += dy; // update coords
            float alpha = atan2(xm - xc, -ym + yc) * 180.0 / M_PI; // current angle
            float alphadif = alpha - lastalpha; // difference to arc center
            x->x_start_angle += alphadif;
            pos = x->x_start_angle / x->x_angle_range;
            if(pos > 1)
                pos -= 1;
            if(pos < 0)
                pos += 1;
        }
    }
    else{ // normal/non circular mode
        float delta = -dy;
        if(fabs(dx) > fabs(dy))
            delta = dx;
        delta /= (float)(x->x_size) * x->x_zoom * 2; // "2" is 'sensitivity'
        if(x->x_shift)
            delta *= 0.01;
        pos = x->x_pos + delta;
        if(pos > 1)
            pos = 1;
        if(pos < 0)
            pos = 0;
    }
    x->x_pos = pos;
    x->x_fval = knob_getfval(x);
    if(x->x_lastval != x->x_fval){
        knob_bang(x);
        knob_update_number(x);
    }
    if(old != x->x_pos){
        if(knob_vis_check(x))
            knob_update(x);
    }
}

static void knob_list(t_knob *x, t_symbol *sym, int ac, t_atom *av){ // get key events
    if(!ac){
        knob_bang(x);
        return;
    }
    if(ac == 1 && av->a_type == A_FLOAT){
        knob_float(x, atom_getfloat(av));
        return;
    }
    if(ac != 2 || x->x_edit)
        return;
    int flag = (int)atom_getfloat(av); // 1 for press, 0 for release
    sym = atom_getsymbol(av+1); // get key name
    if(sym == gensym("Meta_L")){
        x->x_ctrl = flag;
        return;
    }
    if(!x->x_clicked)
        return;
    int dir = 0;
    if(flag && (sym == gensym("Up") || (sym == gensym("Right"))))
        dir = 1;
    else if(flag && (sym == gensym("Down") || (sym == gensym("Left"))))
        dir = -1;
    else
        return;
    knob_arrow_motion(x, dir);
}

static void knob_key(void *z, t_symbol *keysym, t_floatarg fkey){
    t_knob *x = z;
    x->x_ignore = keysym;
    char c = fkey, buf[3], namebuf[512];
    buf[1] = 0;
    if(c == 0 || c == '\e'){ // click out
        knob_update_number(x);
        x->x_clicked = x->x_typing = 0;
        knob_activecheck(x);
        return;
    }
    else if(c == '\t'){ // tab
        if(x->x_var != gensym("empty") && x->x_var != &s_){
            sprintf(namebuf, "%s-tab", x->x_var->s_name);
            t_symbol *snd = gensym(namebuf);
            if(snd->s_thing){
                pd_bang(snd->s_thing);
                return;
            }
        }
        if(x->x_snd != gensym("empty") && x->x_snd != &s_){
            sprintf(namebuf, "%s-tab", x->x_snd->s_name);
            t_symbol *snd = gensym(namebuf);
            if(snd->s_thing){
                pd_bang(snd->s_thing);
                return;
            }
        }
        return;
    }
    else if(((c == '\n') || (c == 13))){ // enter
        float value = x->x_buf[0] == 0 ? x->x_fval : atof(x->x_buf);
        if(x->x_var != gensym("empty") && x->x_var != &s_){
            sprintf(namebuf, "%s-enter", x->x_var->s_name); // ???
            t_symbol *snd = gensym(namebuf);
            if(snd->s_thing)
                pd_float(snd->s_thing, value);
            else
                knob_float(x, value);
        }
        if(x->x_snd != gensym("empty") && x->x_snd != &s_){
            sprintf(namebuf, "%s-enter", x->x_snd->s_name);
            t_symbol *snd = gensym(namebuf);
            if(snd->s_thing)
                pd_float(snd->s_thing, value);
            else
                knob_float(x, value);
        }
        else{ // no send
            knob_float(x, value);
        }
        x->x_buf[0] = 0;
        x->x_typing = 0;
        show_number(x, 0);
        return;
    }
    else if(((c >= '0') && (c <= '9')) || (c == '.') || (c == '-') ||
    (c == 'e') || (c == '+') || (c == 'E')){ // number characters
        x->x_typing = 1;
        if(strlen(x->x_buf) < (MAX_NUMBOX_LEN-2)){
            buf[0] = c;
            strcat(x->x_buf, buf);
            goto update_number;
        }
    }
    else if((c == '\b') || (c == 127)){ // backspace / delete
        int sl = (int)strlen(x->x_buf) - 1;
        if(sl < 0)
            sl = 0;
        x->x_buf[sl] = 0;
        goto update_number;
    }
update_number:{
        show_number(x, 0);
        char *cp = x->x_buf;
        int sl = (int)strlen(x->x_buf);
        x->x_buf[sl] = '|';
        x->x_buf[sl+1] = 0;
        int numwidth = 6;
        if(sl >= (numwidth + 1))
            cp += sl - numwidth + 1;
        pdgui_vmess(0, "crs rs",glist_getcanvas(x->x_glist),
            "itemconfigure", x->x_tag_number, "-text", cp);
        x->x_buf[sl] = 0;
    
        if(x->x_var != gensym("empty") && x->x_var != &s_){
            sprintf(namebuf, "%s-typing", x->x_var->s_name);
            t_symbol *snd = gensym(namebuf);
            if(snd->s_thing)
                pd_symbol(snd->s_thing, gensym(cp));
        }
        if(x->x_snd != gensym("empty") && x->x_snd != &s_){
            sprintf(namebuf, "%s-typing", x->x_snd->s_name);
            t_symbol *snd = gensym(namebuf);
            if(snd->s_thing)
            pd_symbol(snd->s_thing, gensym(cp));
        }
    }
}
    
static void knob_active(t_knob *x, t_floatarg f){
    x->x_clicked = (int)(f != 0);
    knob_activecheck(x);
    if(x->x_clicked){
        knob_bang(x);
        glist_grab(x->x_glist, &x->x_obj.te_g, 0, knob_key, 0, 0);
    }
    else
        x->x_typing = 0;
}

static void knob_reset(t_knob *x){
    knob_set(x, x->x_arcstart);
    knob_bang(x);
}

static void knob_learn(t_knob *x){
    char namebuf[512];
    if(x->x_var != gensym("empty") && x->x_var != &s_){
        sprintf(namebuf, "%s-learn", x->x_var->s_name);
        t_symbol *snd = gensym(namebuf);
        if(snd->s_thing)
            pd_bang(snd->s_thing);
    }
    if(x->x_snd != gensym("empty") && x->x_snd != &s_){
        sprintf(namebuf, "%s-learn", x->x_snd->s_name);
        t_symbol *snd = gensym(namebuf);
        if(snd->s_thing)
            pd_bang(snd->s_thing);
    }
}

static void knob_forget(t_knob *x){
    char namebuf[512];
    if(x->x_var != gensym("empty") && x->x_var != &s_){
        sprintf(namebuf, "%s-forget", x->x_var->s_name);
        t_symbol *snd = gensym(namebuf);
        if(snd->s_thing)
            pd_bang(snd->s_thing);
    }
    if(x->x_snd != gensym("empty") && x->x_snd != &s_){
        sprintf(namebuf, "%s-forget", x->x_snd->s_name);
        t_symbol *snd = gensym(namebuf);
        if(snd->s_thing)
            pd_bang(snd->s_thing);
    }
}

static int knob_click(t_gobj *z, struct _glist *glist, int xpix, int ypix, int shift, int alt, int dbl, int doit){
    t_knob *x = (t_knob *)z;
    if(x->x_readonly)
        return(0);
    if(alt && doit){
        knob_float(x, x->x_load);
        return(1);
    }
    x->x_shift = shift;
    if(x->x_ctrl && doit){
        if(x->x_shift)
            knob_forget(x);
        else
            knob_learn(x);
        return(1);
    }
    else if(dbl){
        knob_reset(x);
        return(1);
    }
    if(doit){
        x->x_buf[0] = 0;
        x->x_clicked = 1;
        knob_activecheck(x);
        if(x->x_jump){
            int xc = text_xpix(&x->x_obj, x->x_glist) + x->x_size / 2;
            int yc = text_ypix(&x->x_obj, x->x_glist) + x->x_size / 2;
            float alphacenter = (x->x_end_angle + x->x_arcstart_angle) / 2;
            float alpha = atan2(xpix - xc, -ypix + yc) * 180.0 / M_PI;
            float pos = (((int)((alpha - alphacenter + 180.0 + 360.0) * 100.0) % 36000) * 0.01
                + (alphacenter - x->x_arcstart_angle - 180.0)) / x->x_angle_range;
            x->x_pos = pos > 1 ? 1 : pos < 0 ? 0 : pos;
            x->x_fval = knob_getfval(x);
            knob_update(x);
            if(x->x_circular){
                xm = xpix;
                ym = ypix;
            }
        }
        else if(x->x_circular){
            x->x_start_angle = (x->x_drag_start_pos = x->x_pos) * x->x_angle_range;
            xm = xpix;
            ym = ypix;
        }
        knob_bang(x);
        glist_grab(glist, &x->x_obj.te_g, (t_glistmotionfn)(knob_motion), knob_key, xpix, ypix);
        return(1);
    }
    return(1);
}

// --------------------- handle ---------------------------------------------------
static void handle__click_callback(t_handle *sh, t_floatarg f){
    int click = (int)f;
    t_knob *x = sh->h_master;
    if(sh->h_dragon && click == 0){
        
        char buf[128];
        snprintf(buf, sizeof(buf),
            ".x%lx.c delete %s",
            (unsigned long)x->x_cv, sh->h_outlinetag);
        pdgui_vmess(buf, NULL);
    
        t_atom undo[1];
        SETFLOAT(undo, x->x_size);
        t_atom redo[1];
        int size = (int)((x->x_size+sh->h_drag_delta)/x->x_zoom);
        SETFLOAT(redo, size);
        pd_undo_set_objectstate(x->x_glist, (t_pd*)x, gensym("size"), 1, undo, 1, redo);
        knob_size(x, size);
        knob_draw_handle(x, 1);
        knob_select((t_gobj *)x, x->x_glist, x->x_select);
        canvas_dirty(x->x_cv, 1);
    }
    else if(!sh->h_dragon && click){
        int x1, y1, x2, y2;
        knob_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        t_canvas *cv = (t_canvas *)x->x_cv;
        pdgui_vmess(0, "crs riii ri rs",
            cv, "create", "rectangle",
            x1, y1, x2, y2,
            "-outline", THISGUI->i_selectcolor,
            "-width", KNOB_SELBDWIDTH * x->x_zoom,
            "-tags", sh->h_outlinetag);
        pdgui_vmess(0, "rr rk",  sh->h_pathname, "configure",
            "-highlightcolor", THISGUI->i_selectcolor);
//        sh->h_dragx = sh->h_dragy = sh->h_drag_delta = 0;
        sh->h_drag_delta = 0;
    }
    sh->h_dragon = click;
}

static void handle__motion_callback(t_handle *sh, t_floatarg f1, t_floatarg f2){
    if(sh->h_dragon){
        t_knob *x = sh->h_master;
        int dx = (int)f1 - HANDLE_SIZE, dy = (int)f2 - HANDLE_SIZE;
        int x1, y1, x2, y2;
        knob_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        // average both movements
        int delta = (dx + dy) / 2;
        int newx = x2 + delta;
        int newy = y2 + delta;
        if(newx < x1 + MIN_SIZE * x->x_zoom)
            newx = x1 + MIN_SIZE * x->x_zoom;
        if(newy < y1 + MIN_SIZE * x->x_zoom)
            newy = y1 + MIN_SIZE * x->x_zoom;
        char buf[128];
        snprintf(buf, sizeof(buf),
            ".x%lx.c coords %s %d %d %d %d",
            (unsigned long)x->x_cv, sh->h_outlinetag, x1, y1, newx, newy);
        pdgui_vmess(buf, NULL);
        // this breaks... we need to change the strategy altogether...
        // sh->h_dragx = dx, sh->h_dragy = dy; 
        sh->h_drag_delta = delta;
    }
}


//-------------------- EDIT ---------------------------------

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
            knob_draw_handle(p->p_cnv, edit);
            knob_config_io(p->p_cnv);
            show_number(p->p_cnv, 0);
        }
    }
}

// ---------------------- new / free / setup ----------------------

static void knob_loadbang(t_knob *x, t_float f){
    if((int)f == LB_LOAD && x->x_lb)
        knob_bang(x);
}

static void edit_proxy_free(t_edit_proxy *p){
    pd_unbind(&p->p_obj.ob_pd, p->p_sym);
    clock_free(p->p_clock);
    pd_free(&p->p_obj.ob_pd);
}

static t_edit_proxy *edit_proxy_new(t_knob *x, t_symbol *s){
    t_edit_proxy *p = (t_edit_proxy*)pd_new(edit_proxy_class);
    p->p_cnv = x;
    pd_bind(&p->p_obj.ob_pd, p->p_sym = s);
    p->p_clock = clock_new(p, (t_method)edit_proxy_free);
    return(p);
}

static void knob_free(t_knob *x){
    pd_unbind((t_pd *)x, gensym("#keyname"));
    if(x->x_rcv != gensym("empty") && x->x_rcv != &s_) // ???
        pd_unbind(&x->x_obj.ob_pd, x->x_rcv);
    pd_unbind(&x->x_obj.ob_pd, x->x_bindname);
    if(x->x_handle){
        pd_unbind(x->x_handle, ((t_handle *)x->x_handle)->h_bindsym);
        pd_free(x->x_handle);
    }
    x->x_proxy->p_cnv = NULL;
    pdgui_stub_deleteforkey(x);
#ifndef PDINSTANCE
    mouse_gui_stoppolling((t_pd *)x);
    mouse_gui_unbindmouse((t_pd *)x);
#endif
}

static void *knob_new(t_symbol *s, int ac, t_atom *av){
    t_knob *x = (t_knob *)pd_new(knob_class);
    x->x_ignore = s;
// handle
   x->x_handle = pd_new(handle_class);
    t_handle *sh = (t_handle *)x->x_handle;
    sh->h_master = x;
    char hbuf[64];
    sprintf(hbuf, "_h%lx", (unsigned long)sh);
    pd_bind(x->x_handle, sh->h_bindsym = gensym(hbuf));
    sprintf(sh->h_outlinetag, "h%lx", (unsigned long)sh);
//
    x->x_buf[0] = 0;
    float loadvalue = 0.0, arcstart = 0.0, exp = 0.0, min = 0.0, max = 127.0;
    x->n_size = 12, x->x_xpos = 0, x->x_ypos = -15;
    t_symbol *snd = gensym("empty"), *rcv = gensym("empty");
    t_symbol *param = gensym("empty"), *var = gensym("empty");
    int size = 50, steps = 0, discrete = 0;
    x->x_circular = 0;
    int arc = 1, angle = 320, offset = 0;
    x->x_bg = gensym("#dfdfdf"), x->x_mg = gensym("#7c7c7c"), x->x_fg = gensym("black");
    x->x_clicked = x->x_log = x->x_jump = x->x_number_mode = x->x_savestate = 0;
    x->x_square = x->x_lb = 1;
    x->x_glist = (t_glist *)canvas_getcurrent();
    x->x_cv = canvas_getcurrent();
//    post("init x->x_cv = canvas_getcurrent() = %d", x->x_cv);
    x->x_cv = glist_getcanvas(x->x_glist);
//    post("init x->x_cv = glist_getcanvas(x->x_glist) = %d", x->x_cv);
    x->x_zoom = x->x_glist->gl_zoom;
    x->x_flag = x->x_readonly = x->x_transparent = 0;
    x->x_theme = 1;
    x->x_var_set = x->x_v_flag = 0;
    x->x_snd_set = x->x_s_flag = 0;
    x->x_rcv_set = x->x_r_flag = 0;
    if(ac){
        if(av->a_type == A_FLOAT){
            size = atom_getintarg(0, ac, av); // 01: i SIZE
            min = atom_getfloatarg(1, ac, av); // 02: f min
            max = atom_getfloatarg(2, ac, av); // 03: f max
            exp = atom_getfloatarg(3, ac, av); // 04: f exp
            loadvalue = atom_getfloatarg(4, ac, av); // 05: f load
            snd = atom_getsymbolarg(5, ac, av); // 06: s snd
            rcv = atom_getsymbolarg(6, ac, av); // 07: s rcv
            x->x_bg = atom_getsymbolarg(7, ac, av); // 08: s bgcolor
            x->x_mg = atom_getsymbolarg(8, ac, av); // 09: s arccolorlor
            x->x_fg = atom_getsymbolarg(9, ac, av); // 10: s fgcolor
            x->x_square = atom_getintarg(10, ac, av); // 11: i square
            x->x_circular = atom_getintarg(11, ac, av); // 12: i circular
            steps = atom_getintarg(12, ac, av); // 13: i steps
            discrete = atom_getintarg(13, ac, av); // 14: i discrete
            arc = atom_getintarg(14, ac, av); // 15: i arc
            angle = atom_getintarg(15, ac, av); // 16: i range
            offset = atom_getintarg(16, ac, av); // 17: i offset
            x->x_jump = atom_getintarg(17, ac, av); // 18: i jump
            arcstart = atom_getfloatarg(18, ac, av); // 19: f start value
            param = atom_getsymbolarg(19, ac, av); // 20: s param
            var = atom_getsymbolarg(20, ac, av); // 21: s var
            x->x_number_mode = atom_getintarg(21, ac, av); // 22: f number
            x->n_size = atom_getintarg(22, ac, av); // 23: f number
            x->x_xpos = atom_getintarg(23, ac, av); // 24: f number xpos
            x->x_ypos = atom_getintarg(24, ac, av); // 25: f number ypos
            x->x_savestate = atom_getintarg(25, ac, av); // 26: f savestate
            x->x_lb = atom_getintarg(26, ac, av); // 27: f loadbang
            x->x_ticks = atom_getintarg(27, ac, av); // 28: f show ticks
            x->x_readonly = atom_getintarg(28, ac, av); // 29: read only
            x->x_theme = atom_getintarg(29, ac, av); // 30: color theme
            x->x_transparent = atom_getintarg(30, ac, av); // 31: transparent
        }
        else{
            while(ac){
                t_symbol *sym = atom_getsymbol(av);
                if(sym == gensym("-size")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_FLOAT){
                            size = atom_getint(av);
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-range")){
                    if(ac >= 3){
                        x->x_flag = 1, av++, ac--;
                        min = atom_getfloat(av);
                        av++, ac--;
                        max = atom_getfloat(av);
                        av++, ac--;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-exp")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_FLOAT){
                            exp = atom_getfloat(av);
                            if(fabs(exp) == 1)
                                exp = 0;
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-log")){
                    x->x_flag = 1, av++, ac--;
                    exp = 1;
                }
                else if(sym == gensym("-nosquare")){
                    x->x_flag = 1, av++, ac--;
                    x->x_square = 0;
                }
                else if(sym == gensym("-readonly")){
                    x->x_flag = 1, av++, ac--;
                    x->x_readonly = 1;
                }
                else if(sym == gensym("-notheme")){
                    x->x_flag = 1, av++, ac--;
                    x->x_theme = 0;
                }
                else if(sym == gensym("-transparent")){
                    x->x_flag = 1, av++, ac--;
                    x->x_transparent = 1;
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
                else if(sym == gensym("-bgcolor")){
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
                else if(sym == gensym("-arccolor")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_SYMBOL){
                            x->x_mg = atom_getsymbol(av);
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-fgcolor")){
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
                else if(sym == gensym("-load")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_FLOAT){
                            loadvalue = atom_getfloat(av);
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-arcstart")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_FLOAT){
                            arcstart = atom_getfloat(av);
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-circular")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_FLOAT){
                            float f = atom_getfloat(av);
                            x->x_circular = f < 0 ? 0 : f > 2 ? 2 : (int)f;
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-jump")){
                    x->x_flag = 1, av++, ac--;
                    x->x_jump = 1;
                }
                else if(sym == gensym("-savestate")){
                    x->x_flag = 1, av++, ac--;
                    x->x_savestate = 1;
                }
                else if(sym == gensym("-noloadbang")){
                    x->x_flag = 1, av++, ac--;
                    x->x_lb = 0;
                }
                else if(sym == gensym("-number")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_FLOAT){
                            int n = atom_getint(av);
                            x->x_number_mode = n < 0 ? 0 : n > 4 ? 4 : n;
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-numbersize")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_FLOAT){
                            int n = atom_getint(av);
                            x->n_size = n < 8 ? 8 : n;
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-numberpos")){
                    if(ac >= 3){
                        x->x_flag = 1, av++, ac--;
                        x->x_xpos = atom_getint(av);
                        av++, ac--;
                        x->x_ypos = atom_getint(av);
                        av++, ac--;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-steps")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_FLOAT){
                            steps = atom_getint(av);
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-discrete")){
                    if(ac >= 1){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_FLOAT)
                            discrete = 1;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-arc")){
                    if(ac >= 1){
                        x->x_flag = 1, av++, ac--;
                        arc = 1;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-angle")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_FLOAT){
                            angle = atom_getint(av);
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-offset")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_FLOAT){
                            offset = atom_getint(av);
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else
                    goto errstate;
            }
        }
    }
    knob_param(x, param);
    x->x_snd = canvas_realizedollar(x->x_glist, x->x_snd_raw = snd);
    x->x_var = canvas_realizedollar(x->x_glist, x->x_var_raw = var);
    x->x_rcv = canvas_realizedollar(x->x_glist, x->x_rcv_raw = rcv);
    x->x_size = size < MIN_SIZE ? MIN_SIZE : size;
    knob_range(x, min, max);
    if(exp == 1)
        knob_log(x, 1);
    else
        knob_exp(x, exp);
    x->x_arcstart = arcstart;
    x->x_steps = steps < 0 ? 0 : steps;
    x->x_discrete = discrete;
    x->x_arc = arc;
    x->x_angle_range = angle < 0 ? 0 : angle > 360 ? 360 : angle;
    x->x_angle_offset = offset < 0 ? 0 : offset > 360 ? 360 : offset;
    x->x_arcstart_angle = -(x->x_angle_range/2) + x->x_angle_offset;
    x->x_end_angle = x->x_angle_range/2 + x->x_angle_offset;
    knob_set(x, x->x_fval = x->x_load = loadvalue);
    x->x_edit = x->x_glist->gl_edit;
    x->x_radius = 0.85;
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING-1, ".x%lx", (unsigned long)x->x_glist);
    buf[MAXPDSTRING-1] = 0;
    x->x_proxy = edit_proxy_new(x, gensym(buf));
    sprintf(buf, "#%lx", (long)x);
    pd_bind(&x->x_obj.ob_pd, x->x_bindname = gensym(buf));
    sprintf(x->x_tag_obj, "%pOBJ", x);
    sprintf(x->x_tag_fg, "%pFG", x);
    sprintf(x->x_tag_handle, "%pHANDLE", x);
    sprintf(x->x_tag_base_circle, "%pBASE_CIRCLE", x);
    sprintf(x->x_tag_sel, "%pSEL", x);
    sprintf(x->x_tag_arc, "%pARC", x);
    sprintf(x->x_tag_bg_arc, "%pBGARC", x);
    sprintf(x->x_tag_ticks, "%pTICKS", x);
    sprintf(x->x_tag_wiper, "%pWIPER", x);
    sprintf(x->x_tag_wpr_c, "%pWIPERC", x);
    sprintf(x->x_tag_center_circle, "%pCENTER_CIRCLE", x);
    sprintf(x->x_tag_outline, "%pOUTLINE", x);
    sprintf(x->x_tag_square, "%pSQUARE", x);
    sprintf(x->x_tag_in, "%pIN", x);
    sprintf(x->x_tag_out, "%pOUT", x);
    sprintf(x->x_tag_IO, "%pIO", x);
    sprintf(x->x_tag_number, "%pNUM", x);
    sprintf(x->x_tag_hover, "%pHOVER", x);
    if(x->x_rcv != gensym("empty") && x->x_rcv != &s_)
        pd_bind(&x->x_obj.ob_pd, x->x_rcv);
    pd_bind(&x->x_obj.ob_pd, gensym("#keyname")); // listen to key events
    get_cname(x, 100);
#ifndef PDINSTANCE
    mouse_gui_bindmouse((t_pd *)x);
    mouse_gui_willpoll();
//    mouse_reset(x);
//    mouse_updatepos(x);
    mouse_gui_startpolling((t_pd *)x, 1);
/*#else
    x->x_hzero = x->x_vzero = 0;
    mouse_updatepos(x);*/
#endif
//    pd_bind(&x->x_obj.ob_pd, gensym("#mouse_mouse")); // listen to mouse events
    outlet_new(&x->x_obj, &s_float);
    return(x);
errstate:
    pd_error(x, "[knob]: improper creation arguments");
    return(NULL);
}

void knob_setup(void){
    knob_class = class_new(gensym("knob"), (t_newmethod)(void*)knob_new,
        (t_method)knob_free, sizeof(t_knob), 0, A_GIMME, 0);
    class_addbang(knob_class,knob_bang);
    class_addfloat(knob_class, knob_float);
    class_addlist(knob_class, knob_list); // used for float and keypresses
    class_addmethod(knob_class, (t_method)knob_load, gensym("load"), A_GIMME, 0);
    class_addmethod(knob_class, (t_method)knob_arcstart, gensym("arcstart"), A_GIMME, 0);
    class_addmethod(knob_class, (t_method)knob_set, gensym("set"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_size, gensym("size"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_circular, gensym("circular"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_up, gensym("inc"), 0);
    class_addmethod(knob_class, (t_method)knob_down, gensym("dec"), 0);
    class_addmethod(knob_class, (t_method)knob_shift, gensym("shift"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_range, gensym("range"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_jump, gensym("jump"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_exp, gensym("exp"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_log, gensym("log"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_discrete, gensym("discrete"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_reload, gensym("reload"), 0);
//    class_addmethod(knob_class, (t_method)knob_proportion, gensym("proportion"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_setbgcolor, gensym("bgcolor"), A_GIMME, 0);
    class_addmethod(knob_class, (t_method)knob_setarccolor, gensym("arccolor"), A_GIMME, 0);
    class_addmethod(knob_class, (t_method)knob_setfgcolor, gensym("fgcolor"), A_GIMME, 0);
    class_addmethod(knob_class, (t_method)knob_colors, gensym("colors"), A_SYMBOL, A_SYMBOL, A_SYMBOL, 0);
    class_addmethod(knob_class, (t_method)knob_send, gensym("send"), A_DEFSYM, 0);
    class_addmethod(knob_class, (t_method)knob_param, gensym("param"), A_DEFSYM, 0);
    class_addmethod(knob_class, (t_method)knob_var, gensym("var"), A_DEFSYM, 0);
    class_addmethod(knob_class, (t_method)knob_receive, gensym("receive"), A_DEFSYM, 0);
    class_addmethod(knob_class, (t_method)knob_arc, gensym("arc"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_angle, gensym("angle"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_offset, gensym("offset"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_steps, gensym("steps"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_ticks, gensym("ticks"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_motion, gensym("motion"), A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_square, gensym("square"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_readonly, gensym("readonly"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_number_mode, gensym("number"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_savestate, gensym("savestate"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_lb, gensym("lb"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_nsize, gensym("numbersize"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_numberpos, gensym("numberpos"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_active, gensym("active"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_theme, gensym("theme"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_transparent, gensym("transparent"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_reset, gensym("reset"), 0);
    class_addmethod(knob_class, (t_method)knob_learn, gensym("learn"), 0);
    class_addmethod(knob_class, (t_method)knob_forget, gensym("forget"), 0);
    class_addmethod(knob_class, (t_method)knob_validate, gensym("_validate"),
        A_FLOAT, A_SYMBOL, A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_mouse_enter, gensym("_mouse_enter"), 0);
    class_addmethod(knob_class, (t_method)knob_mouse_leave, gensym("_mouse_leave"), 0);
    class_addmethod(knob_class, (t_method)knob_dowheel, gensym("_wheel"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_doup, gensym("_up"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_getscreen, gensym("_getscreen"),
        A_FLOAT, A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_dobang, gensym("_bang"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_dozero, gensym("_zero"), A_FLOAT, A_FLOAT, 0);
    handle_class = class_new(gensym("_handle"), 0, 0, sizeof(t_handle), CLASS_PD, 0);
    class_addmethod(handle_class, (t_method)handle__click_callback, gensym("_click"), A_FLOAT, 0);
    class_addmethod(handle_class, (t_method)handle__motion_callback, gensym("_motion"),
        A_FLOAT, A_FLOAT, 0);
    edit_proxy_class = class_new(0, 0, 0, sizeof(t_edit_proxy), CLASS_NOINLET | CLASS_PD, 0);
    class_addanything(edit_proxy_class, edit_proxy_any);
    knob_widgetbehavior.w_getrectfn  = knob_getrect;
    knob_widgetbehavior.w_displacefn = knob_displace;
    knob_widgetbehavior.w_selectfn   = knob_select;
    knob_widgetbehavior.w_activatefn = NULL;
    knob_widgetbehavior.w_deletefn   = knob_delete;
    knob_widgetbehavior.w_visfn      = knob_vis;
    knob_widgetbehavior.w_clickfn    = knob_click;
    class_setwidget(knob_class, &knob_widgetbehavior);
    class_setsavefn(knob_class, knob_save);
    class_setpropertiesfn(knob_class, knob_properties);
    class_addmethod(knob_class, (t_method)knob_apply, gensym("dialog"), A_GIMME, 0);
    class_addmethod(knob_class, (t_method)knob_loadbang, gensym("loadbang"), A_DEFFLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_zoom, gensym("zoom"), A_CANT, 0);
    #include "../Extra/knob_dialog.h" // knob properties in tcl/tk
    pdgui_vmess(knob_dialog_tcl, NULL);
}
