// based on filterview

#include <stdio.h>

#define _USE_MATH_DEFINES
#include <math.h>

#include <stdlib.h>
#include <string.h>
#include <m_pd.h>
#include <m_imp.h>
#include <g_canvas.h>

#define BICOEFF_SELBDWIDTH 2
#define HANDLE_SIZE        12
#define BICOEFF_MINWIDTH   200
#define BICOEFF_MINHEIGHT  100

typedef struct _handle{
    t_pd            h_pd;
    struct _bicoeff *h_master;
    t_symbol       *h_bindsym;
    char            h_pathname[64], h_outlinetag[64];
    int             h_dragon;
    int             h_dragx, h_dragy;
    int             h_dx, h_dy;
}t_handle;

typedef struct _bicoeff_edit_proxy{
    t_object        p_obj;
    t_symbol       *p_sym;
    t_clock        *p_clock;
    struct _bicoeff *p_cnv;
}t_bicoeff_edit_proxy;

typedef struct _bicoeff{
    t_object    x_obj;
    t_glist*    x_glist;
    t_canvas   *x_cv;
    int         x_width;
    int         x_height;
    int         x_savestate;
    int         x_lb;
    int         x_sel;
    int         x_theme;
    int         x_transparent;
    int         x_edit;
    t_float     x_cf;
    t_float     x_bw;
    t_float     x_q_s;
    t_float     x_g;
    t_float     x_b1;
    t_float     x_b2;
    t_float     x_a0;
    t_float     x_a1;
    t_float     x_a2;
    t_symbol*   x_type;
    t_symbol*   x_bind_name;
    t_symbol*   x_bg;
    t_symbol*   x_gg;
    t_symbol*   x_fg;
    char        x_tag_outline[32];
    char        x_tag_sel[32];
    char        x_tag_obj[32];
    char        x_tag_bg[32];
    char        x_tag_fg[32];
    char        x_tag_in[32];
    char        x_tag_IO[32];
    char        x_tag_out[32];
    char        x_tag_graph[32];
    char        x_tag_graphline[32];
    char        x_tag_zline[32];
    char        x_tag_lbw[32];
    char        x_tag_rbw[32];
    char        x_tag_handle[32];
    char        x_tag[MAXPDSTRING];
    char        x_my[MAXPDSTRING];
    t_pd       *x_handle;
    t_bicoeff_edit_proxy *x_edit_proxy;
}t_bicoeff;

static t_class *bicoeff_class;
static t_class *bicoeff_edit_proxy_class;
static t_class *bicoeff_handle_class;
static t_widgetbehavior bicoeff_widgetbehavior;

// Helpers -------------------------------------------------------------------
static double bicoeff_ftom(double hz){
    return(12.0 * log(hz / 110.0) / log(2.0) + 45.0);
}

static double bicoeff_mtof(double midi){
    return(pow(2.0, (midi-45)/12.0)*110.0);
}

static double bicoeff_q_to_bw(double q){
    return(asinh(1.0 / (2.0 * q)) * 2.0 / log(2.0));
}

static double bicoeff_bw_to_q(double bw){
    return(1.0 / (2.0 * sinh(bw * log(2.0) / 2.0)));
}

static double bicoeff_get_xpos(t_bicoeff *x, double f){
    double midi_note = bicoeff_ftom(f);
    double x0 = (double)text_xpix(&x->x_obj, x->x_glist);
    double xpos = x0 + (((midi_note - 16.766) / 120.0) * (double)x->x_width);
    return(xpos);
}

static double bicoeff_get_ypos(t_bicoeff *x){
    double ratio = (25.0 - (double)x->x_g) / 50.0;
    double ypos = (double)x->x_height * ratio;
    return(ypos);
}

static double bicoeff_get_width(t_bicoeff *x){
    double bwf = x->x_cf * (1.0 + x->x_bw);
    double center_pos = bicoeff_get_xpos(x, x->x_cf);
    double right_pos = bicoeff_get_xpos(x, bwf);
    return(right_pos - center_pos);
}

static double bicoeff_get_mag(t_bicoeff *x, double fHz, double radians, int y0){
    (void)fHz;
    float x1 = cos(-1.0*radians);
    float x2 = cos(-2.0*radians);
    float y1 = sin(-1.0*radians);
    float y2 = sin(-2.0*radians);
    float A = x->x_a0 + x->x_a1*x1 + x->x_a2*x2;
    float B = x->x_a1*y1 + x->x_a2*y2;
    float C = 1 - x->x_b1*x1 - x->x_b2*x2;
    float D = 0 - x->x_b1*y1 - x->x_b2*y2;
    float halfframeheight = (float)x->x_height / 2.0;
    if(x->x_type == gensym("allpass")){ // plot phase
        float numerarg = atan2(B, A);
        float denomarg = atan2(D, C);
        float phase = numerarg-denomarg;
        if(phase > M_PI)
            phase = phase - 2*M_PI;
        else if(phase < -M_PI)
            phase = phase + 2*M_PI;
        float scaledphase = halfframeheight*(-phase/M_PI) + halfframeheight + y0;
        return(scaledphase);
    }
    else{ // plot magnitude
        float numermag = sqrt(A*A + B*B);
        float denommag = sqrt(C*C + D*D);
        float magnitude = numermag/denommag;
        float logmagnitude = 20.0*log(magnitude)/log(10);
        if(logmagnitude > 25.0)
            logmagnitude = 25.0;
        else if(logmagnitude < -25.0)
            logmagnitude = -25.0;
        logmagnitude = (logmagnitude/25.0) * halfframeheight;
        logmagnitude = -1.0 * logmagnitude + halfframeheight + y0;
        return(logmagnitude);
    }
}

// ---- theme-aware color accessors ----

static t_symbol *bicoeff_get_bg(t_bicoeff *x){
    if(x->x_theme){
        char buf[16];
        snprintf(buf, sizeof(buf), "#%06X", THISGUI->i_backgroundcolor);
        return(gensym(buf));
    }
    return(x->x_bg);
}

static t_symbol *bicoeff_get_fg(t_bicoeff *x){
    if(x->x_theme){
        char buf[16];
        snprintf(buf, sizeof(buf), "#%06X", THISGUI->i_foregroundcolor);
        return(gensym(buf));
    }
    return(x->x_fg);
}

static t_symbol *bicoeff_get_gg(t_bicoeff *x){
    if(x->x_theme){
        char buf[16];
        snprintf(buf, sizeof(buf), "#%06X", THISGUI->i_gopcolor);
        return(gensym(buf));
    }
    return(x->x_gg);
}

static int bicoeff_vis_check(t_bicoeff *x){
    return(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist));
}

// widgetbehavior
static void bicoeff_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2){
    t_bicoeff* x = (t_bicoeff*)z;
    *xp1 = text_xpix(&x->x_obj, glist);
    *yp1 = text_ypix(&x->x_obj, glist);
    *xp2 = *xp1 + x->x_width;
    *yp2 = *yp1 + x->x_height;
}

static void bicoeff_plot(t_bicoeff *x);

static void bicoeff_save(t_gobj *z, t_binbuf *b){
    t_bicoeff *x = (t_bicoeff *)z;
    binbuf_addv(b, "ssiisiisfffisssiii", gensym("#X"),gensym("obj"),
        (int)x->x_obj.te_xpix, (int)x->x_obj.te_ypix,
        atom_getsymbol(binbuf_getvec(x->x_obj.te_binbuf)),
        x->x_width, x->x_height,
        x->x_type,
        x->x_cf, x->x_q_s, x->x_g,
        x->x_savestate,
        x->x_bg, x->x_gg, x->x_fg,
        x->x_theme, x->x_transparent,
        x->x_lb);
    binbuf_addv(b, ";");
}

