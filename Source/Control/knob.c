// By Porres and Tim Schoen
// Primarily based on the knob proposal for vanilla by Ant1, Porres and others

#include <m_pd.h>
#include <g_canvas.h>
#include <math.h>
#include <string.h>
#include <stdlib.h>

#define NAN_V   0x7FFFFFFFul
#define POS_INF 0x7F800000ul
#define NEG_INF 0xFF800000ul

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

#define MAX_NUMBOX_LEN 32

#define HALF_PI (M_PI / 2)

#define MIN_SIZE 16

#if __APPLE__
char def_font[100] = "Menlo";
#else
char def_font[100] = "DejaVu Sans Mono";
#endif

t_widgetbehavior knob_widgetbehavior;
static t_class *knob_class, *edit_proxy_class;

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
    t_float         x_start;        // arc start value
    t_float         x_radius;   
    int             x_start_angle;
    int             x_fill_bg;
    int             x_end_angle;
    int             x_range;
    int             x_offset;
    int             x_steps;
    int             x_square;
    double          x_lower;
    double          x_upper;
    int             x_clicked;
    int             x_typing;
    int             x_shownumber;
    int             x_showticks;
    int             x_fontsize;
    int             x_xpos;
    int             x_ypos;
    int             x_sel;
    int             x_shift;
    int             x_edit;
    int             x_jump;
    double          x_fval;
    t_symbol       *x_fg;
    t_symbol       *x_mg;
    t_symbol       *x_bg;
    t_symbol       *x_param;
    t_symbol       *x_var;
    t_symbol       *x_var_raw;
    int            x_var_set;
    int            x_savestate;
    int            x_loadbang;
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
    int             x_number;
    int             x_zoom;
    int             x_discrete;
    char            x_tag_obj[32];
    char            x_tag_base_circle[32];
    char            x_tag_bg_arc[32];
    char            x_tag_arc[32];
    char            x_tag_center_circle[32];
    char            x_tag_wiper[32];
    char            x_tag_wpr_c[32];
    char            x_tag_ticks[32];
    char            x_tag_outline[32];
    char            x_tag_square[32];
    char            x_tag_in[32];
    char            x_tag_out[32];
    char            x_tag_sel[32];
    char            x_tag_number[32];
    char            x_buf[MAX_NUMBOX_LEN]; // number buffer
    t_symbol       *x_ignore;
    int             x_ignore_int;
}t_knob;

// ---------------------- Helper functions ----------------------

static void knob_update_number(t_knob *x){
/*    post("x->x_fval = %f", x->x_fval);
    post("(int)x->x_fval = %d", (int)rint(x->x_fval));
    int val = (int)x->x_fval;
    post("val = %d", val);
    float dif = x->x_fval - (float)val;
    post("dif = %f", dif);*/
    char nbuf[32];
    if(x->x_fval > -1 && x->x_fval < 1)
        sprintf(nbuf, "%.5f", x->x_fval);
//    else if(x->x_fval == (int)x->x_fval)
//        sprintf(nbuf, "%.d", (int)x->x_fval);
    else
        sprintf(nbuf, "%#.5g", x->x_fval);
    pdgui_vmess(0, "crs rs", glist_getcanvas(x->x_glist),
        "itemconfigure", x->x_tag_number, "-text", nbuf);
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
    pdgui_vmess(0, "crs rsrs", x->x_cv, "itemconfigure",  x->x_tag_arc,
        "-outline", x->x_fg->s_name, "-fill", x->x_fg->s_name);
    pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_number,
        "-fill", x->x_fg->s_name);
    pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_ticks,
        "-fill", x->x_fg->s_name);
    pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_wiper,
        "-fill", x->x_fg->s_name);
    pdgui_vmess(0, "crs rsrs", x->x_cv, "itemconfigure", x->x_tag_wpr_c,
        "-outline", x->x_fg->s_name, "-fill", x->x_fg->s_name);
}

static void knob_config_bg_arc(t_knob *x){
    pdgui_vmess(0, "crs rsrs", x->x_cv, "itemconfigure",  x->x_tag_bg_arc,
        "-outline", x->x_mg->s_name, "-fill", x->x_mg->s_name);
}

static void knob_config_number(t_knob *x){ // show or hide number value
    t_atom at[2];
    SETSYMBOL(at, gensym(def_font));
    SETFLOAT(at+1, -x->x_fontsize*x->x_zoom);
    pdgui_vmess(0, "crs rA", x->x_cv, "itemconfigure", x->x_tag_number,
        "-font", 2, at);
    pdgui_vmess(0, "crs ii", x->x_cv, "moveto", x->x_tag_number,
        (x->x_obj.te_xpix + x->x_xpos)*x->x_zoom,
        (x->x_obj.te_ypix + x->x_ypos)*x->x_zoom);
}

static void show_number(t_knob *x, int force){ // show or hide number value
    if(x->x_number == 1 || (x->x_number == 2 && x->x_clicked))
        x->x_shownumber = 1; // mode 1 or mode 2 and clicked
    else if(x->x_number == 3 && x->x_typing)
        x->x_shownumber = 1; // mode 3 and typing
    else
        x->x_shownumber = 0;
    if((glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist)) || force)
        pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_number,
            "-state", x->x_shownumber ? "normal" : "hidden");
}

