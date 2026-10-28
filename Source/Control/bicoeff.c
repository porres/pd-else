// based on filterview

#include <stdio.h>

#define _USE_MATH_DEFINES
#include <math.h>

#include <stdlib.h>
#include <string.h>
#include <m_pd.h>
#include <m_imp.h>
#include <g_canvas.h>

typedef struct bicoeff{
    t_object    x_obj;
    t_glist*    x_glist;
    int         x_width;
    int         x_height;
    int         x_zoom;
    int         x_savestate;
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
    char        x_tkcanvas[MAXPDSTRING];
    char        x_tag[MAXPDSTRING];
    char        x_my[MAXPDSTRING];
}t_bicoeff;

t_class *bicoeff_class;
static t_widgetbehavior bicoeff_widgetbehavior;

// Helpers -------------------------------------------------------------------
double bicoeff_ftom(double hz){
    return(12.0 * log(hz / 110.0) / log(2.0) + 45.0);
}

double bicoeff_mtof(double midi){
    return(pow(2.0, (midi-45)/12.0)*110.0);
}

static double bicoeff_q_to_bw(double q){
    return(asinh(1.0 / (2.0 * q)) * 2.0 / log(2.0));
}

double bicoeff_bw_to_q(double bw){
    return(1.0 / (2.0 * sinh(bw * log(2.0) / 2.0)));
}

double bicoeff_get_xpos(t_bicoeff *x, double f){
    double midi_note = bicoeff_ftom(f);
    double x0 = (double)text_xpix(&x->x_obj, x->x_glist);
    double xpos = x0 + (((midi_note - 16.766) / 120.0) * (double)(x->x_width*x->x_zoom));
    return(xpos);
}

double bicoeff_get_ypos(t_bicoeff *x){
//    int y0 = text_ypix(&x->x_obj, x->x_glist);
    double ratio = (25.0 - (double)x->x_g) / 50.0;
    double ypos = (double)(x->x_height*x->x_zoom) * ratio;
    return(ypos);
}

double bicoeff_get_width(t_bicoeff *x){
    double bwf = x->x_cf * (1.0 + x->x_bw);
    double center_pos = bicoeff_get_xpos(x, x->x_cf);
    double right_pos = bicoeff_get_xpos(x, bwf);
    return(right_pos - center_pos);
}

// widgetbehavior
static void bicoeff_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2){
    t_bicoeff* x = (t_bicoeff*)z;
    *xp1 = text_xpix(&x->x_obj, glist);
    *yp1 = text_ypix(&x->x_obj, glist);
    *xp2 = text_xpix(&x->x_obj, glist) + x->x_width*x->x_zoom;
    *yp2 = text_ypix(&x->x_obj, glist) + x->x_height*x->x_zoom;
}

static void bicoeff_save(t_gobj *z, t_binbuf *b){
    t_bicoeff *x = (t_bicoeff *)z;
    binbuf_addv(b, "ssiisiisfffi", gensym("#X"),gensym("obj"),
        (int)x->x_obj.te_xpix, (int)x->x_obj.te_ypix,
        atom_getsymbol(binbuf_getvec(x->x_obj.te_binbuf)),
        x->x_width, x->x_height,
        x->x_type,
        x->x_cf, x->x_q_s, x->x_g,
        x->x_savestate);
    binbuf_addv(b, ";");
}

static void bicoeff_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_bicoeff *x = (t_bicoeff *)z;
    x->x_obj.te_xpix += dx, x->x_obj.te_ypix += dy;
    dx *= x->x_zoom, dy *= x->x_zoom;
    if(glist_isvisible(glist)){
        sys_vgui("%s move %s %d %d\n", x->x_tkcanvas, x->x_tag, dx, dy);
        sys_vgui("%s move RSZ %d %d\n", x->x_tkcanvas, dx, dy);
        canvas_fixlinesfor(glist_getcanvas(glist), (t_text*)x);
        pdgui_vmess(0, "crs ii", glist_getcanvas(glist), "move", x->x_tag_obj, dx, dy);
    }
}

static void bicoeff_select(t_gobj *z, t_glist *glist, int state){
    t_bicoeff *x = (t_bicoeff *)z;
    t_canvas *cv = glist_getcanvas(glist);
    pdgui_vmess(0, "crs rk", cv, "itemconfigure", x->x_tag_sel,
        "-outline", state ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor);
    pdgui_vmess(0, "crs rk", cv, "itemconfigure", x->x_tag_IO,
        "-fill", state ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor);
}