static void bicoeff_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_bicoeff *x = (t_bicoeff *)z;
    x->x_obj.te_xpix += dx, x->x_obj.te_ypix += dy;
    if(glist_isvisible(glist)){
        pdgui_vmess(0, "crs ii", glist_getcanvas(glist), "move", x->x_tag_obj, dx, dy);
        bicoeff_plot(x);
        canvas_fixlinesfor(glist_getcanvas(glist), (t_text*)x);
    }
}

static void bicoeff_select(t_gobj *z, t_glist *glist, int state);

static void bicoeff_delete(t_gobj *z, t_glist *glist){
    canvas_deletelinesfor(glist, (t_text *)z);
}

static void bicoeff_config_fg(t_bicoeff *x){
    t_canvas *cv = glist_getcanvas(x->x_glist);
    unsigned int fg = x->x_sel ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor;
    int width = x->x_sel ? BICOEFF_SELBDWIDTH : 1;
    if(x->x_edit){
        pdgui_vmess(0, "crs rk rk", cv, "itemconfigure", x->x_tag_fg,
            "-fill", fg, "-outline", fg);
    }
    pdgui_vmess(0, "crs rk ri", cv, "itemconfigure", x->x_tag_outline,
        "-outline", fg, "-width", width);
    pdgui_vmess(0, "crs rk", cv, "itemconfigure", x->x_tag_graphline,
        "-fill", fg);
}

// ---- handle ----

static void bicoeff_draw_handle(t_bicoeff *x, int state){
    t_handle *sh = (t_handle *)x->x_handle;
    if(sh->h_pathname[0])
        pdgui_vmess(0, "rs", "destroy", sh->h_pathname);
    if(state){
        unsigned int border = x->x_sel ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor;
        unsigned int bg = x->x_sel ? THISGUI->i_selectcolor : THISGUI->i_gopcolor;
        pdgui_vmess(0, "rs riri rkrkrk ri rr",
            "canvas", sh->h_pathname,
            "-width", HANDLE_SIZE, "-height", HANDLE_SIZE,
            "-bg", bg,
            "-highlightcolor", THISGUI->i_foregroundcolor,
            "-highlightbackground", border,
            "-highlightthickness", 1,
            "-cursor", "bottom_right_corner");
        int x1, y1, x2, y2;
        bicoeff_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        t_canvas *cv = x->x_cv;
        char *tags_handle[] = {x->x_tag_handle, x->x_tag_obj};
        pdgui_vmess(0, "crr ii rs ri ri rs rS",
            cv, "create", "window",
            x2 - HANDLE_SIZE + 1,
            y2 - HANDLE_SIZE + 1,
            "-anchor", "nw",
            "-width", HANDLE_SIZE,
            "-height", HANDLE_SIZE,
            "-window", sh->h_pathname,
            "-tags", 2, tags_handle);
        char buf[512];
        snprintf(buf, sizeof(buf),
            "bind %s <Button> {pdsend [concat %s _click 1 [.x%lx.c canvasx %%x] [.x%lx.c canvasy %%y] \\;]}",
            sh->h_pathname, sh->h_bindsym->s_name, (unsigned long)cv, (unsigned long)cv);
        pdgui_vmess(buf, NULL);
        snprintf(buf, sizeof(buf),
            "bind %s <ButtonRelease> {pdsend [concat %s _click 0 [.x%lx.c canvasx %%x] [.x%lx.c canvasy %%y] \\;]}",
            sh->h_pathname, sh->h_bindsym->s_name, (unsigned long)cv, (unsigned long)cv);
        pdgui_vmess(buf, NULL);
        snprintf(buf, sizeof(buf),
            "bind %s <Motion> {pdsend [concat %s _motion [.x%lx.c canvasx %%x] [.x%lx.c canvasy %%y] \\;]}",
            sh->h_pathname, sh->h_bindsym->s_name, (unsigned long)cv, (unsigned long)cv);
        pdgui_vmess(buf, NULL);
    }
}

// ---- drawing ----

static void bicoeff_draw(t_bicoeff *x){
    int x1, y1, x2, y2;
    bicoeff_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
    t_canvas *cv = glist_getcanvas(x->x_glist);
    int mid = y1 + (y2 - y1) / 2;
    unsigned int fg = x->x_sel ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor;
    int outline_width = x->x_sel ? BICOEFF_SELBDWIDTH : 1;

    pdgui_vmess(0, "s scss iiii",
        "bicoeff::drawme", x->x_my, glist_getcanvas(x->x_glist),
        x->x_bind_name->s_name, x->x_tag, x1, y1, x2, y2);

    char tag_response[64], tag_band[64], tag_bandedges[64], tag_lines[64];
    sprintf(tag_response, "response%s", x->x_tag);
    sprintf(tag_band, "band%s", x->x_tag);
    sprintf(tag_bandedges, "bandedges%s", x->x_tag);
    sprintf(tag_lines, "lines%s", x->x_tag);

    char *tags_bg[] = {x->x_tag_bg, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rs rS", cv, "create", "rectangle",
        x1, y1, x2, y2,
        "-fill", x->x_transparent ? "" : bicoeff_get_bg(x)->s_name, "-tags", 2, tags_bg);

    char *tags_fill[] = {x->x_tag_graph, tag_response, x->x_tag, x->x_tag_obj};
    pdgui_vmess(0, "crr iiiiiiii rs rS", cv, "create", "polygon",
        x1, mid, x2, mid, x2, y2, x1, y2,
        "-fill", bicoeff_get_fg(x)->s_name, "-tags", 4, tags_fill);

    char *tags_zline[] = {x->x_tag_zline, x->x_tag, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rs rS", cv, "create", "line",
        x1, mid, x2, mid,
        "-fill", bicoeff_get_gg(x)->s_name, "-tags", 3, tags_zline);

    char *tags_rline[] = {x->x_tag_graphline, tag_response, x->x_tag, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rk ri rS", cv, "create", "line",
        x1, mid, x2, mid,
        "-fill", fg, "-width", 1, "-tags", 4, tags_rline);

    char *tags_lbw[] = {x->x_tag_lbw, tag_bandedges, tag_band, tag_lines, x->x_tag, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rs ri rS", cv, "create", "line",
        x1, y1, x1, y2,
        "-fill", bicoeff_get_gg(x)->s_name, "-width", 1, "-tags", 6, tags_lbw);
    char *tags_rbw[] = {x->x_tag_rbw, tag_bandedges, tag_band, tag_lines, x->x_tag, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rs ri rS", cv, "create", "line",
        x2, y1, x2, y2,
        "-fill", bicoeff_get_gg(x)->s_name, "-width", 1, "-tags", 6, tags_rbw);

    if(x->x_edit){
        char *tags_in[] = {x->x_tag_in, x->x_tag_sel, x->x_tag_fg, x->x_tag_IO, x->x_tag_obj};
        pdgui_vmess(0, "crr iiii rS", cv, "create", "rectangle",
            x1, y1, x1 + IOWIDTH, y1 + IHEIGHT, "-tags", 5, tags_in);
        char *tags_out[] = {x->x_tag_out, x->x_tag_sel, x->x_tag_fg, x->x_tag_IO, x->x_tag_obj};
        pdgui_vmess(0, "crr iiii rS", cv, "create", "rectangle",
            x1, y2 - OHEIGHT, x1 + IOWIDTH, y2, "-tags", 5, tags_out);
    }

    char *tags_outline[] = {x->x_tag_outline, x->x_tag_obj, x->x_tag_sel};
    pdgui_vmess(0, "crr iiii rk rs ri rS", cv, "create", "rectangle",
        x1, y1, x2, y2, "-outline", fg, "-fill", "",
        "-width", outline_width, "-tags", 3, tags_outline);

    bicoeff_config_fg(x);
}

