// By Tim Schoen and Porres

#include "m_pd.h"
#include "g_canvas.h"
#include "magic.h"
#include <stdlib.h>
#include <string.h>

#define MINDIGITS      1
#define MAX_NUMBOX_LEN 32
#define MINSIZE        8

#if __APPLE__
char def_font[100] = "Menlo";
#else
char def_font[100] = "DejaVu Sans Mono";
#endif

typedef struct _numbox{
    t_object  x_obj;
    t_clock  *x_clock_repaint;
    t_symbol *x_fg;
    t_symbol *x_bg;
    t_glist  *x_glist;;
    t_float   x_display;
    t_float   x_in_val;
    t_float   x_out_val;
    t_float   x_set_val;
    t_float   x_last_out;
    t_float   x_maximum;
    t_float   x_minimum;
    t_float   x_sr_khz;
    t_float   x_ramp_step;
    t_float   x_ramp_val;
    int       x_ramp_ms;
    int       x_change;
    int       x_finemoved;
    int       x_height;
    int       x_width;
    int       x_fontsize;
    int       x_selected;
    int       x_rate;
    int       x_zoom;
    int       x_numwidth;
    int       x_outmode;
    char      x_buf[MAX_NUMBOX_LEN]; // number buffer
}t_numbox;

static void numbox_dialog_init(void);
t_widgetbehavior numbox_widgetbehavior;
static t_class *numbox_class;

// // // // // // // // helper functions // // // // // // // // // // //
char *set_x_buf(t_numbox *x){
    sprintf(x->x_buf, "~%g", x->x_display = x->x_outmode ? x->x_set_val : x->x_in_val);
    int bufsize = (int)strlen(x->x_buf), i, e;
    int real_numwidth = x->x_numwidth + 1;
    if(bufsize > real_numwidth){ // reduce
        for(i = 0; i < bufsize; i++)
            if(x->x_buf[i] == '.')
                break;
        for(e = 0; e < bufsize; e++)
            if(x->x_buf[e] == 'e' || x->x_buf[e] == 'E')
                break;
        if(i >= real_numwidth || e < bufsize)
            x->x_buf[real_numwidth-1] = '>';
        x->x_buf[real_numwidth] = 0;
    }
    return(x->x_buf);
}

static void numbox_draw_update(t_gobj *client, t_glist *glist){ // update number value
    t_numbox *x = (t_numbox *)client;
    if(glist_isvisible(glist) && gobj_shouldvis((t_gobj *)x, glist)){
        t_canvas *cv = glist_getcanvas(glist);
        if(x->x_change && x->x_buf[0] && x->x_outmode){ // keyboard input values
            char *cp = x->x_buf;
            int sl = (int)strlen(x->x_buf);
            x->x_buf[sl] = '>';
            x->x_buf[sl+1] = 0;
            if(sl >= (x->x_numwidth + 1))
                cp += sl - x->x_numwidth + 1;
            sys_vgui(".x%lx.c itemconfigure %lxNUM -text {%s}\n", cv, x, cp);
            x->x_buf[sl] = 0;
        }
        else{ // plain update
            sys_vgui(".x%lx.c itemconfigure %lxNUM -text {%s}\n", cv, x, set_x_buf(x));
            if(x->x_selected){
                sys_vgui(".x%lx.c itemconfigure %lxNUM -fill blue\n", cv, x);
                sys_vgui(".x%lx.c itemconfigure %lxBASE -outline blue\n", cv, x);
            }
            else{
                sys_vgui(".x%lx.c itemconfigure %lxNUM -fill %s\n", cv, x, x->x_fg->s_name);
                sys_vgui(".x%lx.c itemconfigure %lxBASE -outline black\n", cv, x);
            }
            x->x_buf[0] = 0;
        }
    }
}

static void clock_update(t_numbox *x){
    if(x->x_display != (x->x_outmode ? x->x_set_val : x->x_in_val)) // Check if changed
        numbox_draw_update(&x->x_obj.te_g, x->x_glist);
    clock_delay(x->x_clock_repaint, x->x_rate);
}

static void numbox_width_calc(t_numbox *x){
    x->x_width = (x->x_fontsize - (x->x_fontsize/2)+2) * (x->x_numwidth+2) + 2;
}

static void numbox_draw_new(t_numbox *x, t_glist *glist){
    int xpos = text_xpix(&x->x_obj, glist), ypos = text_ypix(&x->x_obj, glist);
    int zoom = x->x_zoom, size = x->x_fontsize;
    int w = x->x_width, h = x->x_height;
    int half = h/2, d = zoom + h/(34*zoom); // ???
    int iow = IOWIDTH * zoom, ioh = 3*zoom; // why not ioheight??
    t_canvas *cv = glist_getcanvas(glist);
    sys_vgui(".x%lx.c create rectangle %d %d %d %d -width %d -outline black -fill %s -tags [list %lxBASE %lxALL]\n",
        cv, xpos, ypos, xpos+w, ypos+h, zoom, x->x_bg->s_name, x, x);
    sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags [list %lxIN %lxALL]\n",
        cv, xpos, ypos, xpos+iow, ypos-zoom+ioh, x, x);
    sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags [list %lxOUT %lxALL]\n",
        cv, xpos, ypos+h+zoom-ioh, xpos+iow, ypos+h, x, x);
    sys_vgui(".x%lx.c create text %d %d -text {%s} -anchor w -font {{%s} -%d} -fill %s -tags [list %lxNUM %lxALL]\n",
        cv, xpos+2*zoom, ypos+half+d, set_x_buf(x), def_font, size*zoom, x->x_fg->s_name, x, x, x);
}

static void numbox_resize(t_numbox *x, t_glist *glist){
    int xpos = text_xpix(&x->x_obj, glist), ypos = text_ypix(&x->x_obj, glist);
    sys_vgui(".x%lx.c coords %lxBASE %d %d %d %d\n", glist_getcanvas(glist), x,
        xpos, ypos, xpos+x->x_width, ypos+x->x_height);
}

static void numbox_delete(t_gobj *z, t_glist *glist){
    canvas_deletelinesfor(glist, (t_text *)z);
}