static void knob_config_bg(t_knob *x){
    pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_base_circle,
        "-fill", x->x_bg->s_name);
    pdgui_vmess(0, "crs rsrs", x->x_cv, "itemconfigure", x->x_tag_center_circle,
        "-outline", x->x_bg->s_name, "-fill", x->x_bg->s_name);
    if(x->x_square)
        pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_square,
            "-fill", x->x_bg->s_name);
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
// inlet, outlet, square/outline
    pdgui_vmess(0, "crs iiii", x->x_cv, "coords", x->x_tag_in,
        x1, y1, x1 + IOWIDTH*z, y1 + IHEIGHT*z);
    pdgui_vmess(0, "crs iiii", x->x_cv, "coords", x->x_tag_out,
        x1, y2 - OHEIGHT*z, x1 + IOWIDTH*z, y2);
    pdgui_vmess(0, "crs iiii", x->x_cv, "coords", x->x_tag_square,
        x1, y1, x2, y2);
    pdgui_vmess(0, "crs iiii", x->x_cv, "coords", x->x_tag_outline,
        x1, y1, x2, y2);
    pdgui_vmess(0, "crs ri", x->x_cv, "itemconfigure", x->x_tag_outline,
        "-width", z);
// wiper's width, wiper's center circle
    int w_width = circle_width * z / 20; // wiper width is 1/20 of circle size
    pdgui_vmess(0, "crs ri", x->x_cv, "itemconfigure", x->x_tag_wiper,
        "-width", w_width < z ? z : w_width); // wiper width
    int half_size = x->x_size * z / 2; // half_size
    int xc = x1 + half_size, yc = y1 + half_size; // center coords
    int wx1 = rint(xc - w_width), wy1 = rint(yc - w_width);
    int wx2 = rint(xc + w_width), wy2 = rint(yc + w_width);
    pdgui_vmess(0, "crs iiii", x->x_cv, "coords", x->x_tag_wpr_c,
        wx1, wy1, wx2, wy2); // wiper center circle
// knob circle stuff
    // knob arc
    pdgui_vmess(0, "crs iiii", x->x_cv, "coords", x->x_tag_bg_arc,
        x1 + offset + z, y1 + offset + z,
        x2 - offset - z, y2 - offset - z);
    pdgui_vmess(0, "crs iiii", x->x_cv, "coords", x->x_tag_arc,
        x1 + offset + z, y1 + offset + z,
        x2 - offset - z, y2 - offset - z);
    // knob circle (base)
    pdgui_vmess(0, "crs iiii", x->x_cv, "coords", x->x_tag_base_circle,
        x1 + offset, y1 + offset, x2 - offset, y2 - offset);
    pdgui_vmess(0, "crs ri", x->x_cv, "itemconfigure", x->x_tag_base_circle,
        "-width", z);
    // knob center
    int arcwidth = circle_width * z * 0.1; // arc width is 1/10 of circle
    if(arcwidth < 1)
        arcwidth = 1;
    pdgui_vmess(0, "crs iiii", x->x_cv, "coords", x->x_tag_center_circle,
        x1 + arcwidth + offset * z, y1 + arcwidth + offset * z,
        x2 - arcwidth - offset * z, y2 - arcwidth - offset * z);
}

// configure wiper center
static void knob_config_wcenter(t_knob *x){
    pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_wpr_c,
                "-state", x->x_clicked ? "normal" : "hidden");
}

// configure inlet outlet and outline/square
static void knob_config_io(t_knob *x){
    int inlet = (x->x_rcv == &s_ || x->x_rcv == gensym("empty")) && x->x_edit;
    pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_in,
        "-state", inlet ? "normal" : "hidden");
    int outlet = (x->x_snd == &s_ || x->x_snd == gensym("empty")) && x->x_edit;
    pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_out,
        "-state", outlet ? "normal" : "hidden");
    int outline = x->x_edit || x->x_square;
    pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_outline,
        "-state", outline ? "normal" : "hidden");
    pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_square,
        "-state", x->x_square ? "normal" : "hidden");
}

// configure arc
static void knob_config_arc(t_knob *x){
    pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_arc,
        "-state", x->x_arc && x->x_fval != x->x_start ? "normal" : "hidden");
    pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_bg_arc,
        "-state", x->x_arc ? "normal" : "hidden");
}

// Update Arc/Wiper according to position
static void knob_update(t_knob *x){
    t_float pos = x->x_pos;
    if(x->x_discrete){ // later just 1 tick case
        t_float steps = (x->x_steps < 2 ? 2 : (float)x->x_steps) -1;
        pos = rint(pos * steps) / steps;
    }
    float start = (x->x_start_angle / 90.0 - 1) * HALF_PI;
    float range = x->x_range / 180.0 * M_PI;
    float angle = start + pos * range; // pos angle
// configure arc
    knob_config_arc(x);
    pdgui_vmess(0, "crs sf sf", x->x_cv, "itemconfigure", x->x_tag_bg_arc,
        "-start", start * -180.0 / M_PI,
        "-extent", range * -179.99 / M_PI);
    start += (knob_getpos(x, x->x_start) * range);
    pdgui_vmess(0, "crs sf sf", x->x_cv, "itemconfigure", x->x_tag_arc,
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
    pdgui_vmess(0, "crs iiii", x->x_cv, "coords", x->x_tag_wiper, xc, yc, xp, yp);
}

//---------------------- DRAW STUFF ----------------------------//

// redraw ticks
static void knob_draw_ticks(t_knob *x){
    pdgui_vmess(0, "crs", x->x_cv, "delete", x->x_tag_ticks);
    if(!x->x_steps || !x->x_showticks)
        return;
    int z = x->x_zoom;
    int divs = x->x_steps;
    if((divs > 1) && ((x->x_range + 360) % 360 != 0)) // ????
        divs = divs - 1;
    float delta_w = x->x_range / (float)divs;
    int x0 = text_xpix(&x->x_obj, x->x_glist), y0 = text_ypix(&x->x_obj, x->x_glist);
    float half_size = (float)x->x_size*z / 2.0; // half_size
    int xc = x0 + half_size, yc = y0 + half_size; // center coords
    int start = x->x_start_angle - 90.0;
    if(x->x_square)
        half_size *= x->x_radius;
    if(x->x_steps == 1){
        int width = (x->x_size / 40);
        if(width < 1)
            width = 1;
        width *= 1.5;
        double pos = knob_getpos(x, x->x_start) * x->x_range;
        float w = pos + start; // tick angle
        w *= M_PI/180.0; // in radians
        float dx = half_size * cos(w), dy = half_size * sin(w);
        int x1 = xc + (int)(dx);
        int y1 = yc + (int)(dy);
        int x2 = xc + (int)(dx * 0.65);
        int y2 = yc + (int)(dy * 0.65);
        char *tags_ticks[] = {x->x_tag_ticks, x->x_tag_obj};
        pdgui_vmess(0, "crr iiii ri rs rS",
            x->x_cv, "create", "line",
            x1, y1, x2, y2,
            "-width", width * z,
            "-fill", x->x_fg->s_name,
            "-tags", 2, tags_ticks);
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
            x->x_cv, "create", "line",
            x1, y1, x2, y2,
            "-width", width * z,
            "-fill", x->x_fg->s_name,
            "-tags", 2, tags_ticks);
    }
}