void bicoeff_delete(t_gobj *z, t_glist *glist){
    canvas_deletelinesfor(glist, (t_text *)z);
}

// configure colors
static void bicoeff_config_fg(t_bicoeff *x){
    t_canvas *cv = glist_getcanvas(x->x_glist);
    char fg[32];
    if(1) // x->x_theme)
        snprintf(fg, sizeof(fg), "#%06X", THISGUI->i_foregroundcolor);
    /*    else
        snprintf(fg, sizeof(fg), "%s", x->x_fg->s_name);*/
    pdgui_vmess(0, "crs rsrs", cv, "itemconfigure", x->x_tag_fg,
        "-fill", fg, "-outline", fg);
    pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_outline,
        "-outline", fg);
}

// configure colors
static void bicoeff_config_bg(t_bicoeff *x){
    t_canvas *cv = glist_getcanvas(x->x_glist);
    char bg[32];
    if(1) // x->x_theme)
        snprintf(bg, sizeof(bg), "#%06X", THISGUI->i_backgroundcolor);
    /*    else
        snprintf(bg, sizeof(bg), "%s", x->x_bg->s_name);*/
    pdgui_vmess(0, "crs rsrs", cv, "itemconfigure", x->x_tag_bg,
        "-fill", bg, "-outline", bg);
}

static void bicoeff_draw(t_bicoeff *x){
    int x1, y1, x2, y2;
    bicoeff_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
    int z = x->x_zoom;
    t_canvas *cv = glist_getcanvas(x->x_glist);
/*
// background
    char *tags_bg[] = {x->x_tag_bg, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rS", cv, "create", "rectangle",
        x1, y1, x2, y2, "-tags", 2, tags_bg);
    
// graph fill (gray)
    int mid = y1 + (x->x_height / 2);
    
    char *tags_graph[] = {x->x_tag_graph, x->x_tag_obj};
    pdgui_vmess(0, "crr iiiiiiii rs rS", cv, "create", "polygon",
        x1, mid, x2, mid, x2, y2, x1, y2,
        "-fill", "#dcdcdc", "-tags", 2, tags_graph);
    
// left/right bandwidth
    double cf = bicoeff_get_xpos(x, x->x_cf);
    double halfwidth = bicoeff_get_width(x) * 0.5;
    double filterx1 = cf - halfwidth;
    double filterx2 = cf + halfwidth;

    char bandleft[MAXPDSTRING];
    snprintf(bandleft, MAXPDSTRING, "bandleft%s", x->x_tag);
    char bandright[MAXPDSTRING];
    snprintf(bandright, MAXPDSTRING, "bandright%s", x->x_tag);
    char bandedges[MAXPDSTRING];
    snprintf(bandedges, MAXPDSTRING, "bandedges%s", x->x_tag);
    
    char *tags_lbw[] = {bandleft, bandedges, x->x_tag_lbw, x->x_tag_obj, x->x_tag};
    pdgui_vmess(0, "crr iiii rs ri rS", cv, "create", "line",
        filterx1, y1, filterx1, y2,
        "-fill", "#bbbbcc", "-width", z, "-tags", 5, tags_lbw);
    
    char *tags_rbw[] = {bandright, bandedges, x->x_tag_rbw, x->x_tag_obj, x->x_tag};
    pdgui_vmess(0, "crr iiii rs ri rS", cv, "create", "line",
        filterx2, y1, filterx2, y2,
        "-fill", "#bbbbcc", "-width", z, "-tags", 5, tags_rbw);
    */

    sys_vgui("bicoeff::drawme %s %s %s %s %d %d %d %d\n", x->x_my,
        x->x_tkcanvas, x->x_bind_name->s_name, x->x_tag, x1, y1, x2, y2);
    
    
    
/*    char *tags_zline[] = {x->x_tag_zline, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rs ri rS", cv, "create", "line",
        x1, mid, x2, mid,
        "-fill", "#bbbbcc", "-width", z, "-tags", 2, tags_zline);
    
    char *tags_graphline[] = {x->x_tag_graphline, x->x_tag_graph, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rs ri rS", cv, "create", "line",
        x1, mid, x2, mid,
        "-fill", "black", "-width", z, "-tags", 3, tags_graphline);*/
    
    
/*    int x1 = text_xpix(&x->x_obj, glist);
    int y1 = text_ypix(&x->x_obj, glist);
    int x2 = x1 + x->x_width * z;
    int y2 = y1 + x->x_heigth * z;*/
    // outline
    
    // inlet
    char *tags_in[] = {x->x_tag_in, x->x_tag_sel, x->x_tag_fg, x->x_tag_IO, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rS", cv, "create", "rectangle",
        x1, y1, x1 + IOWIDTH*z, y1 + IHEIGHT*z, "-tags", 5, tags_in);
    // outlet
    char *tags_out[] = {x->x_tag_out, x->x_tag_sel, x->x_tag_fg, x->x_tag_IO, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rS", cv, "create", "rectangle",
        x1, y2 - OHEIGHT*z, x1 + IOWIDTH*z, y2, "-tags", 5, tags_out);
    
    char *tags_outline[] = {x->x_tag_outline, x->x_tag_obj, x->x_tag_sel};
    pdgui_vmess(0, "crr iiii rk rs ri rS", cv, "create", "rectangle",
        x1, y1, x2, y2, "-outline", THISGUI->i_foregroundcolor, "-fill", "",
        "-width", z, "-tags", 3, tags_outline);
    
    bicoeff_config_bg(x);
    bicoeff_config_fg(x);
}