static void numbox_select(t_gobj *z, t_glist *glist, int sel){
    t_numbox *x = (t_numbox *)z;
    t_canvas *cv = glist_getcanvas(glist);
    if((x->x_selected = sel)){
        sys_vgui(".x%lx.c itemconfigure %lxNUM -fill blue\n", cv, x);
        sys_vgui(".x%lx.c itemconfigure %lxBASE -outline blue\n", cv, x);
    }
    else{
        sys_vgui(".x%lx.c itemconfigure %lxNUM -fill %s\n", cv, x, x->x_fg->s_name);
        sys_vgui(".x%lx.c itemconfigure %lxBASE -outline black\n", cv, x);
    }
}

static void numbox_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_numbox *x = (t_numbox *)z;
    x->x_obj.te_xpix += dx, x->x_obj.te_ypix += dy;
    sys_vgui(".x%lx.c move %lxALL %d %d\n", glist_getcanvas(glist), x, dx*x->x_zoom, dy*x->x_zoom);
    canvas_fixlinesfor(glist, (t_text*)x);
}

static void numbox_draw_erase(t_numbox* x, t_glist* glist){
    sys_vgui(".x%lx.c delete %lxALL\n", glist_getcanvas(glist), x);
}

void numbox_vis(t_gobj *z, t_glist *glist, int vis){
    t_numbox* x = (t_numbox*)z;
    if(vis)
        numbox_draw_new(x, glist);
    else{
        numbox_draw_erase(x, glist);
        sys_unqueuegui(z); // why unqueue?
    }
}

// ------------------------ Methods-----------------------------
static void numbox_width(t_numbox *x, t_floatarg f){
    int width = f < MINDIGITS ? MINDIGITS : (int)f;
    if(x->x_numwidth != width){
        x->x_numwidth = width;
        numbox_width_calc(x);
        numbox_resize(x, x->x_glist);
        sys_queuegui(x, x->x_glist, numbox_draw_update);
    }
}

static void numbox_size(t_numbox *x, t_floatarg f){
    int size = f < MINSIZE ? MINSIZE : (int)f;
    if(x->x_fontsize != size){
        x->x_fontsize = size;
        int oldheight = x->x_height;
        x->x_height = size + 4;
        numbox_width_calc(x);
        numbox_resize(x, x->x_glist);
        int dy = (x->x_height - oldheight);
        t_canvas *cv = glist_getcanvas(x->x_glist);
        sys_vgui(".x%lx.c itemconfigure %lxNUM -font {{%s} -%d}\n", cv, x, def_font, x->x_fontsize);
        sys_vgui(".x%lx.c moveto %lxNUM %d %d\n", cv, x, x->x_obj.te_xpix, x->x_obj.te_ypix+2);
        sys_vgui(".x%lx.c move %lxOUT 0 %d\n", cv, x, dy);
        canvas_fixlinesfor(x->x_glist, (t_text*)x);
    }
}

static void numbox_rate(t_numbox *x, t_floatarg f){
    x->x_rate = f < 15 ? 15 : (int)f;
}

static void numbox_ramp(t_numbox *x, t_floatarg f){
    x->x_ramp_ms = f < 0 ? 0 : (int)f;
}

static void numbox_float(t_numbox *x, t_floatarg f){ // set float value and update GUI
    t_float ftocompare = f;
    if(memcmp(&ftocompare, &x->x_out_val, sizeof(ftocompare))){ // bitwise comparison
        x->x_set_val = ftocompare;
        numbox_draw_update(&x->x_obj.te_g, x->x_glist);
        if(x->x_outmode){
            x->x_out_val = x->x_set_val;
            if(x->x_ramp_ms > 0)
                x->x_ramp_step = (x->x_out_val - x->x_ramp_val) / (x->x_ramp_ms * x->x_sr_khz);
        }
    }
}

static void numbox_bg(t_numbox *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_symbol *bg = NULL;
    if(ac == 1 && av->a_type == A_SYMBOL)
        bg = atom_getsymbolarg(0, ac, av);
    if(x->x_bg != bg && bg != NULL){
        x->x_bg = bg;
        sys_vgui(".x%lx.c itemconfigure %lxBASE -fill %s\n", glist_getcanvas(x->x_glist), x, x->x_bg->s_name);
    }
//    else if(ac == 3 && av->a_type == A_SYMBOL)
}

static void numbox_fg(t_numbox *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_symbol *fg = NULL;
    if(ac == 1 && av->a_type == A_SYMBOL)
        fg = atom_getsymbolarg(0, ac, av);
    if(x->x_fg != fg && fg != NULL){
        x->x_fg = fg;
        sys_vgui(".x%lx.c itemconfigure %lxNUM -fill %s\n", glist_getcanvas(x->x_glist), x, x->x_fg->s_name);
    }
//    else if(ac == 3 && av->a_type == A_SYMBOL)
}

static void numbox_range(t_numbox *x, t_floatarg f1, t_floatarg f2){
    if(x->x_minimum == f1 && x->x_maximum == f2)
        return;
    float temp = f1;
    if(f2 < f1){
        f1 = f2;
        f2 = temp;
    }
    x->x_minimum = f1, x->x_maximum = f2;
}

// ------------------------ widgetbehaviour-----------------------------
static void numbox_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2){
    t_numbox* x = (t_numbox*)z;
    *xp1 = text_xpix(&x->x_obj, glist);
    *yp1 = text_ypix(&x->x_obj, glist);
    *xp2 = *xp1 + x->x_width;
    *yp2 = *yp1 + x->x_height;
}

static void numbox_save(t_gobj *z, t_binbuf *b){
    t_numbox *x = (t_numbox *)z;
    if(x->x_change){ // ??????????????? why????
        x->x_change = 0;
        sys_queuegui(x, x->x_glist, numbox_draw_update);
    }
    binbuf_addv(b, "ssiisiiissiff", gensym("#X"), gensym("obj"), (int)x->x_obj.te_xpix,
        (int)x->x_obj.te_ypix, gensym("numbox~"), x->x_numwidth, x->x_fontsize,
        x->x_rate, x->x_bg, x->x_fg, x->x_ramp_ms, x->x_minimum, x->x_maximum);
    binbuf_addv(b, ";");
}