static void bicoeff_select(t_gobj *z, t_glist *glist, int state){
    t_bicoeff *x = (t_bicoeff *)z;
    x->x_sel = state;
    bicoeff_config_fg(x);
    if(x->x_edit)
        bicoeff_draw_handle(x, 1);
}

static void bicoeff_plot(t_bicoeff *x){
    int x1, y1, x2, y2;
    bicoeff_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
    t_canvas *cv = glist_getcanvas(x->x_glist);
    double samplerate = sys_getsr();
    if(samplerate <= 0)
        samplerate = 44100.0;

    double cf = bicoeff_get_xpos(x, x->x_cf);
    double fwidth = bicoeff_get_width(x);
    double halfwidth = fwidth * 0.5;
    double filterx1 = cf - halfwidth;
    double filterx2 = cf + halfwidth;
    double filtergain = bicoeff_get_ypos(x);

    char buf[128];
    snprintf(buf, sizeof(buf), "set %s::framex1 %.3f", x->x_my, (double)x1);
    pdgui_vmess(buf, NULL);
    snprintf(buf, sizeof(buf), "set %s::framey1 %.3f", x->x_my, (double)y1);
    pdgui_vmess(buf, NULL);
    snprintf(buf, sizeof(buf), "set %s::framex2 %.3f", x->x_my, (double)x2);
    pdgui_vmess(buf, NULL);
    snprintf(buf, sizeof(buf), "set %s::framey2 %.3f", x->x_my, (double)y2);
    pdgui_vmess(buf, NULL);
    snprintf(buf, sizeof(buf), "set %s::filtercenter %.3f", x->x_my, cf);
    pdgui_vmess(buf, NULL);
    snprintf(buf, sizeof(buf), "set %s::filterwidth %.3f", x->x_my, fwidth);
    pdgui_vmess(buf, NULL);
    snprintf(buf, sizeof(buf), "set %s::filtergain %.3f", x->x_my, filtergain);
    pdgui_vmess(buf, NULL);
    snprintf(buf, sizeof(buf), "set %s::filterx1 %.3f", x->x_my, filterx1);
    pdgui_vmess(buf, NULL);
    snprintf(buf, sizeof(buf), "set %s::filterx2 %.3f", x->x_my, filterx2);
    pdgui_vmess(buf, NULL);

    pdgui_vmess(0, "crs iiii", cv, "coords", x->x_tag_lbw,
        (int)filterx1, y1, (int)filterx1, y2);
    pdgui_vmess(0, "crs iiii", cv, "coords", x->x_tag_rbw,
        (int)filterx2, y1, (int)filterx2, y2);

    int framewidth = x2 - x1;
    int npoints = 0;
    for(int px = x1; px <= x2; px += 5)
        npoints++;
    int maxatoms = npoints * 2 + 4;
    t_atom *at = (t_atom *)getbytes(maxatoms * sizeof(t_atom));
    int n = 0;
    for(int px = x1; px <= x2; px += 5){
        double nn = ((double)(px - x1) / framewidth) * 120.0 + 16.766;
        double hz = bicoeff_mtof(nn);
        double radians = 2.0 * M_PI * hz / samplerate;
        double mag = bicoeff_get_mag(x, hz, radians, y1);
        SETFLOAT(at+n, (float)px); n++;
        SETFLOAT(at+n, (float)mag); n++;
    }
    pdgui_vmess(0, "crs A", cv, "coords", x->x_tag_graphline, n, at);
    SETFLOAT(at+n, (float)x2); n++;
    SETFLOAT(at+n, (float)y2); n++;
    SETFLOAT(at+n, (float)x1); n++;
    SETFLOAT(at+n, (float)y2); n++;
    pdgui_vmess(0, "crs A", cv, "coords", x->x_tag_graph, n, at);
    freebytes(at, maxatoms * sizeof(t_atom));
}

static void bicoeff_vis(t_gobj *z, t_glist *glist, int vis){
    t_bicoeff* x = (t_bicoeff*)z;
    t_canvas *cv = glist_getcanvas(glist);
    t_handle *sh = (t_handle *)x->x_handle;
    x->x_cv = cv;
    sprintf(sh->h_pathname, ".x%lx.h%lx", (unsigned long)cv, (unsigned long)sh);
    if(x->x_edit && sh->h_pathname[0])
        pdgui_vmess(0, "rs", "destroy", sh->h_pathname);
    if(vis){
        bicoeff_draw(x);
        bicoeff_plot(x);
        bicoeff_draw_handle(x, x->x_edit);
    }
    else{
        pdgui_vmess(0, "crs", cv, "delete", x->x_tag_obj);
    }
}

// Methods ------------------------------------------------------------------------

static void bicoeff_bang(t_bicoeff *x){
    t_atom at[5];
    SETFLOAT(at, x->x_b1);
    SETFLOAT(at+1, x->x_b2);
    SETFLOAT(at+2, x->x_a0);
    SETFLOAT(at+3, x->x_a1);
    SETFLOAT(at+4, x->x_a2);
    outlet_list(x->x_obj.ob_outlet, &s_list, 5, at);
}