// draw all and initialize stuff
static void knob_draw_new(t_knob *x){
    int x1 = text_xpix(&x->x_obj, x->x_glist);
    int y1 = text_ypix(&x->x_obj, x->x_glist);
// square
    char *tags_square[] = {x->x_tag_square, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rS", x->x_cv, "create", "rectangle",
        x1, y1, 0, 0, "-tags", 2, tags_square);
// outline
    char *tags_outline[] = {x->x_tag_outline, x->x_tag_obj, x->x_tag_sel};
    pdgui_vmess(0, "crr iiii rS", x->x_cv, "create", "rectangle",
        x1, y1, 0, 0, "-tags", 3, tags_outline);
// base circle
    char *tags_circle[] = {x->x_tag_base_circle, x->x_tag_obj, x->x_tag_sel};
    pdgui_vmess(0, "crr iiii rS", x->x_cv, "create", "oval",
        x1, y1, 0, 0, "-tags", 3, tags_circle);
// arc
    char *tags_arc_bg[] = {x->x_tag_bg_arc, x->x_tag_obj};
        pdgui_vmess(0, "crr iiii rS", x->x_cv, "create", "arc",
        x1, y1, 0, 0,
        "-tags", 2, tags_arc_bg);
    char *tags_arc[] = {x->x_tag_arc, x->x_tag_obj};
        pdgui_vmess(0, "crr iiii rS", x->x_cv, "create", "arc",
        x1, y1, 0, 0,
        "-tags", 2, tags_arc);
// center circle on top of arc
char *tags_center[] = {x->x_tag_center_circle, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rS", x->x_cv, "create", "oval",
    x1, y1, 0, 0, "-tags", 2, tags_center);
// wiper and wiper center
    char *tags_wiper_center[] = {x->x_tag_wpr_c, x->x_tag_obj};
        pdgui_vmess(0, "crr iiii rS", x->x_cv, "create", "oval",
        x1, y1, 0, 0,
        "-tags", 2, tags_wiper_center);
    char *tags_wiper[] = {x->x_tag_wiper, x->x_tag_obj};
        pdgui_vmess(0, "crr iiii rS", x->x_cv, "create", "line",
        x1, y1, 0, 0,
        "-tags", 2, tags_wiper);
    
// number
    char *tags_number[] = {x->x_tag_number, x->x_tag_obj};
    pdgui_vmess(0, "crr ii rs rS", x->x_cv, "create", "text",
        x1, y1,
        "-anchor", "w",
        "-tags", 2, tags_number);
// inlet
    char *tags_in[] = {x->x_tag_in, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rs rS", x->x_cv, "create", "rectangle",
        x1, y1, 0, 0,
        "-fill", "black",
        "-tags", 2, tags_in);
// outlet
    char *tags_out[] = {x->x_tag_out, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rs rS", x->x_cv, "create", "rectangle",
        x1, y1, 0, 0,
        "-fill", "black",
        "-tags", 2, tags_out);
// config and set
    knob_draw_ticks(x);
    knob_config_size(x);
    knob_config_io(x);
    knob_config_bg(x);
    knob_config_bg_arc(x);
    knob_config_fg(x);
    knob_config_arc(x);
    knob_config_wcenter(x);
    knob_config_number(x);
    knob_update(x);
    knob_update_number(x);
    show_number(x, 1);
    pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_sel,
        "-outline", x->x_sel ? "blue" : "black");       // ??????
    pdgui_vmess(0, "crs rs", x->x_cv, "itemconfigure", x->x_tag_number,
        "-fill", x->x_sel ? "blue" : x->x_fg->s_name);  // ??????
}

// ------------------------ knob widgetbehaviour-----------------------------
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
    t_canvas *cv = glist_getcanvas(glist);
    if(sel){
        pdgui_vmess(0, "crs rs", cv, "itemconfigure",
            x->x_tag_sel, "-outline", "blue");
        pdgui_vmess(0, "crs rs",  cv, "itemconfigure",
            x->x_tag_number, "-fill", "blue");
    }
    else{
        pdgui_vmess(0, "crs rs", cv, "itemconfigure",
            x->x_tag_number, "-fill", x->x_fg->s_name);
        pdgui_vmess(0, "crs rs", cv, "itemconfigure",
            x->x_tag_sel, "-outline", "black");
    }
}