static void numbox_properties(t_gobj *z, t_glist *owner){ // called in right click for propeties
    owner = NULL; // not used for some reason, avoid warning
    t_numbox *x = (t_numbox *)z;
    numbox_dialog_init();  // initialising dialog doesn't seem to work in setup...
    if(x->x_change){ // why??
        x->x_change = 0;
        sys_queuegui(x, x->x_glist, numbox_draw_update);
    }
    char buf[800];
    sprintf(buf, "::dialog_numbox::pdtk_numbox_dialog %%s -------dimensions(digits)(pix):------- \
        %d %d %d %d %d %d %s %s %.1f %.1f\n", x->x_numwidth, MINDIGITS, x->x_fontsize, MINSIZE,
        x->x_ramp_ms, x->x_rate, x->x_bg->s_name, x->x_fg->s_name, x->x_minimum, x->x_maximum);
    gfxstub_new(&x->x_obj.ob_pd, x, buf); // no idea what this does...
}

static void numbox_motion(t_numbox *x, t_floatarg dx, t_floatarg dy, t_floatarg up){
    dx = 0; // avoid warning
    if(up != 0) // 0 ???
        return;
    float val = x->x_out_val - dy*(x->x_finemoved ? 0.01 : 1);
    if(x->x_minimum != 0 || x->x_maximum != 0) // clip
        val = val < x->x_minimum ? x->x_minimum : val > x->x_maximum ? x->x_maximum : val;
    numbox_float(x, val);
    sys_queuegui(x, x->x_glist, numbox_draw_update); // why queing?? set already updates
}

static void numbox_key(void *z, t_symbol *keysym, t_floatarg fkey){
    keysym = NULL; // unused, avoid warning
    t_numbox *x = z;
    char c = fkey, buf[3];
    buf[1] = 0;
    if(c == 0){ // click out
        x->x_change = 0;
        sys_queuegui(x, x->x_glist, numbox_draw_update);
        sys_vgui(".x%lx.c itemconfigure %lxBASE -width %d\n", glist_getcanvas(x->x_glist), x, x->x_zoom);
        return;
    }
    if(((c >= '0') && (c <= '9')) || (c == '.') || (c == '-') ||
    (c == 'e') || (c == '+') || (c == 'E')){ // number characters
        if(strlen(x->x_buf) < (MAX_NUMBOX_LEN-2)){
            buf[0] = c;
            strcat(x->x_buf, buf);
            sys_queuegui(x, x->x_glist, numbox_draw_update);
        }
    }
    else if((c == '\b') || (c == 127)){ // backspace / delete
        int sl = (int)strlen(x->x_buf) - 1;
        if(sl < 0)
            sl = 0;
        x->x_buf[sl] = 0;
        sys_queuegui(x, x->x_glist, numbox_draw_update);
    }
    else if(((c == '\n') || (c == 13)) && x->x_buf[0] != 0){ // enter
        numbox_float(x, atof(x->x_buf)); // atof converts string to float
        x->x_buf[0] = 0;
        sys_queuegui(x, x->x_glist, numbox_draw_update); // not needed?? set already updates
    }
}

static void numbox_click(t_numbox *x, t_floatarg xpos, t_floatarg ypos, t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    shift = ctrl =alt = 0; // unused - avoid warning
    if(x->x_outmode)
        glist_grab(x->x_glist, &x->x_obj.te_g, (t_glistmotionfn)numbox_motion, numbox_key, xpos, ypos);
}

static int numbox_newclick(t_gobj *z, struct _glist *glist, int xpix, int ypix, int shift, int alt, int dbl, int doit){
    glist = NULL; // unused, avoid warning
    dbl = 0;
    t_numbox* x = (t_numbox *)z;
    if(doit && x->x_outmode){
        sys_vgui(".x%lx.c itemconfigure %lxBASE -width %d\n", glist_getcanvas(x->x_glist), x, x->x_zoom*2);
        numbox_click(x, (t_floatarg)xpix, (t_floatarg)ypix, (t_floatarg)shift, 0, (t_floatarg)alt);
        x->x_finemoved = shift;
        if(!x->x_change){
            x->x_change = 1;
            x->x_buf[0] = 0;
        } else post("[numbox~] bug: clicked when armed");
    }
    return(1);
}

// Dialog
static void numbox_dialog(t_numbox *x, t_symbol *s, int ac, t_atom *av){
    s = NULL; // we receive this when applying changes in properties
    int width = atom_getintarg(0, ac, av);
    int size = atom_getintarg(1, ac, av);
    int ramp_ms = atom_getintarg(2, ac, av);
    int rate = atom_getintarg(3, ac, av);
    t_symbol *bgcolor = atom_getsymbolarg(4, ac, av);
    t_symbol *fgcolor = atom_getsymbolarg(5, ac, av);
    t_float min = atom_getfloatarg(6, ac, av);
    t_float max = atom_getfloatarg(7, ac, av);
    t_atom undo[9];
    SETFLOAT(undo+0, x->x_numwidth);
    SETFLOAT(undo+1, x->x_fontsize);
    SETFLOAT(undo+2, 0); // why?????????????????
    SETFLOAT(undo+3, x->x_rate);
    SETSYMBOL(undo+4, x->x_bg);
    SETSYMBOL(undo+5, x->x_fg);
    SETFLOAT(undo+6, x->x_ramp_ms);
    SETFLOAT(undo+7, x->x_minimum);
    SETFLOAT(undo+8, x->x_maximum);
    pd_undo_set_objectstate(x->x_glist, (t_pd*)x, gensym("dialog"), 9, undo, ac, av);
    numbox_ramp(x, ramp_ms);
    numbox_rate(x, rate);
    numbox_range(x, min, max);
    t_atom at[1];
    SETSYMBOL(at, bgcolor);
    numbox_bg(x, NULL, 1, at);
    SETSYMBOL(at, fgcolor);
    numbox_fg(x, NULL, 1, at);
    numbox_width(x, width);
    numbox_size(x, size);
    canvas_fixlinesfor(x->x_glist, (t_text*)x);
}