static void bicoeff_calculate(t_bicoeff *x){
    double a0 = 0, a1 = 0, a2 = 0, b1 = 0, b2 = 0;
// allpass
    if(x->x_type == gensym("allpass")){
        double omega, alphaQ, cos_w, b0;
        double hz = x->x_cf;
        double q = x->x_q_s;
        double g = 1;
        double nyq = sys_getsr() * 0.5;
        if (hz < 0.1)
            hz = 0.1;
        if (hz > nyq - 0.1)
            hz = nyq - 0.1;
        if (q < 0.000001)
            q = 0.000001;
        omega = hz * M_PI/nyq;
        alphaQ = sin(omega) / (2*q);
        cos_w = cos(omega);
        b0 = alphaQ + 1;
        a0 = (1 - alphaQ) * g/b0;
        a1 = -2*cos_w  * g/b0;
        a2 = g;
        b1 = 2*cos_w / b0;
        b2 = (alphaQ - 1) / b0;
    }
// bandpass
    else if(x->x_type == gensym("bandpass")){
        double omega, alphaQ, cos_w, b0;
        double hz = x->x_cf;
        double q = x->x_q_s;
        double g = 1;
        double nyq = sys_getsr() * 0.5;
        if (hz < 0.1)
            hz = 0.1;
        if (hz > nyq - 0.1)
            hz = nyq - 0.1;
        if (q < 0.000001)
            q = 0.000001;
        omega = hz * M_PI/nyq;
        alphaQ = sin(omega) / (2*q);
        cos_w = cos(omega);
        b0 = alphaQ + 1;
        a0 = alphaQ  * g/b0;
        a1 = 0.;
        a2 = -a0;
        b1 = 2*cos_w / b0;
        b2 = (alphaQ - 1) / b0;
    }
// bandstop
    else if(x->x_type == gensym("notch")){
        double omega, alphaQ, cos_w, b0;
        double hz = x->x_cf;
        double q = x->x_q_s;
        double g = 1;
        double nyq = sys_getsr() * 0.5;
        if (hz < 0.1)
            hz = 0.1;
        if (hz > nyq - 0.1)
            hz = nyq - 0.1;
        if (q < 0.000001)
            q = 0.000001;
        omega = hz * M_PI/nyq;
        alphaQ = sin(omega) / (2*q);
        cos_w = cos(omega);
        b0 = alphaQ + 1;
        a0 = g / b0;
        a1 = -2*cos_w * g/b0;
        a2 = a0;
        b1 = 2*cos_w / b0;
        b2 = (alphaQ - 1) / b0;
    }
// eq
    else if(x->x_type == gensym("peaking")){
        double omega, alphaQ, cos_w, b0;
        double hz = x->x_cf;
        double q = x->x_q_s;
        double amp = pow(10, x->x_g / 40);
        double nyq = sys_getsr() * 0.5;
        if (hz < 0.1)
            hz = 0.1;
        if (hz > nyq - 0.1)
            hz = nyq - 0.1;
        if (q < 0.000001)
            q = 0.000001;
        omega = hz * M_PI/nyq;
        alphaQ = sin(omega) / (2*q);
        cos_w = cos(omega);
        b0 = alphaQ/amp + 1;
        a0 = (1 + alphaQ*amp) / b0;
        a1 = -2*cos_w / b0;
        a2 = (1 - alphaQ*amp) / b0;
        b1 = 2*cos_w / b0;
        b2 = (alphaQ/amp - 1) / b0;
    }
// highpass
    else if(x->x_type == gensym("highpass")){
        double omega, alphaQ, cos_w, b0;
        double hz = x->x_cf;
        double q = x->x_q_s;
        double g = 1;
        double nyq = sys_getsr() * 0.5;
        if (hz < 0.1)
            hz = 0.1;
        if (hz > nyq - 0.1)
            hz = nyq - 0.1;
        if (q < 0.000001)
            q = 0.000001;
        omega = hz * M_PI/nyq;
        alphaQ = sin(omega) / (2*q);
        cos_w = cos(omega);
        b0 = alphaQ + 1;
        a0 = (1+cos_w)  * g/(2*b0);
        a1 = -(1+cos_w)  * g/b0;
        a2 = a0;
        b1 = 2*cos_w / b0;
        b2 = (alphaQ - 1) / b0;
    }
// highshelf
    else if(x->x_type == gensym("highshelf")){
        double omega, alphaS, cos_w, b0;
        double hz = x->x_cf;
        double slope = x->x_q_s;
        double amp = pow(10, x->x_g / 40);
        double nyq = sys_getsr() * 0.5;
        if (hz < 0.1)
            hz = 0.1;
        if (hz > nyq - 0.1)
            hz = nyq - 0.1;
        if (slope < 0.000001)
            slope = 0.000001;
        if (slope > 1)
            slope = 1;
        omega = hz * M_PI/nyq;
        alphaS = sin(omega) * sqrt((amp*amp + 1) * (1/slope - 1) + 2*amp);
        cos_w = cos(omega);
        b0 = (amp+1) - (amp-1)*cos_w + alphaS;
        a0 = amp*(amp+1 + (amp-1)*cos_w + alphaS) / b0;
        a1 = -2*amp*(amp-1 + (amp+1)*cos_w) / b0;
        a2 = amp*(amp+1 + (amp-1)*cos_w - alphaS) / b0;
        b1 = -2*(amp-1 - (amp+1)*cos_w) / b0;
        b2 = -(amp+1 - (amp-1)*cos_w - alphaS) / b0;
    }
// lowpass
    else if(x->x_type == gensym("lowpass")){
        double omega, alphaQ, cos_w, b0;
        double hz = x->x_cf;
        double q = x->x_q_s;
        double g = 1;
        double nyq = sys_getsr() * 0.5;
        if(hz < 0.1)
            hz = 0.1;
        if(hz > nyq - 0.1)
            hz = nyq - 0.1;
        if(q < 0.000001)
            q = 0.000001;
        omega = hz * M_PI/nyq;
        alphaQ = sin(omega) / (2*q);
        cos_w = cos(omega);
        b0 = alphaQ + 1;
        a0 = (1-cos_w)  * g/(2*b0);
        a1 = (1-cos_w)  * g/b0;
        a2 = a0;
        b1 = 2*cos_w / b0;
        b2 = (alphaQ - 1) / b0;
    }
// lowshelf
    else if(x->x_type == gensym("lowshelf")){
        double omega, alphaS, cos_w, b0;
        double hz = x->x_cf;
        double slope = x->x_q_s;
        double amp = pow(10, x->x_g / 40);
        double nyq = sys_getsr() * 0.5;
        if (hz < 0.1)
            hz = 0.1;
        if (hz > nyq - 0.1)
            hz = nyq - 0.1;
        if (slope < 0.000001)
            slope = 0.000001;
        if (slope > 1)
            slope = 1;
        omega = hz * M_PI/nyq;
        alphaS = sin(omega) * sqrt((amp*amp + 1) * (1/slope - 1) + 2*amp);
        cos_w = cos(omega);
        b0 = (amp+1) + (amp-1)*cos_w + alphaS;
        a0 = amp*(amp+1 - (amp-1)*cos_w + alphaS) / b0;
        a1 = 2*amp*(amp-1 - (amp+1)*cos_w) / b0;
        a2 = amp*(amp+1 - (amp-1)*cos_w - alphaS) / b0;
        b1 = 2*(amp-1 + (amp+1)*cos_w) / b0;
        b2 = -(amp+1 + (amp-1)*cos_w - alphaS) / b0;
    }
// resonant
    else if(x->x_type == gensym("resonant")){
        double omega, alphaQ, cos_w, b0;
        double hz = x->x_cf;
        double q = x->x_q_s;
        double g = 1;
        double nyq = sys_getsr() * 0.5;
        if (hz < 0.1)
            hz = 0.1;
        if (hz > nyq - 0.1)
            hz = nyq - 0.1;
        if (q < 0.000001)
            q = 0.000001;
        omega = hz * M_PI/nyq;
        alphaQ = sin(omega) / (2*q);
        cos_w = cos(omega);
        b0 = alphaQ + 1;
        a0 = alphaQ*q  * g/b0;
        a1 = 0;
        a2 = -a0;
        b1 = 2*cos_w / b0;
        b2 = (alphaQ - 1) / b0;
    }
    x->x_b1 = b1;
    x->x_b2 = b2;
    x->x_a0 = a0;
    x->x_a1 = a1;
    x->x_a2 = a2;
}

static void bicoeff_doit(t_bicoeff *x){
    bicoeff_calculate(x);
    if(bicoeff_vis_check(x))
        bicoeff_plot(x);
    bicoeff_bang(x);
}

// callback from GUI
static void bicoeff__params(t_bicoeff *x, t_floatarg cfpix, t_floatarg bwpix, t_floatarg gpix,
t_floatarg xpos, t_floatarg ypos){
    (void)xpos; (void)ypos;
    int x0 = text_xpix(&x->x_obj, x->x_glist), y0 = text_ypix(&x->x_obj, x->x_glist);
    (void)y0;
    int fdif = (int)cfpix - x0;
    double width = (double)x->x_width;
    double height = (double)x->x_height;
    double fratio  = (double)fdif / width;
    double nn  = fratio * 120.0 + 16.766;
    x->x_cf = pow(2.0, (nn  - 45.0) / 12.0) * 110.0;
    double nn2 = ((double)(bwpix + fdif) / width) * 120.0 + 16.766;
    double bwf = pow(2.0, (nn2 - 45.0) / 12.0) * 110.0;
    x->x_bw  = (bwf / x->x_cf) - 1.0;
    x->x_q_s = bicoeff_bw_to_q(x->x_bw);
    x->x_g = (gpix / height) * -50.0 + 25.0;
    bicoeff_doit(x);
}