double bicoeff_get_mag(t_bicoeff *x, double fHz, double radians, int y0){
    float x1 = cos(-1.0*radians);
    float x2 = cos(-2.0*radians);
    float y1 = sin(-1.0*radians);
    float y2 = sin(-2.0*radians);
    float A = x->x_a0 + x->x_a1*x1 + x->x_a2*x2;
    float B = x->x_a1*y1 + x->x_a2*y2;
    float C = 1 - x->x_b1*x1 - x->x_b2*x2;
    float D = 0 - x->x_b1*y1 - x->x_b2*y2;
    float halfframeheight = (float)x->x_height/2.0;
    if(x->x_type == gensym("allpass")){ // plot phase
        float numerarg = atan2(B, A);
        float denomarg = atan2(D, C);
        float phase = numerarg-denomarg;
        // wrap phase
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
        // convert magnitude to dB scale
        float logmagnitude = 20.0*log(magnitude)/log(10);
        // clip
        if(logmagnitude > 25.0)
            logmagnitude = 25.0;
        else if(logmagnitude < -25.0)
            logmagnitude = -25.0;
        // scale to pixel range
        logmagnitude = (logmagnitude/25.0) * halfframeheight;
        // invert and offset
        logmagnitude = -1.0 * logmagnitude + halfframeheight + y0;
        return(logmagnitude);
    }
}