static void *numbox_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_numbox *x = (t_numbox *)pd_new(numbox_class);;
    x->x_glist = (t_glist *)canvas_getcurrent();
    x->x_zoom = x->x_glist->gl_zoom;
    x->x_in_val = x->x_out_val = x->x_set_val = 0.0;
    x->x_buf[0] = 0; // ??
    x->x_change = 0;
    int width = 4, size = glist_getfont(x->x_glist);
    int rate = 100, ramp_ms = 10;
    x->x_outmode = 1;
    t_float minimum = 0, maximum = 0;
    x->x_bg = gensym("#dfdfdf");
    x->x_fg = gensym("#000000");
    if(ac == 8){
        width = atom_getintarg(0, ac, av);
        size = atom_getintarg(1, ac, av);
        rate = atom_getintarg(2, ac, av);
        x->x_bg = atom_getsymbolarg(3, ac, av);
        x->x_fg = atom_getsymbolarg(4, ac, av);
        ramp_ms = atom_getintarg(5, ac, av);
        minimum = atom_getfloatarg(6, ac, av);
        maximum = atom_getfloatarg(7, ac, av);
    }
    x->x_numwidth = width < MINDIGITS ? MINDIGITS : width;
    x->x_fontsize = size < MINSIZE ? MINSIZE : size;
    numbox_width_calc(x);
    x->x_height = x->x_fontsize + 4;
    x->x_minimum = minimum, x->x_maximum = maximum;
    x->x_rate = rate < 15 ? 15 : (int)rate;
    x->x_ramp_ms = ramp_ms;
    x->x_clock_repaint = clock_new(x, (t_method)clock_update);
    clock_delay(x->x_clock_repaint, x->x_rate); // Start repaint clock
    outlet_new(&x->x_obj,  &s_signal);
    return(x);
}

static void numbox_zoom(t_numbox *x, t_floatarg zoom){
    float mul = (x->x_zoom = (int)zoom) == 1.0 ? 0.5 : 2.0;
    x->x_width = (int)((float)x->x_width * mul);
    x->x_height = (int)((float)x->x_height * mul);
}

static void numbox_free(t_numbox *x){
    clock_free(x->x_clock_repaint);
    gfxstub_deleteforkey(x);
}

static t_int *numbox_perform_output(t_int *w){
    t_numbox *x = (t_numbox *)(w[1]);
    t_sample *out     = (t_sample *)(w[3]);
    t_int n           = (t_int)(w[4]);
    if(x->x_ramp_val != x->x_out_val && x->x_ramp_ms != 0){  // apply a ramp
        for(int i = 0; i < n; i++){
            if((x->x_ramp_step < 0 && x->x_ramp_val <= x->x_out_val) ||
            (x->x_ramp_step > 0 && x->x_ramp_val >= x->x_out_val)){ // Check if we reached our destination
                x->x_ramp_val = x->x_out_val;
                x->x_ramp_step = 0;
            }
            x->x_ramp_val += x->x_ramp_step;
            out[i] = x->x_ramp_val;
            x->x_last_out = x->x_ramp_val;
        }
    }
    else for(int i = 0; i < n; i++) // No ramp needed
        out[i] = x->x_out_val;
    return(w+5);
}

static t_int *numbox_perform(t_int *w){
    t_numbox *x   = (t_numbox *)(w[1]);
    t_sample *in  = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    t_int n       = (t_int)(w[4]);
    x->x_in_val   = in[0];
    for(int i = 0; i < n; i++)
        out[i] = in[i];
    return(w+5);
}

static void numbox_dsp(t_numbox *x, t_signal **sp){
    x->x_outmode = !magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    if(x->x_outmode)
        dsp_add(numbox_perform_output, 4, x, sp[0]->s_vec, sp[1]->s_vec, (t_int)sp[0]->s_n);
    else
        dsp_add(numbox_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, (t_int)sp[0]->s_n);
}