static t_symbol *bicoeff_getcolor(int ac, t_atom *av){
    if(av->a_type == A_SYMBOL)
        return(atom_getsymbol(av));
    if(ac < 3)
        return(&s_);
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

static void bicoeff_send_color_validate(t_bicoeff *x, const char *color, int which){
    char buf[512];
    snprintf(buf, sizeof(buf), "set color_name {%s}", color);
    pdgui_vmess(buf, NULL);
    pdgui_vmess("set color_escaped [string map {{ } {\\ }} $color_name]", NULL);
    pdgui_vmess("set hex_color [color2hex $color_name]", NULL);
    snprintf(buf, sizeof(buf),
        "if {$hex_color eq \"\"} "
        "{pdsend \"%s _validate 0 $color_escaped %d\"} "
        "else {pdsend \"%s _validate 1 $hex_color %d\"}",
        x->x_bind_name->s_name, which,
        x->x_bind_name->s_name, which);
    pdgui_vmess(buf, NULL);
}

static void bicoeff_validate(t_bicoeff *x, t_floatarg v, t_symbol *color, t_floatarg t){
    int valid = (int)v;
    int type = (int)t;
    switch(type){
        case 0:
            if(valid){
                x->x_bg = color;
                if(bicoeff_vis_check(x) && !x->x_transparent)
                    pdgui_vmess(0, "crs rs", glist_getcanvas(x->x_glist), "itemconfigure",
                        x->x_tag_bg, "-fill", bicoeff_get_bg(x)->s_name);
            }
            else
                pd_error(0, "[bicoeff] invalid background color '%s'", color->s_name);
            break;
        case 1:
            if(valid){
                x->x_gg = color;
                if(bicoeff_vis_check(x)){
                    t_canvas *cv = glist_getcanvas(x->x_glist);
                    pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_zline, "-fill", bicoeff_get_gg(x)->s_name);
                    pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_lbw, "-fill", bicoeff_get_gg(x)->s_name);
                    pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_rbw, "-fill", bicoeff_get_gg(x)->s_name);
                }
            }
            else
                pd_error(0, "[bicoeff] invalid grid color '%s'", color->s_name);
            break;
        case 2:
            if(valid){
                x->x_fg = color;
                if(bicoeff_vis_check(x))
                    pdgui_vmess(0, "crs rs", glist_getcanvas(x->x_glist), "itemconfigure",
                        x->x_tag_graph, "-fill", bicoeff_get_fg(x)->s_name);
            }
            else
                pd_error(0, "[bicoeff] invalid fill color '%s'", color->s_name);
            break;
    }
}

static void bicoeff_bgcolor(t_bicoeff *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(!ac) return;
    t_symbol *color = bicoeff_getcolor(ac, av);
    if(color == &s_) return;
    bicoeff_send_color_validate(x, color->s_name, 0);
}

static void bicoeff_fgcolor(t_bicoeff *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(!ac) return;
    t_symbol *color = bicoeff_getcolor(ac, av);
    if(color == &s_) return;
    bicoeff_send_color_validate(x, color->s_name, 2);
}

static void bicoeff_gridcolor(t_bicoeff *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(!ac) return;
    t_symbol *color = bicoeff_getcolor(ac, av);
    if(color == &s_) return;
    bicoeff_send_color_validate(x, color->s_name, 1);
}

static void bicoeff_setbgcolor(t_bicoeff *x, t_symbol *s, int ac, t_atom *av){
    x->x_theme = 0;
    bicoeff_bgcolor(x, s, ac, av);
}

static void bicoeff_setfgcolor(t_bicoeff *x, t_symbol *s, int ac, t_atom *av){
    x->x_theme = 0;
    bicoeff_fgcolor(x, s, ac, av);
}

static void bicoeff_setgridcolor(t_bicoeff *x, t_symbol *s, int ac, t_atom *av){
    x->x_theme = 0;
    bicoeff_gridcolor(x, s, ac, av);
}

static void bicoeff_colors(t_bicoeff *x, t_symbol *bg, t_symbol *gg, t_symbol *fg){
    x->x_theme = 0;
    t_atom at[1];
    SETSYMBOL(at, bg);
    bicoeff_bgcolor(x, NULL, 1, at);
    SETSYMBOL(at, gg);
    bicoeff_gridcolor(x, NULL, 1, at);
    SETSYMBOL(at, fg);
    bicoeff_fgcolor(x, NULL, 1, at);
}

static void bicoeff_theme(t_bicoeff *x, t_floatarg f){
    int theme = (int)(f != 0);
    if(x->x_theme == theme)
        return;
    x->x_theme = theme;
    if(bicoeff_vis_check(x)){
        pdgui_vmess(0, "crs", glist_getcanvas(x->x_glist), "delete", x->x_tag_obj);
        bicoeff_draw(x);
        bicoeff_plot(x);
        if(x->x_edit)
            bicoeff_draw_handle(x, 1);
    }
}

static void bicoeff_transparent(t_bicoeff *x, t_floatarg f){
    int transp = (int)(f != 0);
    if(transp == x->x_transparent)
        return;
    x->x_transparent = transp;
    if(bicoeff_vis_check(x))
        pdgui_vmess(0, "crs rs", glist_getcanvas(x->x_glist), "itemconfigure",
            x->x_tag_bg, "-fill", transp ? "" : bicoeff_get_bg(x)->s_name);
}

static void bicoeff_dim(t_bicoeff *x, t_floatarg f1, t_floatarg f2){
    int width = f1 < BICOEFF_MINWIDTH ? BICOEFF_MINWIDTH : (int)f1;
    int height = f2 < BICOEFF_MINHEIGHT ? BICOEFF_MINHEIGHT : (int)f2;
    if(x->x_width != width || x->x_height != height){
        x->x_width = width;
        x->x_height = height;
        if(bicoeff_vis_check(x)){
            pdgui_vmess(0, "crs", glist_getcanvas(x->x_glist), "delete", x->x_tag_obj);
            bicoeff_draw(x);
            bicoeff_plot(x);
            canvas_fixlinesfor(x->x_glist, (t_text*)x);
            bicoeff_draw_handle(x, x->x_edit);
        }
    }
}

static void bicoeff_allpass(t_bicoeff *x, t_symbol *s, int ac, t_atom* av){
    (void)s;
    x->x_type = gensym("allpass");
    if(ac == 2){
        x->x_cf = atom_getfloat(av);
        x->x_bw = bicoeff_q_to_bw(x->x_q_s = atom_getfloat(av+1));
    }
    bicoeff_doit(x);
}

static void bicoeff_bandpass(t_bicoeff *x, t_symbol *s, int ac, t_atom* av){
    (void)s;
    x->x_type = gensym("bandpass");
    if(ac == 2){
        x->x_cf = atom_getfloat(av);
        x->x_bw = bicoeff_q_to_bw(x->x_q_s = atom_getfloat(av+1));
    }
    bicoeff_doit(x);
}

static void bicoeff_highpass(t_bicoeff *x, t_symbol *s, int ac, t_atom* av){
    (void)s;
    x->x_type = gensym("highpass");
    if(ac == 2){
        x->x_cf = atom_getfloat(av);
        x->x_bw = bicoeff_q_to_bw(x->x_q_s = atom_getfloat(av+1));
    }
    bicoeff_doit(x);
}