static void bicoeff_plot(t_bicoeff *x){
    int x1, y1, x2, y2;
    bicoeff_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
    sys_vgui("bicoeff::plot_graph %s %s %f %f %f %f %f %f %f %f\n",
        x->x_my, x->x_type->s_name,
        x->x_b1, x->x_b2, x->x_a0, x->x_a1, x->x_a2,
        bicoeff_get_ypos(x), // filtergain
        bicoeff_get_xpos(x, x->x_cf), // filtercenter
        bicoeff_get_width(x)); // filterwidth

/*
    double nyq = sys_getsr() * 0.5;
    t_canvas *cv = glist_getcanvas(x->x_glist);
    char *coordlist = NULL;
    size_t len = 0;
    for(int point = 0; point < x->x_width; point += 5){
        float midi = ((float)point / (float)x->x_width) * 120 + 16.766;
        float hz = bicoeff_mtof(midi);
        float radians = hz * M_PI / nyq;
        float mag = bicoeff_get_mag(x, hz, radians, y1);

        char tmp[64];
        int n = snprintf(tmp, sizeof(tmp), "%d %f ", point + x1, mag);

        coordlist = realloc(coordlist, len + n + 1);
        memcpy(coordlist + len, tmp, n);
        len += n;
        coordlist[len] = '\0';
    }
// nyq point
    float hz = nyq;
    float radians = M_PI;
    float mag = bicoeff_get_mag(x, hz, radians, y1);
    char tmp[64];
    int n = snprintf(tmp, sizeof(tmp), "%d %f ", x->x_width + x1, mag);
    coordlist = realloc(coordlist, len + n + 1);
    memcpy(coordlist + len, tmp, n);
    len += n;
    coordlist[len] = '\0';
    
    
    pdgui_vmess(0, "crs s", cv, "coords", x->x_tag_graphline, coordlist);
    
    // append the closing rectangle for the graph
    char tail[64];
    int tn = snprintf(tail, sizeof(tail), "%d %d %d %d", x2, y2, x1, y2);
    coordlist = realloc(coordlist, len + tn + 1);
    memcpy(coordlist + len, tail, tn);
    len += tn;
    coordlist[len] = '\0';
    pdgui_vmess(0, "crs s", cv, "coords", x->x_tag_graph, coordlist);
    
//    post("x2 %d y2 %d x1 %d y2 %d", x2, y2, x1, y2);
    

    
    
// # Draw lines at halfwidth distance from center
    double cf = bicoeff_get_xpos(x, x->x_cf);
    double halfwidth = bicoeff_get_width(x) * 0.5;
    double filterx1 = cf - halfwidth;
    double filterx2 = cf + halfwidth;
    
    char bandleft[MAXPDSTRING];
    snprintf(bandleft, MAXPDSTRING, "bandleft%s", x->x_tag);
    char bandright[MAXPDSTRING];
    snprintf(bandright, MAXPDSTRING, "bandright%s", x->x_tag);
    
//    post("filterx1 = %d / filterx2 = %d ", (int)filterx1, (int)filterx2);
    
    pdgui_vmess(0, "crs iiii", cv, "coords", bandleft, (int)filterx1, y1, (int)filterx1, y2);
    pdgui_vmess(0, "crs iiii", cv, "coords", bandright, (int)filterx2, y1, (int)filterx2, y2);
 */
}

static void bicoeff_vis(t_gobj *z, t_glist *glist, int vis){
    t_bicoeff* x = (t_bicoeff*)z;
    t_canvas *cv = glist_getcanvas(glist);
    snprintf(x->x_tkcanvas, MAXPDSTRING, ".x%lx.c", (long unsigned int)glist_getcanvas(glist));
    // send current samplerate to the GUI for calculation of biquad coeffs
    t_float samplerate = sys_getsr();
    if(samplerate > 0)  // samplerate is sometimes 0, ignore that
        sys_vgui("set ::samplerate %.0f\n", samplerate);
    if(vis){
        bicoeff_draw(x);
        bicoeff_plot(x);
    }
    else{
        sys_vgui("%s delete %s\n", x->x_tkcanvas, x->x_tag);
        pdgui_vmess(0, "crs", cv, "delete", x->x_tag_obj);
    }
}

// Methods ------------------------------------------------------------------------

static void bicoeff_bang(t_bicoeff *x){
//    post("bicoeff_bang");
    t_atom at[5];
    SETFLOAT(at, x->x_b1);
    SETFLOAT(at+1, x->x_b2);
    SETFLOAT(at+2, x->x_a0);
    SETFLOAT(at+3, x->x_a1);
    SETFLOAT(at+4, x->x_a2);
    outlet_list(x->x_obj.ob_outlet, &s_list, 5, at);
}