static void numbox_dialog_init(void){ // Tcl/Tk properties code based on IEMGUI's
    sys_gui("\n"
            "package provide dialog_numbox 0.1\n"
            "namespace eval ::dialog_numbox:: {\n"
            "    variable define_min_fontsize 4\n"
            "\n"
            "    namespace export pdtk_elsegui_dialog\n"
            "}\n"
            "\n"
            "\n"
            "proc ::dialog_numbox::clip_dim {mytoplevel} {\n"
            "    set vid [string trimleft $mytoplevel .]\n"
            "\n"
            "    set var_elsegui_wdt [concat elsegui_wdt_$vid]\n"
            "    global $var_elsegui_wdt\n"
            "    set var_elsegui_min_wdt [concat elsegui_min_wdt_$vid]\n"
            "    global $var_elsegui_min_wdt\n"
            "    set var_elsegui_hgt [concat elsegui_hgt_$vid]\n"
            "    global $var_elsegui_hgt\n"
            "    set var_elsegui_min_hgt [concat elsegui_min_hgt_$vid]\n"
            "    global $var_elsegui_min_hgt\n"
            "\n"
            "    if {[eval concat $$var_elsegui_wdt] < [eval concat $$var_elsegui_min_wdt]} {\n"
            "        set $var_elsegui_wdt [eval concat $$var_elsegui_min_wdt]\n"
            "        $mytoplevel.dim.w_ent configure -textvariable $var_elsegui_wdt\n"
            "    }\n"
            "    if {[eval concat $$var_elsegui_hgt] < [eval concat $$var_elsegui_min_hgt]} {\n"
            "        set $var_elsegui_hgt [eval concat $$var_elsegui_min_hgt]\n"
            "        $mytoplevel.dim.h_ent configure -textvariable $var_elsegui_hgt\n"
            "    }\n"
            "}\n"
            "proc ::dialog_numbox::set_col_example {mytoplevel} {\n"
            "    set vid [string trimleft $mytoplevel .]\n"
            "\n"
            "    set var_elsegui_l2_f1_b0 [concat elsegui_l2_f1_b0_$vid]\n"
            "    global $var_elsegui_l2_f1_b0\n"
            "    set var_elsegui_bcol [concat elsegui_bcol_$vid]\n"
            "    global $var_elsegui_bcol\n"
            "    set var_elsegui_fcol [concat elsegui_fcol_$vid]\n"
            "    global $var_elsegui_fcol\n"
            "\n"
            "    if { [eval concat $$var_elsegui_fcol] ne \"none\" } {\n"
            "        $mytoplevel.colors.sections.exp.fr_bk configure \\\n"
            "            -background [eval concat $$var_elsegui_bcol] \\\n"
            "            -activebackground [eval concat $$var_elsegui_bcol] \\\n"
            "            -foreground [eval concat $$var_elsegui_fcol] \\\n"
            "            -activeforeground [eval concat $$var_elsegui_fcol]\n"
            "    } else {\n"
            "        $mytoplevel.colors.sections.exp.fr_bk configure \\\n"
            "            -background [eval concat $$var_elsegui_bcol] \\\n"
            "            -activebackground [eval concat $$var_elsegui_bcol] \\\n"
            "            -foreground [eval concat $$var_elsegui_bcol] \\\n"
            "            -activeforeground [eval concat $$var_elsegui_bcol]}\n"
            "\n"
            "    # for OSX live updates\n"
            "    if {$::windowingsystem eq \"aqua\"} {\n"
            "        ::dialog_numbox::apply_and_rebind_return $mytoplevel\n"
            "    }\n"
            "}\n"
            "\n"
            "proc ::dialog_numbox::preset_col {mytoplevel presetcol} {\n"
            "    set vid [string trimleft $mytoplevel .]\n"
            "    set var_elsegui_l2_f1_b0 [concat elsegui_l2_f1_b0_$vid]\n"
            "    global $var_elsegui_l2_f1_b0\n"
            "\n"
            "    set var_elsegui_bcol [concat elsegui_bcol_$vid]\n"
            "    global $var_elsegui_bcol\n"
            "    set var_elsegui_fcol [concat elsegui_fcol_$vid]\n"
            "    global $var_elsegui_fcol\n"
            "\n"
            "    if { [eval concat $$var_elsegui_l2_f1_b0] == 0 } { set $var_elsegui_bcol $presetcol }\n"
            "    if { [eval concat $$var_elsegui_l2_f1_b0] == 1 } { set $var_elsegui_fcol $presetcol }\n"
            "    ::dialog_numbox::set_col_example $mytoplevel\n"
            "}\n"
            "\n"
            "proc ::dialog_numbox::choose_col_bkfrlb {mytoplevel} {\n"
            "    set vid [string trimleft $mytoplevel .]\n"
            "\n"
            "    set var_elsegui_l2_f1_b0 [concat elsegui_l2_f1_b0_$vid]\n"
            "    global $var_elsegui_l2_f1_b0\n"
            "    set var_elsegui_bcol [concat elsegui_bcol_$vid]\n"
            "    global $var_elsegui_bcol\n"
            "    set var_elsegui_fcol [concat elsegui_fcol_$vid]\n"
            "    global $var_elsegui_fcol\n"
            "\n"
            "    if {[eval concat $$var_elsegui_l2_f1_b0] == 0} {\n"
            "        set $var_elsegui_bcol [eval concat $$var_elsegui_bcol]\n"
            "        set helpstring [tk_chooseColor -title [_ \"Background color\"] -initialcolor [eval concat $$var_elsegui_bcol]]\n"
            "        if { $helpstring ne \"\" } {\n"
            "            set $var_elsegui_bcol $helpstring }\n"
            "    }\n"
            "    if {[eval concat $$var_elsegui_l2_f1_b0] == 1} {\n"
            "        set $var_elsegui_fcol [eval concat $$var_elsegui_fcol]\n"
            "        set helpstring [tk_chooseColor -title [_ \"Foreground color\"] -initialcolor [eval concat $$var_elsegui_fcol]]\n"
            "        if { $helpstring ne \"\" } {\n"
            "            set $var_elsegui_fcol $helpstring }\n"
            "    }\n"
            "    ::dialog_numbox::set_col_example $mytoplevel\n"
            "}\n"
            "\n"
            "\n"
            "\n"
            "\n"
            "proc ::dialog_numbox::ramp {mytoplevel} {\n"
            "    set vid [string trimleft $mytoplevel .]\n"
            "\n"
            "    set var_elsegui_ramp [concat elsegui_ramp_$vid]\n"
            "    global $var_elsegui_ramp\n"
            "\n"
            "    if {[eval concat $$var_elsegui_ramp]} {\n"
            "        set $var_elsegui_ramp 0\n"
            "        $mytoplevel.para.ramp configure -text [_ \"Input\"]\n"
            "    } else {\n"
            "        set $var_elsegui_ramp 1\n"
            "        $mytoplevel.para.ramp configure -text [_ \"Output\"]\n"
            "    }\n"
            "}\n"
            "\n"
            "proc ::dialog_numbox::apply {mytoplevel} {\n"
            "    set vid [string trimleft $mytoplevel .]\n"
            "\n"
            "    set var_elsegui_wdt [concat elsegui_wdt_$vid]\n"
            "    global $var_elsegui_wdt\n"
            "    set var_elsegui_min_wdt [concat elsegui_min_wdt_$vid]\n"
            "    global $var_elsegui_min_wdt\n"
            "    set var_elsegui_hgt [concat elsegui_hgt_$vid]\n"
            "    global $var_elsegui_hgt\n"
            "    set var_elsegui_min_hgt [concat elsegui_min_hgt_$vid]\n"
            "    global $var_elsegui_min_hgt\n"
            "    set var_elsegui_interval [concat elsegui_interval_$vid]\n"
            "    global $var_elsegui_interval\n"
            "    set var_elsegui_ramp [concat elsegui_ramp_$vid]\n"
            "    global $var_elsegui_ramp\n"
            "    set var_elsegui_bcol [concat elsegui_bcol_$vid]\n"
            "    global $var_elsegui_bcol\n"
            "    set var_elsegui_fcol [concat elsegui_fcol_$vid]\n"
            "    global $var_elsegui_fcol\n"
            "    set var_elsegui_min_rng [concat elsegui_min_rng_$vid]\n"
            "    global $var_elsegui_min_rng\n"
            "    set var_elsegui_max_rng [concat elsegui_max_rng_$vid]\n"
            "    global $var_elsegui_max_rng\n"
            "\n"
            "    ::dialog_numbox::clip_dim $mytoplevel\n"
            "\n"
            "\n"
            "\n"
            "\n"
// send "dialog" message to pd
            "    pdsend [concat $mytoplevel dialog \\\n"
            "            [eval concat $$var_elsegui_wdt] \\\n"
            "            [eval concat $$var_elsegui_hgt] \\\n"
            "            [eval concat $$var_elsegui_ramp] \\\n"
            "            [eval concat $$var_elsegui_interval] \\\n"
            "            [string tolower [eval concat $$var_elsegui_bcol]] \\\n"
            "            [string tolower [eval concat $$var_elsegui_fcol]] \\\n"
            "            [eval concat $$var_elsegui_min_rng] \\\n"
            "            [eval concat $$var_elsegui_max_rng]] \\\n"
            "}\n"
            "\n"
            "\n"
            "proc ::dialog_numbox::cancel {mytoplevel} {\n"
            "    pdsend \"$mytoplevel cancel\"\n"
            "}\n"
            "\n"
            "proc ::dialog_numbox::ok {mytoplevel} {\n"
            "    ::dialog_numbox::apply $mytoplevel\n"
            "    ::dialog_numbox::cancel $mytoplevel\n"
            "}\n"
            "\n"
            "proc ::dialog_numbox::pdtk_numbox_dialog {mytoplevel dim_header \\\n"
            "                                       wdt min_wdt \\\n"
            "                                       hgt min_hgt \\\n"
            "                                       ramp interval \\\n"
            "                                       bcol fcol min_rng max_rng} {\n"
            "\n"
            "    set vid [string trimleft $mytoplevel .]\n"
            "\n"
            "    set var_elsegui_wdt [concat elsegui_wdt_$vid]\n"
            "    global $var_elsegui_wdt\n"
            "    set var_elsegui_min_wdt [concat elsegui_min_wdt_$vid]\n"
            "    global $var_elsegui_min_wdt\n"
            "    set var_elsegui_hgt [concat elsegui_hgt_$vid]\n"
            "    global $var_elsegui_hgt\n"
            "    set var_elsegui_min_hgt [concat elsegui_min_hgt_$vid]\n"
            "    global $var_elsegui_min_hgt\n"
            "    set var_elsegui_interval [concat elsegui_interval_$vid]\n"
            "    global $var_elsegui_interval\n"
            "    set var_elsegui_ramp [concat elsegui_ramp_$vid]\n"
            "    global $var_elsegui_ramp\n"
            "    set var_elsegui_bcol [concat elsegui_bcol_$vid]\n"
            "    global $var_elsegui_bcol\n"
            "    set var_elsegui_fcol [concat elsegui_fcol_$vid]\n"
            "    global $var_elsegui_fcol\n"
            "    set var_elsegui_l2_f1_b0 [concat elsegui_l2_f1_b0_$vid]\n"
            "    global $var_elsegui_l2_f1_b0\n"
            "    set var_elsegui_min_rng [concat elsegui_min_rng_$vid]\n"
            "    global $var_elsegui_min_rng\n"
            "    set var_elsegui_max_rng [concat elsegui_max_rng_$vid]\n"
            "    global $var_elsegui_max_rng\n"
            "\n"
            "    set $var_elsegui_wdt $wdt\n"
            "    set $var_elsegui_min_wdt $min_wdt\n"
            "    set $var_elsegui_hgt $hgt\n"
            "    set $var_elsegui_min_hgt $min_hgt\n"
            "    set $var_elsegui_interval $interval\n"
            "    set $var_elsegui_ramp $ramp\n"
            "    set $var_elsegui_bcol $bcol\n"
            "    set $var_elsegui_fcol $fcol\n"
            "    set $var_elsegui_min_rng $min_rng\n"
            "    set $var_elsegui_max_rng $max_rng\n"
            "\n"
            "    set $var_elsegui_l2_f1_b0 0\n"
            "\n"
            "    set elsegui_type [_ \"numbox~\"]\n"
            "    set wdt_label [_ \"Width (digits):\"]\n"
            "    set hgt_label [_ \"Font Size:\"]\n"
            "    toplevel $mytoplevel -class DialogWindow\n"
            "    wm title $mytoplevel [format [_ \"%s Properties\"] $elsegui_type]\n"
            "    wm group $mytoplevel .\n"
            "    wm resizable $mytoplevel 0 0\n"
            "    wm transient $mytoplevel $::focused_window\n"
            "    $mytoplevel configure -menu $::dialog_menubar\n"
            "    $mytoplevel configure -padx 0 -pady 0\n"
            "    ::pd_bindings::dialog_bindings $mytoplevel \"elsegui\"\n"
            "\n"
            "    # dimensions\n"
            "    frame $mytoplevel.dim -height 7\n"
            "    pack $mytoplevel.dim -side top\n"
            "    label $mytoplevel.dim.w_lab -text [_ $wdt_label]\n"
            "    entry $mytoplevel.dim.w_ent -textvariable $var_elsegui_wdt -width 4\n"
            "    label $mytoplevel.dim.dummy1 -text \"\" -width 1\n"
            "    label $mytoplevel.dim.h_lab -text [_ $hgt_label]\n"
            "    entry $mytoplevel.dim.h_ent -textvariable $var_elsegui_hgt -width 4\n"
            "    pack $mytoplevel.dim.w_lab $mytoplevel.dim.w_ent -side left\n"
            "    if { $hgt_label ne \"empty\" } {\n"
            "        pack $mytoplevel.dim.dummy1 $mytoplevel.dim.h_lab $mytoplevel.dim.h_ent -side left }\n"
            "\n"
            "    # range\n"
            "    labelframe $mytoplevel.rng\n"
            "    pack $mytoplevel.rng -side top -fill x\n"
            "    frame $mytoplevel.rng.min\n"
            "    label $mytoplevel.rng.min.lab -text \"Minimum\"\n"
            "    entry $mytoplevel.rng.min.ent -textvariable $var_elsegui_min_rng -width 7\n"
            "    label $mytoplevel.rng.dummy1 -text \"\" -width 1\n"
            "    label $mytoplevel.rng.max_lab -text \"Maximum\"\n"
            "    entry $mytoplevel.rng.max_ent -textvariable $var_elsegui_max_rng -width 7\n"
            "    $mytoplevel.rng config -borderwidth 1 -pady 4 -text \"Output Range\"\n"
     
            "    pack $mytoplevel.rng.min\n"
            "    pack $mytoplevel.rng.min.lab $mytoplevel.rng.min.ent -side left\n"
            "    $mytoplevel.rng config -padx 26\n"
            "    pack configure $mytoplevel.rng.min -side left\n"
            "    pack $mytoplevel.rng.dummy1 $mytoplevel.rng.max_lab $mytoplevel.rng.max_ent -side left\n"
            "\n"
            "    # parameters\n"
            "    labelframe $mytoplevel.para -borderwidth 1 -padx 5 -pady 5 -text [_ \"Parameters\"]\n"
            "    pack $mytoplevel.para -side top -fill x -pady 5\n"

            "   frame $mytoplevel.para.interval\n"
            "       label $mytoplevel.para.interval.lab -text [_ \"Rate (ms)\"]\n"
            "       entry $mytoplevel.para.interval.ent -textvariable $var_elsegui_interval -width 6\n"
            "       pack $mytoplevel.para.interval.ent $mytoplevel.para.interval.lab -side right -anchor e\n"
            "   frame $mytoplevel.para.ramp\n"
            "       label $mytoplevel.para.ramp.lab -text [_ \"Ramp (ms)\"]\n"
            "       entry $mytoplevel.para.ramp.ent -textvariable $var_elsegui_ramp -width 6\n"
            "       pack $mytoplevel.para.ramp.ent $mytoplevel.para.ramp.lab -side right -anchor e\n"
            "       pack $mytoplevel.para.interval -side left -expand 1 -ipadx 10\n"
            "       pack $mytoplevel.para.ramp -side left -expand 1 -ipadx 10\n"
            "    # get the current font name from the int given from C-space (gn_f)\n"
            "    set current_font $::font_family\n"
            "\n"
            "    # colors\n"
            "    labelframe $mytoplevel.colors -borderwidth 1 -text [_ \"Colors\"] -padx 5 -pady 5\n"
            "    pack $mytoplevel.colors -fill x\n"
            "\n"
            "    frame $mytoplevel.colors.select\n"
            "    pack $mytoplevel.colors.select -side top\n"
            "    radiobutton $mytoplevel.colors.select.radio0 -value 0 -variable \\\n"
            "        $var_elsegui_l2_f1_b0 -text [_ \"Background\"] -justify left\n"
            "    radiobutton $mytoplevel.colors.select.radio1 -value 1 -variable \\\n"
            "        $var_elsegui_l2_f1_b0 -text [_ \"Foreground\"] -justify left\n"
            "    radiobutton $mytoplevel.colors.select.radio2 -value 2 -variable \\\n"
            "        $var_elsegui_l2_f1_b0 -text [_ \"Text\"] -justify left\n"
            "    if { [eval concat $$var_elsegui_fcol] ne \"none\" } {\n"
            "        pack $mytoplevel.colors.select.radio0 $mytoplevel.colors.select.radio1 \\\n"
            "            -side left\n"
            "    } else {\n"
            "        pack $mytoplevel.colors.select.radio0 -side left\n"
            "    }\n"
            "\n"
            "    frame $mytoplevel.colors.sections\n"
            "    pack $mytoplevel.colors.sections -side top\n"
            "    button $mytoplevel.colors.sections.but -text [_ \"Compose color\"] \\\n"
            "        -command \"::dialog_numbox::choose_col_bkfrlb $mytoplevel\"\n"
            "    pack $mytoplevel.colors.sections.but -side left -anchor w -pady 5 \\\n"
            "        -expand yes -fill x\n"
            "    frame $mytoplevel.colors.sections.exp\n"
            "    pack $mytoplevel.colors.sections.exp -side right -padx 5\n"
            "    if { [eval concat $$var_elsegui_fcol] ne \"none\" } {\n"
            "        label $mytoplevel.colors.sections.exp.fr_bk -text \"o=||=o\" -width 6 \\\n"
            "            -background [eval concat $$var_elsegui_bcol] \\\n"
            "            -activebackground [eval concat $$var_elsegui_bcol] \\\n"
            "            -foreground [eval concat $$var_elsegui_fcol] \\\n"
            "            -activeforeground [eval concat $$var_elsegui_fcol] \\\n"
            "            -font [list $current_font 14 $::font_weight] -padx 2 -pady 2 -relief ridge\n"
            "    } else {\n"
            "        label $mytoplevel.colors.sections.exp.fr_bk -text \"o=||=o\" -width 6 \\\n"
            "            -background [eval concat $$var_elsegui_bcol] \\\n"
            "            -activebackground [eval concat $$var_elsegui_bcol] \\\n"
            "            -foreground [eval concat $$var_elsegui_bcol] \\\n"
            "            -activeforeground [eval concat $$var_elsegui_bcol] \\\n"
            "            -font [list $current_font 14 $::font_weight] -padx 2 -pady 2 -relief ridge\n"
            "    }\n"
            "\n"
            "    # color scheme by Mary Ann Benedetto http://piR2.org\n"
            "    foreach r {r1 r2 r3} hexcols {\n"
            "       { \"#FFFFFF\" \"#DFDFDF\" \"#BBBBBB\" \"#FFC7C6\" \"#FFE3C6\" \"#FEFFC6\" \"#C6FFC7\" \"#C6FEFF\" \"#C7C6FF\" \"#E3C6FF\" }\n"
            "       { \"#9F9F9F\" \"#7C7C7C\" \"#606060\" \"#FF0400\" \"#FF8300\" \"#FAFF00\" \"#00FF04\" \"#00FAFF\" \"#0400FF\" \"#9C00FF\" }\n"
            "       { \"#404040\" \"#202020\" \"#000000\" \"#551312\" \"#553512\" \"#535512\" \"#0F4710\" \"#0E4345\" \"#131255\" \"#2F004D\" } } \\\n"
            "    {\n"
            "       frame $mytoplevel.colors.$r\n"
            "       pack $mytoplevel.colors.$r -side top\n"
            "       foreach i { 0 1 2 3 4 5 6 7 8 9} hexcol $hexcols \\\n"
            "           {\n"
            "               label $mytoplevel.colors.$r.c$i -background $hexcol -activebackground $hexcol -relief ridge -padx 7 -pady 0 -width 1\n"
            "               bind $mytoplevel.colors.$r.c$i <Button> \"::dialog_numbox::preset_col $mytoplevel $hexcol\"\n"
            "           }\n"
            "       pack $mytoplevel.colors.$r.c0 $mytoplevel.colors.$r.c1 $mytoplevel.colors.$r.c2 $mytoplevel.colors.$r.c3 \\\n"
            "           $mytoplevel.colors.$r.c4 $mytoplevel.colors.$r.c5 $mytoplevel.colors.$r.c6 $mytoplevel.colors.$r.c7 \\\n"
            "           $mytoplevel.colors.$r.c8 $mytoplevel.colors.$r.c9 -side left\n"
            "    }\n"
            "\n"
            "    # buttons\n"
            "    frame $mytoplevel.cao -pady 10\n"
            "    pack $mytoplevel.cao -side top\n"
            "    button $mytoplevel.cao.cancel -text [_ \"Cancel\"] \\\n"
            "        -command \"::dialog_numbox::cancel $mytoplevel\"\n"
            "    pack $mytoplevel.cao.cancel -side left -expand 1 -fill x -padx 15 -ipadx 10\n"
            "    if {$::windowingsystem ne \"aqua\"} {\n"
            "        button $mytoplevel.cao.apply -text [_ \"Apply\"] \\\n"
            "            -command \"::dialog_numbox::apply $mytoplevel\"\n"
            "        pack $mytoplevel.cao.apply -side left -expand 1 -fill x -padx 15 -ipadx 10\n"
            "    }\n"
            "    button $mytoplevel.cao.ok -text [_ \"OK\"] \\\n"
            "        -command \"::dialog_numbox::ok $mytoplevel\" -default active\n"
            "    pack $mytoplevel.cao.ok -side left -expand 1 -fill x -padx 15 -ipadx 10\n"
            "\n"
            "    $mytoplevel.dim.w_ent select from 0\n"
            "    $mytoplevel.dim.w_ent select adjust end\n"
            "    focus $mytoplevel.dim.w_ent\n"
            "\n"
            "    # live widget updates on OSX in lieu of Apply button\n"
            "    if {$::windowingsystem eq \"aqua\"} {\n"
            "\n"
            "        # call apply on Return in entry boxes that are in focus & rebind Return to ok button\n"
            "        bind $mytoplevel.dim.w_ent <KeyPress-Return> \"::dialog_numbox::apply_and_rebind_return $mytoplevel\"\n"
            "        bind $mytoplevel.dim.h_ent <KeyPress-Return>    \"::dialog_numbox::apply_and_rebind_return $mytoplevel\"\n"
            "\n"
            "        # unbind Return from ok button when an entry takes focus\n"
            "        $mytoplevel.dim.w_ent config -validate focusin -vcmd \"::dialog_numbox::unbind_return $mytoplevel\"\n"
            "        $mytoplevel.dim.h_ent config -validate focusin -vcmd  \"::dialog_numbox::unbind_return $mytoplevel\"\n"
            "\n"
            "        # remove cancel button from focus list since it's not activated on Return\n"
            "        $mytoplevel.cao.cancel config -takefocus 0\n"
            "\n"
            "        # show active focus on the ok button as it *is* activated on Return\n"
            "        $mytoplevel.cao.ok config -default normal\n"
            "        bind $mytoplevel.cao.ok <FocusIn> \"$mytoplevel.cao.ok config -default active\"\n"
            "        bind $mytoplevel.cao.ok <FocusOut> \"$mytoplevel.cao.ok config -default normal\"\n"
            "\n"
            "        # since we show the active focus, disable the highlight outline\n"
            "        $mytoplevel.cao.ok config -highlightthickness 0\n"
            "        $mytoplevel.cao.cancel config -highlightthickness 0\n"
            "    }\n"
            "\n"
            "    position_over_window $mytoplevel $::focused_window\n"
            "}\n"
            "\n"
            "# for live widget updates on OSX\n"
            "proc ::dialog_numbox::apply_and_rebind_return {mytoplevel} {\n"
            "    ::dialog_numbox::apply $mytoplevel\n"
            "    bind $mytoplevel <KeyPress-Return> \"::dialog_numbox::ok $mytoplevel\"\n"
            "    focus $mytoplevel.cao.ok\n"
            "    return 0\n"
            "}\n"
            "\n"
            "# for live widget updates on OSX\n"
            "proc ::dialog_numbox::unbind_return {mytoplevel} {\n"
            "    bind $mytoplevel <KeyPress-Return> break\n"
            "    return 1\n"
            "}\n");
}