static void bicoeff_highshelf(t_bicoeff *x, t_symbol *s, int ac, t_atom* av){
    (void)s;
    x->x_type = gensym("highshelf");
    if(ac == 3){
        x->x_cf = atom_getfloat(av);
        x->x_bw = bicoeff_q_to_bw(x->x_q_s = atom_getfloat(av+1));
        x->x_g = atom_getfloat(av+2);
    }
    bicoeff_doit(x);
}

static void bicoeff_lowpass(t_bicoeff *x, t_symbol *s, int ac, t_atom* av){
    (void)s;
    x->x_type = gensym("lowpass");
    if(ac == 2){
        x->x_cf = atom_getfloat(av);
        x->x_bw = bicoeff_q_to_bw(x->x_q_s = atom_getfloat(av+1));
    }
    bicoeff_doit(x);
}

static void bicoeff_lowshelf(t_bicoeff *x, t_symbol *s, int ac, t_atom* av){
    (void)s;
    x->x_type = gensym("lowshelf");
    if(ac == 3){
        x->x_cf = atom_getfloat(av);
        x->x_bw = bicoeff_q_to_bw(x->x_q_s = atom_getfloat(av+1));
        x->x_g = atom_getfloat(av+2);
    }
    bicoeff_doit(x);
}

static void bicoeff_notch(t_bicoeff *x, t_symbol *s, int ac, t_atom* av){
    (void)s;
    x->x_type = gensym("notch");
    if(ac == 2){
        x->x_cf = atom_getfloat(av);
        x->x_bw = bicoeff_q_to_bw(x->x_q_s = atom_getfloat(av+1));
    }
    bicoeff_doit(x);
}

static void bicoeff_eq(t_bicoeff *x, t_symbol *s, int ac, t_atom* av){
    (void)s;
    x->x_type = gensym("peaking");
    if(ac == 3){
        x->x_cf = atom_getfloat(av);
        x->x_bw = bicoeff_q_to_bw(x->x_q_s = atom_getfloat(av+1));
        x->x_g = atom_getfloat(av+2);
    }
    bicoeff_doit(x);
}

static void bicoeff_resonant(t_bicoeff *x, t_symbol *s, int ac, t_atom* av){
    (void)s;
    x->x_type = gensym("resonant");
    if(ac == 2){
        x->x_cf = atom_getfloat(av);
        x->x_bw = bicoeff_q_to_bw(x->x_q_s = atom_getfloat(av+1));
    }
    bicoeff_doit(x);
}

static void bicoeff_savestate(t_bicoeff *x, t_floatarg f){
    x->x_savestate = (f != 0);
}

static void bicoeff_loadbang(t_bicoeff *x, t_float f){
    if((int)f == LB_LOAD && x->x_lb)
        bicoeff_bang(x);
}

static void bicoeff_properties(t_gobj *z, t_glist *owner){
    (void)owner;
    t_bicoeff *x = (t_bicoeff *)z;
    char pd_bg[32], pd_grid[32], pd_fg[32];
    snprintf(pd_bg, sizeof(pd_bg), "#%06X", THISGUI->i_backgroundcolor);
    snprintf(pd_grid, sizeof(pd_grid), "#%06X", THISGUI->i_gopcolor);
    snprintf(pd_fg, sizeof(pd_fg), "#%06X", THISGUI->i_foregroundcolor);
    pdgui_stub_vnew(&x->x_obj.ob_pd, "bicoeff_dialog", x,
        "ii iii s ssssss ii",
        x->x_width, x->x_height,
        x->x_savestate, x->x_lb,
        x->x_theme,
        x->x_type->s_name,
        x->x_bg->s_name, x->x_gg->s_name, x->x_fg->s_name,
        pd_bg, pd_grid, pd_fg,
        x->x_theme, x->x_transparent);
}

static void bicoeff_apply(t_bicoeff *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_atom undo[10];
    SETFLOAT(undo+0, x->x_width);
    SETFLOAT(undo+1, x->x_height);
    SETFLOAT(undo+2, x->x_savestate);
    SETFLOAT(undo+3, x->x_lb);
    SETSYMBOL(undo+4, x->x_type);
    SETSYMBOL(undo+5, x->x_bg);
    SETSYMBOL(undo+6, x->x_gg);
    SETSYMBOL(undo+7, x->x_fg);
    SETFLOAT(undo+8, x->x_theme);
    SETFLOAT(undo+9, x->x_transparent);
    pd_undo_set_objectstate(x->x_glist, (t_pd*)x, gensym("dialog"), 10, undo, ac, av);

    int w = atom_getintarg(0, ac, av);
    int h = atom_getintarg(1, ac, av);
    int savestate = atom_getintarg(2, ac, av);
    int lb = atom_getintarg(3, ac, av);
    t_symbol *type = atom_getsymbolarg(4, ac, av);
    t_symbol *bg = atom_getsymbolarg(5, ac, av);
    t_symbol *gg = atom_getsymbolarg(6, ac, av);
    t_symbol *fg = atom_getsymbolarg(7, ac, av);
    int theme = atom_getintarg(8, ac, av);
    int transp = atom_getintarg(9, ac, av);

    int width = w < BICOEFF_MINWIDTH ? BICOEFF_MINWIDTH : w;
    int height = h < BICOEFF_MINHEIGHT ? BICOEFF_MINHEIGHT : h;
    int dimchanged = (width != x->x_width || height != x->x_height);
    int typechanged = (type != x->x_type);

    x->x_width = width;
    x->x_height = height;
    x->x_savestate = (savestate != 0);
    x->x_lb = (lb != 0);
    x->x_type = type;
    x->x_bg = bg;
    x->x_gg = gg;
    x->x_fg = fg;
    x->x_theme = (theme != 0);
    x->x_transparent = (transp != 0);

    if(typechanged)
        bicoeff_calculate(x);

    if(bicoeff_vis_check(x)){
        pdgui_vmess(0, "crs", glist_getcanvas(x->x_glist), "delete", x->x_tag_obj);
        bicoeff_draw(x);
        bicoeff_plot(x);
        if(dimchanged)
            canvas_fixlinesfor(x->x_glist, (t_text*)x);
        bicoeff_draw_handle(x, x->x_edit);
    }
    canvas_dirty(x->x_glist, 1);
}

// ---- handle callbacks ----

static void handle__click_callback(t_handle *sh, t_floatarg f, t_floatarg xarg, t_floatarg yarg){
    int click = (int)f;
    t_bicoeff *x = sh->h_master;
    if(sh->h_dragon && click == 0){
        char buf[128];
        snprintf(buf, sizeof(buf), ".x%lx.c delete %s", (unsigned long)x->x_cv, sh->h_outlinetag);
        pdgui_vmess(buf, NULL);
        int width = x->x_width + sh->h_dx;
        int height = x->x_height + sh->h_dy;
        width = width < BICOEFF_MINWIDTH ? BICOEFF_MINWIDTH : width;
        height = height < BICOEFF_MINHEIGHT ? BICOEFF_MINHEIGHT : height;
        if(width != x->x_width || height != x->x_height){
            t_atom undo[2], redo[2];
            SETFLOAT(undo+0, x->x_width);
            SETFLOAT(undo+1, x->x_height);
            SETFLOAT(redo+0, width);
            SETFLOAT(redo+1, height);
            pd_undo_set_objectstate(x->x_glist, (t_pd*)x, gensym("dim"), 2, undo, 2, redo);
            bicoeff_dim(x, width, height);
            canvas_dirty(x->x_glist, 1);
        }
        bicoeff_draw_handle(x, 1);
    }
    else if(!sh->h_dragon && click){
        int x1, y1, x2, y2;
        bicoeff_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        pdgui_vmess(0, "crr iiii rk ri rs",
            x->x_cv, "create", "rectangle",
            x1, y1, x2, y2,
            "-outline", THISGUI->i_selectcolor,
            "-width", BICOEFF_SELBDWIDTH,
            "-tags", sh->h_outlinetag);
        pdgui_vmess(0, "rr rk", sh->h_pathname, "configure",
            "-highlightcolor", THISGUI->i_selectcolor);
        sh->h_dragx = (int)xarg;
        sh->h_dragy = (int)yarg;
        sh->h_dx = sh->h_dy = 0;
    }
    sh->h_dragon = click;
}