static void bicoeff_calculate(t_bicoeff *x){
//    post("bicoeff_calculate / type = %s", x->x_type->s_name);
    double a0 = 0, a1 = 0, a2 = 0, b1 = 0, b2 = 0;
// allpass
    if(x->x_type == gensym("allpass")){
        double omega, alphaQ, cos_w, b0;
        double hz = x->x_cf;
        double q = x->x_q_s;
        double g = 1; // pow(10, x->x_gain / 20);
        double nyq = sys_getsr() * 0.5;
        if (hz < 0.1)
            hz = 0.1;
        if (hz > nyq - 0.1)
            hz = nyq - 0.1;
        if (q < 0.000001)
            q = 0.000001; // prevent blow-up
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
        double g = 1 ; // pow(10, x->x_gain / 20);
        double nyq = sys_getsr() * 0.5;
        if (hz < 0.1)
            hz = 0.1;
        if (hz > nyq - 0.1)
            hz = nyq - 0.1;
        if (q < 0.000001)
            q = 0.000001; // prevent blow-up
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
        double g = 1 ; // pow(10, x->x_gain / 20);
        double nyq = sys_getsr() * 0.5;
        if (hz < 0.1)
            hz = 0.1;
        if (hz > nyq - 0.1)
            hz = nyq - 0.1;
        if (q < 0.000001)
            q = 0.000001; // prevent blow-up
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
            q = 0.000001; // prevent blow-up
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
        double g = 1; // pow(10, x->x_gain / 20);
        double nyq = sys_getsr() * 0.5;
        if (hz < 0.1)
            hz = 0.1;
        if (hz > nyq - 0.1)
            hz = nyq - 0.1;
        if (q < 0.000001)
            q = 0.000001; // prevent blow-up
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
        double g = 1; //pow(10, x->x_g / 20);
        double nyq = sys_getsr() * 0.5;
        if(hz < 0.1)
            hz = 0.1;
        if(hz > nyq - 0.1)
            hz = nyq - 0.1;
        if(q < 0.000001)
            q = 0.000001; // prevent blow-up
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
        double g = 1; // pow(10, x->x_gain / 20);
        double nyq = sys_getsr() * 0.5;
        if (hz < 0.1)
            hz = 0.1;
        if (hz > nyq - 0.1)
            hz = nyq - 0.1;
        if (q < 0.000001)
            q = 0.000001; // prevent blow-up
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
    if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
        bicoeff_plot(x);
    bicoeff_bang(x);
}

// callback from GUI
static void bicoeff__params(t_bicoeff *x, t_floatarg cfpix, t_floatarg bwpix, t_floatarg gpix,
t_floatarg xpos, t_floatarg ypos){
    int x0 = text_xpix(&x->x_obj, x->x_glist), y0 = text_ypix(&x->x_obj, x->x_glist);
    int fdif = (int)cfpix - x0;
    double width = (double)(x->x_width*x->x_zoom);
    double height = (double)(x->x_height*x->x_zoom);
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

static void bicoeff_dim(t_bicoeff *x, t_floatarg f1, t_floatarg f2){
    x->x_width = f1 < 200 ? 200 : (int)(f1);
    x->x_height = f2 < 100 ? 100 : (int)(f2);
    if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist)){
        sys_vgui("%s delete %s\n", x->x_tkcanvas, x->x_tag);
        snprintf(x->x_tkcanvas, MAXPDSTRING, ".x%lx.c",
            (long unsigned int)glist_getcanvas(x->x_glist));
        bicoeff_draw(x);
        bicoeff_plot(x);
    }
}

static void bicoeff_savestate(t_bicoeff *x, t_floatarg f){
    x->x_savestate = (f != 0);
}


static void bicoeff_motion(t_bicoeff *x, t_floatarg dx, t_floatarg dy, t_floatarg up){
}

static void bicoeff_key(void *z, t_symbol *keysym, t_floatarg fkey){
}

static int bicoeff_click(t_gobj *z, struct _glist *glist, int xpix, int ypix, int shift, int alt, int dbl, int doit){
    t_bicoeff *x = (t_bicoeff *)z;
    if(doit){
        
        double width = (double)(x->x_width*x->x_zoom);
        double height = (double)(x->x_height*x->x_zoom);
        
        int x0 = text_xpix(&x->x_obj, x->x_glist), y0 = text_ypix(&x->x_obj, x->x_glist);
        double fratio  = (double)(xpix - x0) / x->x_width;
        
        double nn  = fratio * 120.0 + 16.766;
        float cf = pow(2.0, (nn  - 45.0) / 12.0) * 110.0;

        float g = (double)((ypix - y0) / height) * -50.0 + 25.0;
        
//        post("cf = %g / g = %g", cf, g); // JUMP ON CLICK?
        
        glist_grab(glist, &x->x_obj.te_g, (t_glistmotionfn)(bicoeff_motion), bicoeff_key, xpix, ypix);
        bicoeff_bang(x);
    }
    return(1);
}