void knob_vis(t_gobj *z, t_glist *glist, int vis){
    t_knob* x = (t_knob*)z;
    x->x_cv = glist_getcanvas(x->x_glist = glist);
    vis ? knob_draw_new(x) : pdgui_vmess(0, "crs", x->x_cv, "delete", x->x_tag_obj);
}

static void knob_delete(t_gobj *z, t_glist *glist){
    canvas_deletelinesfor(glist, (t_text *)z);
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
    binbuf_addv(b, "iffffsssssiiiiiiiifssiiiiiii", // 28 args
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
        x->x_range, // 16: i range
        x->x_offset, // 17: i offset
        x->x_jump, // 18: i offset
        x->x_start, // 19: f start
        x->x_param, // 20: s param
        x->x_var_raw, // 21: s var
        x->x_number, // 22: i number
        x->x_fontsize, // 23: i number
        x->x_xpos, // 24: i number
        x->x_ypos, // 25: i number
        x->x_savestate, // 26: i savestate
        x->x_loadbang, // 27: i loadbang
        x->x_showticks); // 28: i show ticks
    binbuf_addv(b, ";");
}

// ------------------------ knob methods -----------------------------

static void knob_set(t_knob *x, t_floatarg f){
    if(isnan(f) || isinf(f))
        return;
    double old = x->x_pos;
    x->x_fval = knob_clipfloat(x, f);
    x->x_pos = knob_getpos(x, x->x_fval);
    x->x_fval = knob_getfval(x);
    if(x->x_pos != old){
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
            knob_update(x);
        knob_update_number(x);
    }
}

static void knob_bang(t_knob *x){
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
        if(x->x_snd->s_thing)
            pd_float(x->x_snd->s_thing, x->x_fval);
    }
empty:
    if(x->x_var != gensym("empty"))
        value_setfloat(x->x_var, x->x_fval);
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
        x->x_load = knob_clipfloat(x, f);
        x->x_pos = knob_getpos(x, x->x_fval = x->x_load);
    }
    else
        return;
    if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist)){
        knob_update(x);
        if(x->x_steps == 1)
            knob_draw_ticks(x);
    }
}

static void knob_start(t_knob *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(!ac)
        x->x_start = x->x_fval;
    else if(ac == 1 && av->a_type == A_FLOAT){
        float f = atom_getfloat(av);
        x->x_start = knob_clipfloat(x, f);
    }
    else
        return;
    if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist)){
        knob_update(x);
        if(x->x_steps == 1)
            knob_draw_ticks(x);
    }
}

// set start/end angles
static void knob_start_end(t_knob *x, t_floatarg f1, t_floatarg f2){
    int range = f1 > 360 ? 360 : f1 < 0 ? 0 : (int)f1;
    int offset = f2 > 360 ? 360 : f2 < 0 ? 0 : (int)f2;
    if(x->x_range == range && x->x_offset == offset)
        return;
    x->x_range = range;
    x->x_offset = offset;
    int start = -(x->x_range/2) + x->x_offset;
    int end = x->x_range/2 + x->x_offset;
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
    x->x_start_angle = start;
    x->x_end_angle = end;
    knob_set(x, x->x_fval);
    if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist)){
        knob_draw_ticks(x);
        knob_update(x);
    }
}

static void knob_angle(t_knob *x, t_floatarg f){
    knob_start_end(x, f, x->x_offset);
}

static void knob_offset(t_knob *x, t_floatarg f){
    knob_start_end(x, x->x_range, f);
}

static void knob_size(t_knob *x, t_floatarg f){
    float size = f < MIN_SIZE ? MIN_SIZE : f;
    if(x->x_size != size){
        x->x_size = size;
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist)){
            knob_config_size(x);
            knob_draw_ticks(x);
            knob_update(x);
            canvas_fixlinesfor(x->x_glist, (t_text *)x);
        }
    }
}

/*static void knob_proportion(t_knob *x, t_floatarg f){
    float r = (f < 50 ? 50 : f > 100 ? 100 : f) / 100;
    if(x->x_radius != r){
        x->x_radius = r;
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
            knob_config_size(x);
            knob_update(x);
    }
}*/

static void knob_arc(t_knob *x, t_floatarg f){
    int arc = f != 0;
    if(x->x_arc != arc){
        x->x_arc = arc;
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
            knob_config_arc(x);
    }
}

static void knob_circular(t_knob *x, t_floatarg f){
    x->x_circular = (f != 0);
}

static void knob_discrete(t_knob *x, t_floatarg f){
    x->x_discrete = (f != 0);
}

static void knob_ticks(t_knob *x, t_floatarg f){
    int showticks = f != 0;
    if(showticks != x->x_showticks){
        x->x_showticks = showticks;
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
            knob_draw_ticks(x);
    }
}