static void handle__motion_callback(t_handle *sh, t_floatarg xarg, t_floatarg yarg){
    if(sh->h_dragon){
        t_bicoeff *x = sh->h_master;
        int dx = (int)xarg - sh->h_dragx, dy = (int)yarg - sh->h_dragy;
        if(x->x_width + dx < BICOEFF_MINWIDTH)
            dx = BICOEFF_MINWIDTH - x->x_width;
        if(x->x_height + dy < BICOEFF_MINHEIGHT)
            dy = BICOEFF_MINHEIGHT - x->x_height;
        int x1, y1, x2, y2;
        bicoeff_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        char buf[128];
        snprintf(buf, sizeof(buf), ".x%lx.c coords %s %d %d %d %d",
            (unsigned long)x->x_cv, sh->h_outlinetag, x1, y1, x2 + dx, y2 + dy);
        pdgui_vmess(buf, NULL);
        sh->h_dx = dx;
        sh->h_dy = dy;
    }
}

// ---- edit proxy ----

static void bicoeff_edit_proxy_free(t_bicoeff_edit_proxy *p){
    pd_unbind(&p->p_obj.ob_pd, p->p_sym);
    clock_free(p->p_clock);
    pd_free(&p->p_obj.ob_pd);
}

static t_bicoeff_edit_proxy *bicoeff_edit_proxy_new(t_bicoeff *x, t_symbol *s){
    t_bicoeff_edit_proxy *p = (t_bicoeff_edit_proxy*)pd_new(bicoeff_edit_proxy_class);
    p->p_cnv = x;
    pd_bind(&p->p_obj.ob_pd, p->p_sym = s);
    p->p_clock = clock_new(p, (t_method)bicoeff_edit_proxy_free);
    return(p);
}

static void bicoeff_edit_proxy_any(t_bicoeff_edit_proxy *p, t_symbol *s, int ac, t_atom *av){
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
            if(bicoeff_vis_check(p->p_cnv)){
                t_canvas *cv = glist_getcanvas(p->p_cnv->x_glist);
                if(edit){
                    pdgui_vmess(0, "crs", cv, "delete", p->p_cnv->x_tag_obj);
                    bicoeff_draw(p->p_cnv);
                    bicoeff_plot(p->p_cnv);
                }
                else{
                    pdgui_vmess(0, "crs", cv, "delete", p->p_cnv->x_tag_in);
                    pdgui_vmess(0, "crs", cv, "delete", p->p_cnv->x_tag_out);
                }
                bicoeff_draw_handle(p->p_cnv, edit);
            }
        }
    }
}

// ---- new / free / setup ----