static void *bicoeff_new(t_symbol *s, int ac, t_atom* av){
    (void)s;
    t_bicoeff *x = (t_bicoeff *)pd_new(bicoeff_class);
    int width = 450, height = 150;
    x->x_savestate = 0;
    float cf = 1000, q = 0.5, g = 0;
    t_symbol *type = gensym("peaking");
    x->x_type = type;
    x->x_cf = cf;
    x->x_q_s = q;
    x->x_g = g;
    if(ac && av->a_type == A_FLOAT){ // 1ST Width
        int w = (int)av->a_w.w_float;
        width = w < 100 ? 100 : w; // min width is 100
        ac--, av++;
        if(ac && av->a_type == A_FLOAT){ // 2ND Height
            int h = (int)av->a_w.w_float;
            height = h < 50 ? 50 : h; // min height is 50
            ac--, av++;
            if(ac && av->a_type == A_SYMBOL){ // 3RD type
                type = av->a_w.w_symbol;
                ac--, av++;
                if(ac && av->a_type == A_FLOAT){ // 4th cf
                    cf = av->a_w.w_float;
                    ac--, av++;
                    if(ac && av->a_type == A_FLOAT){ // 5th q
                        q = av->a_w.w_float;
                        ac--, av++;
                        if(ac && av->a_type == A_FLOAT){ // 6th g
                            g = av->a_w.w_float;
                            ac--, av++;
                            if(ac && av->a_type == A_FLOAT){ // 7th savestate
                                x->x_savestate = (int)av->a_w.w_float;
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
            else if(sym == gensym("-type")){ // and params
                if(ac >= 5 && (av+1)->a_type == A_SYMBOL){
                    x->x_type = type = atom_getsymbolarg(1, ac, av);
                    x->x_cf = cf = atom_getfloatarg(2, ac, av);
                    x->x_q_s = q = atom_getfloatarg(3, ac, av);
                    x->x_g = g = atom_getfloatarg(4, ac, av);
                    ac-=5, av+=5;
                }
                else goto errstate;
            }
            else if(sym == gensym("-savestate")){
                x->x_savestate = 1;
                ac--, av++;
            }
            else goto errstate;
        }
        else goto errstate;
    }
    x->x_width = width < 200 ? 200 : width;
    x->x_height = height < 100 ? 100 : height;
    if(x->x_savestate){
        x->x_type = type;
        x->x_cf = cf;
        x->x_q_s = q;
        x->x_g = g;
    }
    x->x_bw = bicoeff_q_to_bw(x->x_q_s);
    x->x_glist = (t_glist*)canvas_getcurrent();
    x->x_zoom = x->x_glist->gl_zoom;
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
    sprintf(x->x_tag_graph, "%pGRAPH", x);
    sprintf(x->x_tag_graphline, "%pGRAPHLINE", x);
    sprintf(x->x_tag_zline, "%pZLINE", x);
    sprintf(x->x_tag_lbw, "%pRBW", x);
    sprintf(x->x_tag_rbw, "%pLBW", x);
    return(x);
errstate:
    pd_error(x, "[bicoeff]: improper args");
    return(NULL);
}

static void bicoeff_zoom(t_bicoeff *x, t_floatarg zoom){
    x->x_zoom = (int)zoom;
}

static void bicoeff_free(t_bicoeff *x){
    pd_unbind(&x->x_obj.ob_pd, x->x_bind_name);
}

void bicoeff_setup(void){
    bicoeff_class = class_new(gensym("bicoeff"), (t_newmethod)bicoeff_new,
        (t_method)bicoeff_free, sizeof(t_bicoeff), 0, A_GIMME, 0);
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
    class_addmethod(bicoeff_class, (t_method)bicoeff__params, gensym("_params"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(bicoeff_class, (t_method)bicoeff_zoom, gensym("zoom"), A_CANT, 0);
    // widget behavior
    bicoeff_widgetbehavior.w_getrectfn  = bicoeff_getrect;
    bicoeff_widgetbehavior.w_displacefn = bicoeff_displace;
    bicoeff_widgetbehavior.w_selectfn   = bicoeff_select;
    bicoeff_widgetbehavior.w_activatefn = NULL;
    bicoeff_widgetbehavior.w_deletefn   = bicoeff_delete;
    bicoeff_widgetbehavior.w_visfn      = bicoeff_vis;
    bicoeff_widgetbehavior.w_clickfn    = bicoeff_click;
    class_setwidget(bicoeff_class, &bicoeff_widgetbehavior);
    class_setsavefn(bicoeff_class, bicoeff_save);
    sys_vgui("eval [read [open {%s/bicoeff.tcl}]]\n", bicoeff_class->c_externdir->s_name);
}