static void knob_steps(t_knob *x, t_floatarg f){
    int steps = f < 0 ? 0 : (int)f;
    if(x->x_steps != steps){
        x->x_steps = steps;
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
            knob_draw_ticks(x);
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

static void knob_bgcolor(t_knob *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(!ac)
        return;
    t_symbol *color = getcolor(ac, av);
    if(x->x_bg != color){
        x->x_bg = color;
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
            knob_config_bg(x);
    }
}

/*static void knob_fill_bg(t_knob *x, t_floatarg f){
    x->x_fill_bg = (f != 0);
    if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
        knob_config_bg(x);
}*/

static void knob_arccolor(t_knob *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(!ac)
        return;
    t_symbol *color = getcolor(ac, av);
    if(x->x_mg != color){
        x->x_mg = color;
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
            knob_config_bg_arc(x);
    }
}

static void knob_fgcolor(t_knob *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(!ac)
        return;
    t_symbol *color = getcolor(ac, av);
    if(x->x_fg != color){
        x->x_fg = color;
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
            knob_config_fg(x);
    }
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
    t_symbol *var = s == gensym("empty") ? &s_ : canvas_realizedollar(x->x_glist, s);
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
    x->x_fval = knob_clipfloat(x, x->x_fval);
    x->x_load = knob_clipfloat(x, x->x_load);
    x->x_start = knob_clipfloat(x, x->x_start);
    if(x->x_lower == x->x_upper)
        x->x_fval = x->x_load = x->x_start = x->x_lower;
    if(x->x_log){
        if((x->x_lower <= 0 && x->x_upper >= 0) || (x->x_lower >= 0 && x->x_upper <= 0))
            pd_error(x, "[knob]: range can't contain '0' in log mode");
    }
    x->x_pos = knob_getpos(x, x->x_fval);
    if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
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
    if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
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
    if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
        knob_update(x);
}

static void knob_savestate(t_knob *x, t_floatarg f){
    x->x_savestate = f != 0;
}

static void knob_lb(t_knob *x, t_floatarg f){
    x->x_loadbang = f != 0;
}

static void knob_number(t_knob *x, t_floatarg f){
    x->x_number = f < 0 ? 0 : f > 3 ? 3 : (int)f;
    show_number(x, 0);
}

static void knob_numbersize(t_knob *x, t_floatarg f){
    x->x_fontsize = f < 8 ? 8 : (int)f;
    knob_config_number(x);
}

static void knob_numberpos(t_knob *x, t_floatarg xpos, t_floatarg ypos){
    x->x_xpos = (int)xpos, x->x_ypos = (int)ypos;
    knob_config_number(x);
}

static void knob_square(t_knob *x, t_floatarg f){
    int square = (int)f;
    if(x->x_square != square){
        x->x_square = (int)f;
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist)){
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

// --------------- properties stuff --------------------
static void knob_properties(t_gobj *z, t_glist *owner){
    t_knob *x = (t_knob *)z;
    knob_get_snd(x);
    knob_get_var(x);
    knob_get_rcv(x);
    pdgui_stub_vnew(&x->x_obj.ob_pd, "knob_dialog",
        owner, "ffffi ss ifi sss iiiiii fss iii iiii",
        (float)(x->x_size / x->x_zoom), x->x_lower, x->x_upper, x->x_load, x->x_circular,
        x->x_snd_raw->s_name, x->x_rcv_raw->s_name,
        x->x_expmode, x->x_exp, x->x_jump,
        x->x_bg->s_name, x->x_fg->s_name, x->x_mg->s_name,
        x->x_discrete, x->x_steps, x->x_arc, x->x_range, x->x_offset, x->x_square,
        x->x_start, x->x_param->s_name, x->x_var_raw->s_name,
        x->x_savestate, x->x_loadbang, x->x_showticks,
        x->x_fontsize, x->x_xpos, x->x_ypos, x->x_shownumber);
}

static void knob_apply(t_knob *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    t_atom undo[29];
    SETFLOAT(undo+0, x->x_size);
    SETFLOAT(undo+1, x->x_lower);
    SETFLOAT(undo+2, x->x_upper);
    SETFLOAT(undo+3, x->x_load);
    SETSYMBOL(undo+4, x->x_snd);
    SETSYMBOL(undo+5, x->x_rcv);
    SETFLOAT(undo+6, x->x_square);
    SETFLOAT(undo+7, x->x_log ? 1 : x->x_exp);
    SETFLOAT(undo+8, x->x_expmode);
    SETFLOAT(undo+9, x->x_jump);
    SETSYMBOL(undo+10, x->x_bg);
    SETSYMBOL(undo+11, x->x_mg);
    SETSYMBOL(undo+12, x->x_fg);
    SETFLOAT(undo+13, x->x_circular);
    SETFLOAT(undo+14, x->x_steps);
    SETFLOAT(undo+15, x->x_discrete);
    SETFLOAT(undo+16, x->x_arc);
    SETFLOAT(undo+17, x->x_range);
    SETFLOAT(undo+18, x->x_offset);
    SETFLOAT(undo+19, x->x_start);
    SETSYMBOL(undo+20, x->x_param);
    SETSYMBOL(undo+21, x->x_var);
    SETFLOAT(undo+22, x->x_savestate);
    SETFLOAT(undo+23, x->x_loadbang);
    SETFLOAT(undo+24, x->x_showticks);
    SETFLOAT(undo+25, x->x_fontsize);
    SETFLOAT(undo+26, x->x_xpos);
    SETFLOAT(undo+27, x->x_ypos);
    SETFLOAT(undo+28, x->x_shownumber);
    pd_undo_set_objectstate(x->x_glist, (t_pd*)x, gensym("dialog"), 29, undo, ac, av);
    int size = (int)atom_getintarg(0, ac, av);
    float min = atom_getfloatarg(1, ac, av);
    float max = atom_getfloatarg(2, ac, av);
    double load = atom_getfloatarg(3, ac, av);
    t_symbol* snd = atom_getsymbolarg(4, ac, av);
    t_symbol* rcv = atom_getsymbolarg(5, ac, av);
    int square = atom_getintarg(6, ac, av);
    float exp = atom_getfloatarg(7, ac, av);
    int expmode = atom_getintarg(8, ac, av);
    x->x_jump = atom_getintarg(9, ac, av);
    t_symbol *bg = atom_getsymbolarg(10, ac, av);
    t_symbol *mg = atom_getsymbolarg(11, ac, av);
    t_symbol *fg = atom_getsymbolarg(12, ac, av);
    x->x_circular = atom_getintarg(13, ac, av);
    int steps = atom_getintarg(14, ac, av);
    x->x_discrete = atom_getintarg(15, ac, av);
    int arc = atom_getintarg(16, ac, av) != 0;
    int range = atom_getintarg(17, ac, av);
    int offset = atom_getintarg(18, ac, av);
    float start = atom_getfloatarg(19, ac, av);
    t_symbol *param = atom_getsymbolarg(20, ac, av);
    t_symbol *var = atom_getsymbolarg(21, ac, av);
    x->x_savestate = atom_getintarg(22, ac, av);
    x->x_loadbang = atom_getintarg(23, ac, av);
    int ticks = atom_getintarg(24, ac, av);
    int fontsize = atom_getintarg(25, ac, av);
    int xpos = atom_getintarg(26, ac, av);
    int ypos = atom_getintarg(27, ac, av);
    int shownumber = atom_getintarg(28, ac, av);
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
    if(x->x_start != start){
        SETFLOAT(at, start);
        knob_start(x, NULL, 1, at);
    }
    knob_param(x, param);
    knob_var(x, var);
    knob_numbersize(x, fontsize);
    knob_numberpos(x, xpos, ypos);
    knob_number(x, shownumber);
}

// --------------- click + motion stuff --------------------

static int xm, ym;

static void knob_arrow_motion(t_knob *x, t_floatarg dir){
    float old = x->x_pos, pos;
    if(x->x_discrete){
        t_float ticks = (x->x_steps < 2 ? 2 : (float)x->x_steps) - 1;
        old = rint(old * ticks) / ticks;
        pos = old + dir/ticks;
    }
    else{
        float delta = dir;
        delta /= (float)(x->x_size) * x->x_zoom * 2; // 2 is 'sensitivity'
        if(x->x_shift)
            delta *= 0.01;
        pos = x->x_pos + delta;
    }
    if(x->x_circular){
        if(pos > 1)
            pos = 0;
        if(pos < 0)
            pos = 1;
    }
    else
        pos = pos > 1 ? 1 : pos < 0 ? 0 : pos;
    x->x_pos = pos;
    t_float fval = x->x_fval;
    x->x_fval = knob_getfval(x);
    if(fval != x->x_fval){
        knob_bang(x);
        knob_update_number(x);
    }
    if(old != x->x_pos){
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
            knob_update(x);
    }
}

static void knob_motion(t_knob *x, t_floatarg dx, t_floatarg dy){
    if(dx == 0 && dy == 0)
        return;
    float old = x->x_pos, pos;
    if(!x->x_circular){ // normal mode
        float delta = -dy;
        if(fabs(dx) > fabs(dy))
            delta = dx;
        delta /= (float)(x->x_size) * x->x_zoom * 2; // 2 is 'sensitivity'
        if(x->x_shift)
            delta *= 0.01;
        pos = x->x_pos + delta;
    }
    else{ // circular mode
        xm += dx, ym += dy;
        int xc = text_xpix(&x->x_obj, x->x_glist) + x->x_size / 2;
        int yc = text_ypix(&x->x_obj, x->x_glist) + x->x_size / 2;
        float alphacenter = (x->x_end_angle + x->x_start_angle) / 2;
        float alpha = atan2(xm - xc, -ym + yc) * 180.0 / M_PI;
        pos = (((int)((alpha - alphacenter + 180.0 + 360.0) * 100.0) % 36000) * 0.01
            + (alphacenter - x->x_start_angle - 180.0)) / x->x_range;
    }
    x->x_pos = pos > 1 ? 1 : pos < 0 ? 0 : pos; // should go up in non circular (to do)
    t_float fval = x->x_fval;
    x->x_fval = knob_getfval(x);
    if(fval != x->x_fval){
        knob_bang(x);
        knob_update_number(x);
    }
    if(old != x->x_pos){
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
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
    char c = fkey, buf[3];
    buf[1] = 0;
    if(c == 0){ // click out
        x->x_clicked = 0;
        knob_config_wcenter(x);
        char buff[512];
        sprintf(buff, "%s-active", x->x_snd->s_name);
        t_symbol *snd_active = gensym(buff);
        if(snd_active->s_thing)
            pd_float(snd_active->s_thing, x->x_clicked);
        knob_update_number(x);
        return;
    }
    else if(((c == '\n') || (c == 13))){ // enter
        knob_float(x, x->x_buf[0] == 0 ? x->x_fval : atof(x->x_buf));
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
        pdgui_vmess(0, "crs rs", glist_getcanvas(x->x_glist),
            "itemconfigure", x->x_tag_number, "-text", cp);
        x->x_buf[sl] = 0;
    
        char buff[512];
        sprintf(buff, "%s-typing", x->x_snd->s_name);
        t_symbol *snd_typing = gensym(buff);
        if(snd_typing->s_thing)
            pd_symbol(snd_typing->s_thing, gensym(cp));
    
    }
}

static void knob_learn(t_knob *x){
    if(x->x_snd == gensym("empty") || x->x_snd == &s_)
        return;
    char buff[512];
    sprintf(buff, "%s-learn", x->x_snd->s_name);
    t_symbol *snd_learn = gensym(buff);
    if(snd_learn->s_thing)
        pd_bang(snd_learn->s_thing);
}

static void knob_forget(t_knob *x){
    if(x->x_snd == gensym("empty") || x->x_snd == &s_)
        return;
    char buff[512];
    sprintf(buff, "%s-forget", x->x_snd->s_name);
    t_symbol *snd_forget = gensym(buff);
    if(snd_forget->s_thing)
        pd_bang(snd_forget->s_thing);
}

static int knob_click(t_gobj *z, struct _glist *glist, int xpix, int ypix, int shift, int alt, int dbl, int doit){
    t_knob *x = (t_knob *)z;
    x->x_ignore_int = dbl;
    if(x->x_ctrl && shift && doit){
        knob_forget(x);
        return(1);
    }
    if(alt && shift && doit){
        knob_learn(x);
        return(1);
    }
    if(alt && !x->x_ctrl && doit){
        knob_set(x, x->x_start);
        knob_bang(x);
        return(1);
    }
    if(doit){
        x->x_buf[0] = 0;
//        pd_bind(&x->x_obj.ob_pd, gensym("#keyname")); // listen to key events
        x->x_clicked = 1;
        show_number(x, 0);
        knob_config_wcenter(x);
        x->x_shift = shift;
        if(x->x_circular){
//            if(x->x_jump){
                xm = xpix;
                ym = ypix;
/*            }
            else{ // did not work
                float start = (x->x_start_angle / 90.0 - 1) * HALF_PI;
                float range = x->x_range / 180.0 * M_PI;
                float angle = start + x->x_pos * range; // pos angle
                int radius = (int)(x->x_size*x->x_zoom / 2.0);
                int x0 = text_xpix(&x->x_obj, x->x_glist), y0 = text_ypix(&x->x_obj, x->x_glist);
                int xp = x0 + radius + rint(radius * cos(angle)); // circle point x coordinate
                int yp = y0 + radius + rint(radius * sin(angle)); // circle point x coordinate
                xm = xp;
                ym = yp;
            }*/
        }
        else{
            if(x->x_jump){
                int xc = text_xpix(&x->x_obj, x->x_glist) + x->x_size / 2;
                int yc = text_ypix(&x->x_obj, x->x_glist) + x->x_size / 2;
                float alphacenter = (x->x_end_angle + x->x_start_angle) / 2;
                float alpha = atan2(xpix - xc, -ypix + yc) * 180.0 / M_PI;
                float pos = (((int)((alpha - alphacenter + 180.0 + 360.0) * 100.0) % 36000) * 0.01
                    + (alphacenter - x->x_start_angle - 180.0)) / x->x_range;
                x->x_pos = pos > 1 ? 1 : pos < 0 ? 0 : pos;
                x->x_fval = knob_getfval(x);
                knob_update(x);
            }
        }
        knob_bang(x);
        glist_grab(glist, &x->x_obj.te_g, (t_glistmotionfn)(knob_motion), knob_key, xpix, ypix);
        if(x->x_snd == gensym("empty") || x->x_snd == &s_)
            return(1);
        char buff[512];
        sprintf(buff, "%s-active", x->x_snd->s_name);
        t_symbol *snd_active = gensym(buff);
        if(snd_active->s_thing)
            pd_float(snd_active->s_thing, x->x_clicked);
    }
    return(1);
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
            knob_config_io(p->p_cnv);
        }
    }
}

// ---------------------- new / free / setup ----------------------

static void knob_loadbang(t_knob *x, t_float f){
    if((int)f == LB_LOAD && x->x_loadbang)
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
    if(x->x_rcv != gensym("empty"))
        pd_unbind(&x->x_obj.ob_pd, x->x_rcv);
    x->x_proxy->p_cnv = NULL;
    pdgui_stub_deleteforkey(x);
}

static void *knob_new(t_symbol *s, int ac, t_atom *av){
    t_knob *x = (t_knob *)pd_new(knob_class);
    x->x_ignore = s;
    x->x_buf[0] = 0;
    float loadvalue = 0.0, startvalue = 0.0, exp = 0.0, min = 0.0, max = 127.0;
    x->x_fontsize = 12, x->x_xpos = 6, x->x_ypos = -15;
    t_symbol *snd = gensym("empty"), *rcv = gensym("empty");
    t_symbol *param = gensym("empty"), *var = gensym("empty");
    int size = 50, circular = 0, steps = 0, discrete = 0;
    int arc = 1, angle = 320, offset = 0;
    x->x_bg = gensym("#dfdfdf"), x->x_mg = gensym("#7c7c7c"), x->x_fg = gensym("black");
    x->x_clicked = x->x_log = x->x_jump = x->x_number = x->x_savestate = 0;
    x->x_square = 1;
    x->x_glist = (t_glist *)canvas_getcurrent();
    x->x_cv = glist_getcanvas(x->x_glist);
    x->x_zoom = x->x_glist->gl_zoom;
    x->x_flag = 0;
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
            circular = atom_getintarg(11, ac, av); // 12: i circular
            steps = atom_getintarg(12, ac, av); // 13: i steps
            discrete = atom_getintarg(13, ac, av); // 14: i discrete
            arc = atom_getintarg(14, ac, av); // 15: i arc
            angle = atom_getintarg(15, ac, av); // 16: i range
            offset = atom_getintarg(16, ac, av); // 17: i offset
            x->x_jump = atom_getintarg(17, ac, av); // 18: i jump
            startvalue = atom_getfloatarg(18, ac, av); // 19: f start value
            param = atom_getsymbolarg(19, ac, av); // 20: s param
            var = atom_getsymbolarg(20, ac, av); // 21: s var
            x->x_number = atom_getintarg(21, ac, av); // 22: f number
            x->x_fontsize = atom_getintarg(22, ac, av); // 23: f number
            x->x_xpos = atom_getintarg(23, ac, av); // 24: f number xpos
            x->x_ypos = atom_getintarg(24, ac, av); // 25: f number ypos
            x->x_savestate = atom_getintarg(25, ac, av); // 26: f savestate
            x->x_loadbang = atom_getintarg(26, ac, av); // 27: f loadbang
            x->x_showticks = atom_getintarg(27, ac, av); // 28: f show ticks
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
                    x->x_square = 1;
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
                else if(sym == gensym("-start")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_FLOAT){
                            startvalue = atom_getfloat(av);
                            av++, ac--;
                        }
                        else
                            goto errstate;
                    }
                    else
                        goto errstate;
                }
                else if(sym == gensym("-circular")){
                    x->x_flag = 1, av++, ac--;
                    circular = 1;
                }
                else if(sym == gensym("-jump")){
                    x->x_flag = 1, av++, ac--;
                    x->x_jump = 1;
                }
                else if(sym == gensym("-savestate")){
                    x->x_flag = 1, av++, ac--;
                    x->x_savestate = 1;
                }
                else if(sym == gensym("-lb")){
                    x->x_flag = 1, av++, ac--;
                    x->x_loadbang = 1;
                }
                else if(sym == gensym("-lb")){
                    x->x_flag = 1, av++, ac--;
                    x->x_loadbang = 1;
                }
                else if(sym == gensym("-number")){
                    if(ac >= 2){
                        x->x_flag = 1, av++, ac--;
                        if(av->a_type == A_FLOAT){
                            int n = atom_getint(av);
                            x->x_number = n < 0 ? 0 : n > 3 ? 3 : n;
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
                            x->x_fontsize = n < 8 ? 8 : n;
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
    x->x_start = startvalue;
    x->x_circular = circular;
    x->x_steps = steps < 0 ? 0 : steps;
    x->x_discrete = discrete;
    x->x_arc = arc;
    x->x_range = angle < 0 ? 0 : angle > 360 ? 360 : angle;
    x->x_offset = offset < 0 ? 0 : offset > 360 ? 360 : offset;
    x->x_start_angle = -(x->x_range/2) + x->x_offset;
    x->x_end_angle = x->x_range/2 + x->x_offset;
    x->x_fval = x->x_load = loadvalue;
    x->x_pos = knob_getpos(x, x->x_fval);
    x->x_edit = x->x_glist->gl_edit;
    x->x_radius = 0.85;
    x->x_fill_bg = 1;
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING-1, ".x%lx", (unsigned long)x->x_glist);
    buf[MAXPDSTRING-1] = 0;
    x->x_proxy = edit_proxy_new(x, gensym(buf));
    sprintf(x->x_tag_obj, "%pOBJ", x);
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
    sprintf(x->x_tag_number, "%pNUM", x);
    if(x->x_rcv != gensym("empty"))
        pd_bind(&x->x_obj.ob_pd, x->x_rcv);
    pd_bind(&x->x_obj.ob_pd, gensym("#keyname")); // listen to key events
    outlet_new(&x->x_obj, &s_float);
    return(x);
errstate:
    pd_error(x, "[knob]: improper creation arguments");
    return(NULL);
}

void knob_setup(void){
    knob_class = class_new(gensym("knob"), (t_newmethod)knob_new,
        (t_method)knob_free, sizeof(t_knob), 0, A_GIMME, 0);
    class_addbang(knob_class,knob_bang);
    class_addfloat(knob_class, knob_float);
    class_addlist(knob_class, knob_list); // used for float and keypresses
    class_addmethod(knob_class, (t_method)knob_load, gensym("load"), A_GIMME, 0);
    class_addmethod(knob_class, (t_method)knob_start, gensym("start"), A_GIMME, 0);
    class_addmethod(knob_class, (t_method)knob_set, gensym("set"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_size, gensym("size"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_circular, gensym("circular"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_range, gensym("range"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_jump, gensym("jump"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_exp, gensym("exp"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_log, gensym("log"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_discrete, gensym("discrete"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_bgcolor, gensym("bgcolor"), A_GIMME, 0);
//    class_addmethod(knob_class, (t_method)knob_fill_bg, gensym("fill_bg"), A_FLOAT, 0);
//    class_addmethod(knob_class, (t_method)knob_proportion, gensym("proportion"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_arccolor, gensym("arccolor"), A_GIMME, 0);
    class_addmethod(knob_class, (t_method)knob_fgcolor, gensym("fgcolor"), A_GIMME, 0);
    class_addmethod(knob_class, (t_method)knob_send, gensym("send"), A_DEFSYM, 0);
    class_addmethod(knob_class, (t_method)knob_param, gensym("param"), A_DEFSYM, 0);
    class_addmethod(knob_class, (t_method)knob_var, gensym("var"), A_DEFSYM, 0);
    class_addmethod(knob_class, (t_method)knob_receive, gensym("receive"), A_DEFSYM, 0);
    class_addmethod(knob_class, (t_method)knob_arc, gensym("arc"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_angle, gensym("angle"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_offset, gensym("offset"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_steps, gensym("steps"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_ticks, gensym("ticks"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_zoom, gensym("zoom"), A_CANT, 0);
    class_addmethod(knob_class, (t_method)knob_apply, gensym("dialog"), A_GIMME, 0);
    class_addmethod(knob_class, (t_method)knob_motion, gensym("motion"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_square, gensym("square"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_number, gensym("number"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_savestate, gensym("savestate"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_lb, gensym("lb"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_loadbang, gensym("loadbang"), A_DEFFLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_numbersize, gensym("numbersize"), A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_numberpos, gensym("numberpos"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(knob_class, (t_method)knob_learn, gensym("learn"), 0);
    class_addmethod(knob_class, (t_method)knob_forget, gensym("forget"), 0);
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
    #include "../Extra/knob_dialog.h"
}