static void *bicoeff_new(t_symbol *s, int ac, t_atom* av){
    (void)s;
    t_bicoeff *x = (t_bicoeff *)pd_new(bicoeff_class);
    int width = 450, height = 150;
    x->x_savestate = 0;
    x->x_lb = 1;
    x->x_sel = 0;
    x->x_theme = 0;
    x->x_transparent = 0;
    float cf = 1000, q = 0.5, g = 0;
    t_symbol *type = gensym("peaking");
    t_symbol *bg_sym = gensym("#ffffff");
    t_symbol *gg_sym = gensym("#bbbbcc");
    t_symbol *fg_sym = gensym("#dcdcdc");
    x->x_type = type;
    x->x_cf = cf;
    x->x_q_s = q;
    x->x_g = g;
    if(ac && av->a_type == A_FLOAT){
        int w = (int)av->a_w.w_float;
        width = w < 100 ? 100 : w;
        ac--, av++;
        if(ac && av->a_type == A_FLOAT){
            int h = (int)av->a_w.w_float;
            height = h < 50 ? 50 : h;
            ac--, av++;
            if(ac && av->a_type == A_SYMBOL){
                type = av->a_w.w_symbol;
                ac--, av++;
                if(ac && av->a_type == A_FLOAT){
                    cf = av->a_w.w_float;
                    ac--, av++;
                    if(ac && av->a_type == A_FLOAT){
                        q = av->a_w.w_float;
                        ac--, av++;
                        if(ac && av->a_type == A_FLOAT){
                            g = av->a_w.w_float;
                            ac--, av++;
                            if(ac && av->a_type == A_FLOAT){
                                x->x_savestate = (int)av->a_w.w_float;
                                ac--, av++;
                            }
                            if(ac && av->a_type == A_SYMBOL){
                                bg_sym = atom_getsymbol(av);
                                ac--, av++;
                                if(ac && av->a_type == A_SYMBOL){
                                    gg_sym = atom_getsymbol(av);
                                    ac--, av++;
                                    if(ac && av->a_type == A_SYMBOL){
                                        fg_sym = atom_getsymbol(av);
                                        ac--, av++;
                                    }
                                }
                            }
                            if(ac && av->a_type == A_FLOAT){
                                x->x_theme = (int)av->a_w.w_float;
                                ac--, av++;
                            }
                            if(ac && av->a_type == A_FLOAT){
                                x->x_transparent = (int)av->a_w.w_float;
                                ac--, av++;
                            }
                            if(ac && av->a_type == A_FLOAT){
                                x->x_lb = (int)(av->a_w.w_float != 0);
                                ac--, av++;
                            }
                        }
                    }
                }
            }
        }
    }
    while(ac > 0){
        if(av->a_type == A_SYMBOL){
            t_symbol *sym = atom_getsymbolarg(0, ac, av);
            if(sym == gensym("-dim")){
                if(ac >= 3 && (av+1)->a_type == A_FLOAT){
                    width = atom_getfloatarg(1, ac, av);
                    height = atom_getfloatarg(2, ac, av);
                    ac-=3, av+=3;
                }
                else goto errstate;
            }
            else if(sym == gensym("-type")){
                if(ac >= 5 && (av+1)->a_type == A_SYMBOL){
                    x->x_type = type = atom_getsymbolarg(1, ac, av);
                    x->x_cf = cf = atom_getfloatarg(2, ac, av);
                    x->x_q_s = q = atom_getfloatarg(3, ac, av);
                    x->x_g = g = atom_getfloatarg(4, ac, av);
                    ac-=5, av+=5;
                }
                else goto errstate;
            }
            else if(sym == gensym("-bgcolor")){
                if(ac >= 2 && (av+1)->a_type == A_SYMBOL){
                    bg_sym = atom_getsymbolarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(sym == gensym("-gridcolor")){
                if(ac >= 2 && (av+1)->a_type == A_SYMBOL){
                    gg_sym = atom_getsymbolarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(sym == gensym("-fgcolor")){
                if(ac >= 2 && (av+1)->a_type == A_SYMBOL){
                    fg_sym = atom_getsymbolarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(sym == gensym("-theme")){
                x->x_theme = 1;
                ac--, av++;
            }
            else if(sym == gensym("-transparent")){
                x->x_transparent = 1;
                ac--, av++;
            }
            else if(sym == gensym("-savestate")){
                x->x_savestate = 1;
                ac--, av++;
            }
            else if(sym == gensym("-noloadbang")){
                x->x_lb = 0;
                ac--, av++;
            }
            else goto errstate;
        }
        else goto errstate;
    }
    x->x_width = width < BICOEFF_MINWIDTH ? BICOEFF_MINWIDTH : width;
    x->x_height = height < BICOEFF_MINHEIGHT ? BICOEFF_MINHEIGHT : height;
    if(x->x_savestate){
        x->x_type = type;
        x->x_cf = cf;
        x->x_q_s = q;
        x->x_g = g;
    }
    x->x_bg = bg_sym;
    x->x_gg = gg_sym;
    x->x_fg = fg_sym;
    x->x_bw = bicoeff_q_to_bw(x->x_q_s);
    x->x_glist = (t_glist*)canvas_getcurrent();
    x->x_cv = glist_getcanvas(x->x_glist);
    x->x_edit = x->x_glist->gl_edit;
    snprintf(x->x_tag, MAXPDSTRING, "T%lx", (long unsigned int)x);
    snprintf(x->x_my, MAXPDSTRING, "::N%lx", (long unsigned int)x);
    char buf[MAXPDSTRING];
    sprintf(buf, "#R%lx", (long unsigned int)x);
    x->x_bind_name = gensym(buf);
    pd_bind(&x->x_obj.ob_pd, x->x_bind_name);
    bicoeff_calculate(x);
    outlet_new(&x->x_obj, &s_list);
    sprintf(x->x_tag_outline, "%pOUTLINE", x);
    sprintf(x->x_tag_sel, "%pSEL", x);
    sprintf(x->x_tag_obj, "%pOBJ", x);
    sprintf(x->x_tag_bg, "%pBG", x);
    sprintf(x->x_tag_fg, "%pFG", x);
    sprintf(x->x_tag_in, "%pIN", x);
    sprintf(x->x_tag_out, "%pOUT", x);
    sprintf(x->x_tag_IO, "%pIO", x);
    sprintf(x->x_tag_graph, "responsefill%s", x->x_tag);
    sprintf(x->x_tag_graphline, "responseline%s", x->x_tag);
    sprintf(x->x_tag_zline, "zeroline%s", x->x_tag);
    sprintf(x->x_tag_lbw, "bandleft%s", x->x_tag);
    sprintf(x->x_tag_rbw, "bandright%s", x->x_tag);
    sprintf(x->x_tag_handle, "%pHANDLE", x);
    // handle
    x->x_handle = pd_new(bicoeff_handle_class);
    t_handle *sh = (t_handle *)x->x_handle;
    sh->h_master = x;
    sh->h_dragon = 0;
    sh->h_pathname[0] = 0;
    sh->h_dx = sh->h_dy = 0;
    char hbuf[64];
    sprintf(hbuf, "_h%lx", (unsigned long)sh);
    pd_bind(x->x_handle, sh->h_bindsym = gensym(hbuf));
    sprintf(sh->h_outlinetag, "h%lx", (unsigned long)sh);
    // edit proxy: bind to the canvas window to receive editmode notifications
    snprintf(buf, MAXPDSTRING-1, ".x%lx", (unsigned long)x->x_glist);
    buf[MAXPDSTRING-1] = 0;
    x->x_edit_proxy = bicoeff_edit_proxy_new(x, gensym(buf));
    return(x);
errstate:
    pd_error(x, "[bicoeff]: improper args");
    return(NULL);
}

static void bicoeff_free(t_bicoeff *x){
    pd_unbind(&x->x_obj.ob_pd, x->x_bind_name);
    if(x->x_handle){
        pd_unbind(x->x_handle, ((t_handle *)x->x_handle)->h_bindsym);
        pd_free(x->x_handle);
    }
    if(x->x_edit_proxy){
        x->x_edit_proxy->p_cnv = NULL;
        clock_delay(x->x_edit_proxy->p_clock, 0);
    }
}

void bicoeff_setup(void){
    bicoeff_class = class_new(gensym("bicoeff"), (t_newmethod)bicoeff_new,
        (t_method)bicoeff_free, sizeof(t_bicoeff), 0, A_GIMME, 0);
    bicoeff_handle_class = class_new(gensym("_bicoeff_handle"), 0, 0,
        sizeof(t_handle), CLASS_PD, 0);
    class_addmethod(bicoeff_handle_class, (t_method)handle__click_callback,
        gensym("_click"), A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(bicoeff_handle_class, (t_method)handle__motion_callback,
        gensym("_motion"), A_FLOAT, A_FLOAT, 0);
    bicoeff_edit_proxy_class = class_new(0, 0, 0,
        sizeof(t_bicoeff_edit_proxy), CLASS_NOINLET | CLASS_PD, 0);
    class_addanything(bicoeff_edit_proxy_class, bicoeff_edit_proxy_any);
    class_addbang(bicoeff_class, bicoeff_bang);
    class_addmethod(bicoeff_class, (t_method)bicoeff_dim, gensym("dim"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_allpass, gensym("allpass"), A_GIMME, 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_bandpass, gensym("bandpass"), A_GIMME, 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_highpass, gensym("highpass"), A_GIMME, 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_highshelf, gensym("highshelf"), A_GIMME, 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_lowpass, gensym("lowpass"), A_GIMME, 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_lowshelf, gensym("lowshelf"), A_GIMME, 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_notch, gensym("bandstop"), A_GIMME, 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_eq, gensym("eq"), A_GIMME, 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_resonant, gensym("resonant"), A_GIMME, 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_savestate, gensym("savestate"), A_FLOAT, 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_loadbang, gensym("loadbang"), A_DEFFLOAT, 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff__params, gensym("_params"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_setbgcolor, gensym("bgcolor"), A_GIMME, 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_setfgcolor, gensym("fgcolor"), A_GIMME, 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_setgridcolor, gensym("gridcolor"), A_GIMME, 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_colors, gensym("colors"),
        A_SYMBOL, A_SYMBOL, A_SYMBOL, 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_theme, gensym("theme"), A_FLOAT, 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_transparent, gensym("transparent"), A_FLOAT, 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_validate, gensym("_validate"),
        A_FLOAT, A_SYMBOL, A_FLOAT, 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_apply, gensym("dialog"), A_GIMME, 0);
    bicoeff_widgetbehavior.w_getrectfn  = bicoeff_getrect;
    bicoeff_widgetbehavior.w_displacefn = bicoeff_displace;
    bicoeff_widgetbehavior.w_selectfn   = bicoeff_select;
    bicoeff_widgetbehavior.w_activatefn = NULL;
    bicoeff_widgetbehavior.w_deletefn   = bicoeff_delete;
    bicoeff_widgetbehavior.w_visfn      = bicoeff_vis;
    bicoeff_widgetbehavior.w_clickfn    = NULL;
    class_setwidget(bicoeff_class, &bicoeff_widgetbehavior);
    class_setsavefn(bicoeff_class, bicoeff_save);
    class_setpropertiesfn(bicoeff_class, bicoeff_properties);
    #include "../Extra/else_tcl_utils.h"
    pdgui_vmess(else_tcl_utils, NULL);
    #include "../Extra/bicoeff_dialog.h"
    pdgui_vmess(bicoeff_dialog_tcl, NULL);
}