void numbox_tilde_setup(void){
    numbox_class = class_new(gensym("numbox~"), (t_newmethod)numbox_new,
        (t_method)numbox_free, sizeof(t_numbox), 0, A_GIMME, 0);
    class_addmethod(numbox_class, nullfn, gensym("signal"), 0);
    class_addmethod(numbox_class, (t_method)numbox_dsp, gensym("dsp"), A_CANT, 0);
    class_addfloat(numbox_class, numbox_float);
    class_addmethod(numbox_class, (t_method)numbox_rate, gensym("rate"), A_FLOAT, 0);
    class_addmethod(numbox_class, (t_method)numbox_ramp, gensym("ramp"), A_FLOAT, 0);
    class_addmethod(numbox_class, (t_method)numbox_width, gensym("width"), A_FLOAT, 0);
    class_addmethod(numbox_class, (t_method)numbox_size, gensym("size"), A_FLOAT, 0);
    class_addmethod(numbox_class, (t_method)numbox_range, gensym("range"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(numbox_class, (t_method)numbox_bg, gensym("bgcolor"), A_GIMME, 0);
    class_addmethod(numbox_class, (t_method)numbox_fg, gensym("fgcolor"), A_GIMME, 0);
    class_addmethod(numbox_class, (t_method)numbox_zoom, gensym("zoom"), A_CANT, 0);
    class_addmethod(numbox_class, (t_method)numbox_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(numbox_class, (t_method)numbox_motion, gensym("motion"),
        A_FLOAT, A_FLOAT, A_DEFFLOAT, 0);
    class_addmethod(numbox_class, (t_method)numbox_dialog, gensym("dialog"), A_GIMME, 0);
    numbox_widgetbehavior.w_getrectfn  = numbox_getrect;
    numbox_widgetbehavior.w_displacefn = numbox_displace;
    numbox_widgetbehavior.w_selectfn   = numbox_select;
    numbox_widgetbehavior.w_deletefn   = numbox_delete;
    numbox_widgetbehavior.w_clickfn    = numbox_newclick;
    numbox_widgetbehavior.w_visfn      = numbox_vis;
    numbox_widgetbehavior.w_activatefn = NULL;
    class_setwidget(numbox_class, &numbox_widgetbehavior);
    class_setsavefn(numbox_class, numbox_save);
    class_setpropertiesfn(numbox_class, numbox_properties);
}
