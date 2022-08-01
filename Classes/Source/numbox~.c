// By Tim Schoen and Porres

#include "m_pd.h"
#include "g_canvas.h"
#include "magic.h"
#include <stdlib.h>
#include <string.h>

#define MINDIGITS       1
#define MAX_NUMBOX_LEN  32
#define MINSIZE         8

typedef struct _numbox{
    t_object  x_obj;
    int       x_change;
    int       x_finemoved;
    int       x_height;
    int       x_width;
    char      x_font[MAXPDSTRING]; // WE DON'T ACTUALLY SET FONT NAME - FIX ME
    int       x_fontsize; // GET SYSTEM font size
    int       x_fgcolor;  // oh no, no int, no no no
    int       x_bgcolor;
    t_glist  *x_glist;
    t_clock  *x_clock_reset;
    t_clock  *x_clock_wait;
    t_clock  *x_clock_repaint;
    t_float   x_in_val;
    t_float   x_out_val;
    t_float   x_set_val;
    t_float   x_ramp_val;
    t_float   x_ramp;
    t_float   x_last_out;
    t_float   x_maximum;
    t_float   x_minimum;
    t_float   x_sr_khz;
    int       x_selected;
    int       x_ramp_time;
    int       x_interval_ms;
    int       x_zoom;
    int       x_numwidth;
    int       x_outmode;
    int       x_needs_update;
    char      x_buf[MAX_NUMBOX_LEN];
}t_numbox;

static void numbox_dialog_init(void);
t_widgetbehavior numbox_widgetbehavior;
static t_class *numbox_class;

// // // // // // // //  widget helper functions // // // // // // // // // // //

static void numbox_ftoa(t_numbox *x){
    double f = x->x_outmode ? x->x_set_val : x->x_in_val;
    int bufsize, is_exp = 0, i, idecimal;
    sprintf(x->x_buf, "%g", f);
    bufsize = (int)strlen(x->x_buf);
    int real_numwidth = x->x_numwidth + 1;
    if(bufsize > real_numwidth){ // if to reduce
        if(is_exp){
            if(real_numwidth <= 5){
                x->x_buf[0] = (f < 0.0 ? '-' : '+');
                x->x_buf[1] = 0;
            }
            i = bufsize - 4;
            for(idecimal = 0; idecimal < i; idecimal++)
                if(x->x_buf[idecimal] == '.')
                    break;
            if(idecimal > (real_numwidth - 4)){
                x->x_buf[0] = (f < 0.0 ? '-' : '+');
                x->x_buf[1] = 0;
            }
            else{
                int new_exp_index = real_numwidth - 4;
                int old_exp_index = bufsize - 4;
                for(i = 0; i < 4; i++, new_exp_index++, old_exp_index++)
                    x->x_buf[new_exp_index] = x->x_buf[old_exp_index];
                x->x_buf[x->x_numwidth] = 0;
            }
        }
        else{
            for(idecimal = 0; idecimal < bufsize; idecimal++)
                if(x->x_buf[idecimal] == '.')
                    break;
            if(idecimal > real_numwidth){
                x->x_buf[0] = (f < 0.0 ? '-' : '+');
                x->x_buf[1] = 0;
            }
            else
                x->x_buf[real_numwidth] = 0;
        }
    }
}

static void numbox_draw_update(t_gobj *client, t_glist *glist){
    t_numbox *x = (t_numbox *)client;
    if(glist_isvisible(glist)){
        t_canvas *cv = glist_getcanvas(glist);
        if(x->x_change && x->x_buf[0] && x->x_outmode){ // what is this???
            char *cp = x->x_buf;
            int sl = (int)strlen(x->x_buf);
            x->x_buf[sl] = '>';
            x->x_buf[sl+1] = 0;
            if(sl >= x->x_numwidth)
                cp += sl - x->x_numwidth + 1;
            sys_vgui(".x%lx.c itemconfigure %lxTEXT -fill #%06x\n", cv, x, x->x_fgcolor);
            sys_vgui(".x%lx.c itemconfigure %lxNUMBER -text {%s}\n", cv, x, cp);
            sys_vgui(".x%lx.c itemconfigure %lxBASE -width %d\n", cv, x, x->x_zoom*2);
            x->x_buf[sl] = 0;
        }
        else{
            numbox_ftoa(x);
            sys_vgui(".x%lx.c itemconfigure %lxNUMBER -text {%s}\n", cv, x, x->x_buf);
            if(x->x_selected){
                sys_vgui(".x%lx.c itemconfigure %lxTEXT -fill blue\n", cv, x);
                sys_vgui(".x%lx.c itemconfigure %lxBASE -outline blue\n", cv, x);
            }
            else{
                sys_vgui(".x%lx.c itemconfigure %lxTEXT -fill #%06x\n", cv, x, x->x_fgcolor);
                sys_vgui(".x%lx.c itemconfigure %lxBASE -outline black\n", cv, x);
            }
            sys_vgui(".x%lx.c itemconfigure %lxBASE -width %d\n", cv, x, x->x_zoom);
            x->x_buf[0] = 0;
        }
    }
}

static void numbox_tick_reset(t_numbox *x){
    if(x->x_change && x->x_glist){
        x->x_change = 0;
        sys_queuegui(x, x->x_glist, numbox_draw_update);
    }
}

static void numbox_tick_wait(t_numbox *x){
    sys_queuegui(x, x->x_glist, numbox_draw_update);
}

static void numbox_periodic_update(t_numbox *x){
    if(x->x_needs_update){ // repaint when audio is coming in
        numbox_draw_update(&x->x_obj.te_g, x->x_glist);
        x->x_needs_update = 0;  // clear repaint flag
    }
    clock_delay(x->x_clock_repaint, x->x_interval_ms);
}

int numbox_check_minmax(t_numbox *x, t_float min, t_float max){
    int ret = 0;
    x->x_minimum = min;
    x->x_maximum = max;
    if(x->x_set_val < x->x_minimum){
        x->x_set_val = x->x_minimum;
        ret = 1;
    }
    if(x->x_set_val > x->x_maximum){
        x->x_set_val = x->x_maximum;
        ret = 1;
    }
    if(x->x_out_val < x->x_minimum)
        x->x_out_val = x->x_minimum;
    if(x->x_out_val > x->x_maximum)
        x->x_out_val = x->x_maximum;
    return(ret);
}

static void numbox_clip(t_numbox *x){
    if(x->x_minimum == 0 && x->x_maximum == 0)
        return;
    if(x->x_out_val < x->x_minimum)
        x->x_out_val = x->x_minimum;
    if(x->x_out_val > x->x_maximum)
        x->x_out_val = x->x_maximum;
    if(x->x_set_val < x->x_minimum)
        x->x_set_val = x->x_minimum;
    if(x->x_set_val > x->x_maximum)
        x->x_set_val = x->x_maximum;
}

static void numbox_range(t_numbox *x, t_floatarg f1, t_floatarg f2){
    x->x_minimum = f1, x->x_maximum = f2;
    if(numbox_check_minmax(x, x->x_minimum, x->x_maximum))
        sys_queuegui(x, x->x_glist, numbox_draw_update);
}

static void numbox_calc_fontwidth(t_numbox *x){
    int w, f = 31;
    w = x->x_fontsize * f * x->x_numwidth;
    w /= 36;
    x->x_width = (w + (x->x_height/2)/x->x_zoom + 4) * x->x_zoom;
}

static void numbox_draw_new(t_numbox *x, t_glist *glist){
    int xpos = text_xpix(&x->x_obj, glist);
    int ypos = text_ypix(&x->x_obj, glist);
    int w = x->x_width, half = x->x_height/2;
    int d = x->x_zoom + x->x_height/(34*x->x_zoom);
    int iow = IOWIDTH * x->x_zoom, ioh = 3 * x->x_zoom;
    t_canvas *cv = glist_getcanvas(glist);
    sys_vgui(".x%lx.c create polygon %d %d %d %d %d %d %d %d %d %d -width %d -outline black -fill #%06x "
        "-tags [list %lxBASE %lxALL]\n", cv, xpos, ypos, xpos+w, ypos, xpos+w, ypos+x->x_height,
        xpos, ypos+x->x_height, xpos, ypos, x->x_zoom, x->x_bgcolor, x, x);
    sys_vgui(".x%lx.c create text %d %d -text {~} -anchor w -font {{%s} -%d %s} -fill #%06x "
        "-tags [list %lxTILDE %lxTEXT %lxALL]\n", cv, xpos + 2*x->x_zoom+1, ypos+half+d, x->x_font,
        x->x_fontsize*x->x_zoom, sys_fontweight, x->x_fgcolor, x, x, x);
    sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags [list %lxOUT %lxALL]\n",
        cv, xpos, ypos+x->x_height+x->x_zoom-ioh, xpos+iow, ypos+x->x_height, x, x);
    sys_vgui(".x%lx.c create rectangle %d %d %d %d -fill black -tags [list %lxIN %lxALL]\n",
        cv, xpos, ypos, xpos+iow, ypos-x->x_zoom+ioh, x, x);
    numbox_ftoa(x);
    sys_vgui(".x%lx.c create text %d %d -text {%s} -anchor w -font {{%s} -%d %s} -fill #%06x "
        "-tags [list %lxNUMBER %lxTEXT %lxALL]\n", cv, xpos+half+2*x->x_zoom+3, ypos+half+d,
        x->x_buf, x->x_font, x->x_fontsize*x->x_zoom, sys_fontweight, x->x_fgcolor, x, x, x);
}

static void numbox_delete(t_gobj *z, t_glist *glist){
    canvas_deletelinesfor(glist, (t_text *)z);
}

static void numbox_select(t_gobj *z, t_glist *glist, int sel){
    t_numbox *x = (t_numbox *)z;
    t_canvas *cv = glist_getcanvas(glist);
    if((x->x_selected = sel)){
        sys_vgui(".x%lx.c itemconfigure %lxTEXT -fill blue\n", cv, x);
        sys_vgui(".x%lx.c itemconfigure %lxBASE -outline blue\n", cv, x);
    }
    else{
        sys_vgui(".x%lx.c itemconfigure %lxTEXT -fill #%06x\n", cv, x, x->x_fgcolor);
        sys_vgui(".x%lx.c itemconfigure %lxBASE -outline black\n", cv, x);
    }
}

static void numbox_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_numbox *x = (t_numbox *)z;
    x->x_obj.te_xpix += dx;
    x->x_obj.te_ypix += dy;
    t_canvas *cv = glist_getcanvas(glist);
    sys_vgui(".x%lx.c move %lxALL %d %d\n", cv, x, dx*x->x_zoom, dy*x->x_zoom);
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
        sys_unqueuegui(z);
    }
}

// ------------------------ widgetbehaviour-----------------------------
static void numbox_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2){
    t_numbox* x = (t_numbox*)z;
    *xp1 = text_xpix(&x->x_obj, glist);
    *yp1 = text_ypix(&x->x_obj, glist);
    *xp2 = *xp1 + x->x_width;
    *yp2 = *yp1 + x->x_height;
}

static t_symbol* color2symbol(int col){
    char colname[MAXPDSTRING];
    colname[0] = colname[MAXPDSTRING-1] = 0;
    snprintf(colname, MAXPDSTRING-1, "#%06x", col);
    return(gensym(colname));
}

static void numbox_save(t_gobj *z, t_binbuf *b){
    t_numbox *x = (t_numbox *)z;
    t_symbol *bgcol = color2symbol(x->x_bgcolor);
    t_symbol *fgcol = color2symbol(x->x_fgcolor);
    if(x->x_change){
        x->x_change = 0;
        clock_unset(x->x_clock_reset);
        sys_queuegui(x, x->x_glist, numbox_draw_update);
    }
    binbuf_addv(b, "ssiisiiissiff", gensym("#X"), gensym("obj"), (int)x->x_obj.te_xpix,
        (int)x->x_obj.te_ypix, gensym("numbox~"), x->x_numwidth, (x->x_height/x->x_zoom)-5,
        x->x_interval_ms, bgcol, fgcol, x->x_ramp_time, x->x_minimum, x->x_maximum);
    binbuf_addv(b, ";");
}

static void numbox_properties(t_gobj *z, t_glist *owner){
    owner = NULL; // not used for some reason, avoid warning
    // For some reason the dialog doesn't work if we initialise it in the setup...
    numbox_dialog_init();
    t_numbox *x = (t_numbox *)z;
    char buf[800];
    if(x->x_change){
        x->x_change = 0;
        clock_unset(x->x_clock_reset);
        sys_queuegui(x, x->x_glist, numbox_draw_update);
    }
    sprintf(buf, "::dialog_numbox::pdtk_numbox_dialog %%s \
            -------dimensions(digits)(pix):------- \
            %d %d %d %d %d %d #%06x #%06x %.1f %.1f\n",
            x->x_numwidth, MINDIGITS, (x->x_height /x->x_zoom) - 5, MINSIZE, x->x_ramp_time, x->x_interval_ms,
            0xffffff & x->x_bgcolor, 0xffffff & x->x_fgcolor, x->x_minimum, x->x_maximum);
    gfxstub_new(&x->x_obj.ob_pd, x, buf);
}

static void numbox_doit(t_numbox *x){
    x->x_out_val = x->x_set_val;
    x->x_ramp = (x->x_out_val - x->x_ramp_val) / (x->x_ramp_time * x->x_sr_khz);
    numbox_clip(x);
}

static int getcolor_int(int index, int ac, t_atom*av){
    if((av+index)->a_type == A_SYMBOL){
        t_symbol *sym = atom_getsymbolarg(index, ac, av);
        if('#' == sym->s_name[0]){
            int col = (int)strtol(sym->s_name+1, 0, 16);
            return(col & 0xFFFFFF);
        }
    }
    return(0);
}

static void numbox_interval(t_numbox *x, t_floatarg f){
    x->x_interval_ms = f < 15 ? 15 : (int)f;
}

static void numbox_ramp(t_numbox *x, t_floatarg f){
    x->x_ramp_time = f < 0 ? 0 : (int)f;
}

static void numbox_dialog(t_numbox *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    #define SETCOLOR(a, col) do {char color[MAXPDSTRING]; snprintf(color, MAXPDSTRING-1, "#%06x", 0xffffff & col); color[MAXPDSTRING-1] = 0; SETSYMBOL(a, gensym(color));} while(0)
    int w = (int)atom_getfloatarg(0, ac, av);
    int h = (int)atom_getfloatarg(1, ac, av) + 5;
    t_float ramp_time = (int)atom_getfloatarg(2, ac, av);
    t_float interval = (int)atom_getfloatarg(3, ac, av);
    int bcol = (int)getcolor_int(4, ac, av);
    int fcol = (int)getcolor_int(5, ac, av);
    int min = atom_getfloatarg(6, ac, av);
    int max = atom_getfloatarg(7, ac, av);    
    t_atom at[ac];
    SETFLOAT(at, x->x_numwidth);
    SETFLOAT(at+1, x->x_height);
    SETFLOAT(at+2, x->x_interval_ms);
    SETCOLOR(at+3, x->x_bgcolor);
    SETCOLOR(at+4, x->x_fgcolor);
    SETFLOAT(at+5, x->x_ramp_time);
    SETFLOAT(at+6, x->x_minimum);
    SETFLOAT(at+7, x->x_maximum);
    pd_undo_set_objectstate(x->x_glist, (t_pd*)x, gensym("dialog"), ac, at, ac, av);
    x->x_numwidth = w < MINDIGITS ? MINDIGITS : w;
    x->x_height = (h >= MINSIZE ? h : MINSIZE) * x->x_zoom;
    x->x_fontsize = x->x_height - 5;
    numbox_calc_fontwidth(x);
    numbox_interval(x, interval);
    numbox_check_minmax(x, min, max);
    x->x_fgcolor = fcol & 0xffffff;
    x->x_bgcolor = bcol & 0xffffff;
    numbox_ramp(x, ramp_time);
    numbox_draw_erase(x, x->x_glist);
    numbox_draw_new(x, x->x_glist);
    canvas_fixlinesfor(x->x_glist, (t_text*)x);
}

static void numbox_set(t_numbox *x, t_floatarg f){
    t_float ftocompare = f;
        // bitwise comparison, suggested by Dan Borstein - to make this work
        // ftocompare must be t_float type like x_val.
    if(memcmp(&ftocompare, &x->x_out_val, sizeof(ftocompare))){
        x->x_set_val = ftocompare;
        numbox_clip(x);
        numbox_draw_update(&x->x_obj.te_g, x->x_glist);
    }
}

static void numbox_motion(t_numbox *x, t_floatarg dx, t_floatarg dy, t_floatarg up){
    dx = 0; // avoid warning
    double k2 = 1.0;
    if(up != 0)
        return;
    if(x->x_finemoved)
        k2 = 0.01;
    numbox_set(x, x->x_out_val - k2*dy);
    sys_queuegui(x, x->x_glist, numbox_draw_update);
    numbox_doit(x);
    clock_unset(x->x_clock_reset);
}

static void numbox_key(void *z, t_symbol *keysym, t_floatarg fkey){
    keysym = NULL; // unused, avoid warning
    t_numbox *x = z;
    char c = fkey;
    char buf[3];
    buf[1] = 0;
    if(c == 0){
        x->x_change = 0;
        clock_unset(x->x_clock_reset);
        sys_queuegui(x, x->x_glist, numbox_draw_update);
        return;
    }
    if(((c >= '0') && (c <= '9')) || (c == '.') || (c == '-') ||
    (c == 'e') || (c == '+') || (c == 'E')){
        if(strlen(x->x_buf) < (MAX_NUMBOX_LEN-2)){
            buf[0] = c;
            strcat(x->x_buf, buf);
            sys_queuegui(x, x->x_glist, numbox_draw_update);
        }
    }
    else if((c == '\b') || (c == 127)){
        int sl = (int)strlen(x->x_buf) - 1;
        if(sl < 0)
            sl = 0;
        x->x_buf[sl] = 0;
        sys_queuegui(x, x->x_glist, numbox_draw_update);
    }
    else if(((c == '\n') || (c == 13)) && x->x_buf[0] != 0){
        numbox_set(x, atof(x->x_buf));
        x->x_buf[0] = 0;
        x->x_change = 0;
        clock_unset(x->x_clock_reset);
        numbox_doit(x);
        sys_queuegui(x, x->x_glist, numbox_draw_update);
    }
    clock_delay(x->x_clock_reset, 3000);
}

static void numbox_click(t_numbox *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    shift = ctrl =alt = 0; // unused - avoid warning
    if(x->x_outmode)
        glist_grab(x->x_glist, &x->x_obj.te_g,
            (t_glistmotionfn)numbox_motion, numbox_key, xpos, ypos);
}

static int numbox_newclick(t_gobj *z, struct _glist *glist,
    int xpix, int ypix, int shift, int alt, int dbl, int doit){
    glist = NULL; // unused, avoid warning
    dbl = 0;
    t_numbox* x = (t_numbox *)z;
    if(doit){
        numbox_click(x, (t_floatarg)xpix, (t_floatarg)ypix, (t_floatarg)shift,
            0, (t_floatarg)alt);
        x->x_finemoved = shift;
        if(!x->x_change && xpix - x->x_obj.te_xpix > 10){
            clock_delay(x->x_clock_wait, 50);
            x->x_change = 1;
            clock_delay(x->x_clock_reset, 3000);
            x->x_buf[0] = 0;
        }
        else{
            x->x_change = 0;
            clock_unset(x->x_clock_reset);
            x->x_buf[0] = 0;
            sys_queuegui(x, x->x_glist, numbox_draw_update);
        }
    }
    return(1);
}

static void numbox_float(t_numbox *x, t_floatarg f){
    numbox_set(x, f);
    numbox_doit(x);
}

static void numbox_width(t_numbox *x, t_floatarg f){
    int w = (int)f;
    x->x_numwidth = w < MINDIGITS ? MINDIGITS : w;
    numbox_calc_fontwidth(x);
    numbox_draw_erase(x, x->x_glist);
    numbox_draw_new(x, x->x_glist);
}

static void numbox_size(t_numbox *x, t_floatarg f){
    int h = (int)f < MINSIZE ? MINSIZE : (int)f;
    x->x_height = (h + 4) * x->x_zoom;
    x->x_fontsize = x->x_height - 5;
    numbox_calc_fontwidth(x);
    numbox_draw_erase(x, x->x_glist);
    numbox_draw_new(x, x->x_glist);
}

/*static void numbox_bgcolor(t_numbox *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(ac == 1 && av->a_type == A_SYMBOL)
        ;
    else if(ac == 3 && av->a_type == A_SYMBOL)
        ;
}*/

static int colorfromarg(t_symbol *color_arg){
    if('#' == color_arg->s_name[0]){
        int col = (int)strtol(color_arg->s_name+1, 0, 16);
        return(col & 0xFFFFFF);
    }
    return(0);
}

static void *numbox_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_numbox *x = (t_numbox *)pd_new(numbox_class);
    int w = 4, h = 12; // GET SYSTEM FONT SIZE
    int interval = 100, ramp_time = 10;
    x->x_outmode = 1;
    t_float minimum = 0, maximum = 0;
    x->x_bgcolor = 0xDFDFDF;
    x->x_fgcolor = 0x00;
    x->x_zoom = ((t_glist*)canvas_getcurrent())->gl_zoom;
    if(ac == 8){
        w = atom_getintarg(0, ac, av);
        h = atom_getintarg(1, ac, av);
        interval = atom_getintarg(2, ac, av);
        x->x_bgcolor = colorfromarg(atom_getsymbolarg(3, ac, av));
        x->x_fgcolor = colorfromarg(atom_getsymbolarg(4, ac, av));
        ramp_time = atom_getintarg(5, ac, av);
        minimum = atom_getfloatarg(6, ac, av);
        maximum = atom_getfloatarg(7, ac, av);
    }
    x->x_glist = (t_glist *)canvas_getcurrent();
    x->x_in_val = 0.0;
    x->x_out_val = 0.0;
    x->x_set_val = 0.0;
    numbox_check_minmax(x, minimum, maximum);
    x->x_numwidth = w < MINDIGITS ? MINDIGITS : w;
    if(h < MINSIZE)
        h = MINSIZE;
    x->x_height = h + 4;
    x->x_fontsize = h;
    strcpy(x->x_font, sys_font);
    x->x_buf[0] = 0;
    x->x_clock_reset = clock_new(x, (t_method)numbox_tick_reset);
    x->x_clock_wait = clock_new(x, (t_method)numbox_tick_wait);
    x->x_clock_repaint = clock_new(x, (t_method)numbox_periodic_update);
    x->x_change = 0;
    numbox_calc_fontwidth(x);
    x->x_interval_ms = interval < 15 ? 15 : (int)interval;
    x->x_ramp_time = ramp_time;
    outlet_new(&x->x_obj,  &s_signal);
    // Start repaint clock
    clock_delay(x->x_clock_repaint, x->x_interval_ms);
    return(x);
}

static void numbox_zoom(t_numbox *x, t_floatarg zoom){
//    float mul = zoom == 1.0 ? 0.5 : 2.0;
//    x->x_widthidth = (int)((float)x->x_widthidth * mul);
//    x->x_heighteight = (int)((float)x->x_heighteight * mul);
    x->x_zoom = (int)zoom;
    numbox_draw_update(&x->x_obj.te_g, x->x_glist);
}

static void numbox_free(t_numbox *x){
    clock_free(x->x_clock_reset);
    clock_free(x->x_clock_wait);
    clock_free(x->x_clock_repaint);
    gfxstub_deleteforkey(x);
}

static t_int *numbox_perform_output(t_int *w){
    t_numbox *x = (t_numbox *)(w[1]);
    t_sample *out     = (t_sample *)(w[3]);
    t_int n           = (t_int)(w[4]);
    if(x->x_ramp_val != x->x_out_val && x->x_ramp != 0){  // apply a ramp
        for(int i = 0; i < n; i++){
            // Check if we reached our destination
            if((x->x_ramp < 0 && x->x_ramp_val <= x->x_out_val)
            || (x->x_ramp > 0 && x->x_ramp_val >= x->x_out_val)){
                x->x_ramp_val = x->x_out_val;
                x->x_ramp = 0;
            }
            x->x_ramp_val += x->x_ramp;
            out[i] = x->x_ramp_val;
            x->x_last_out = x->x_ramp_val;
        }
    }
    else for(int i = 0; i < n; i++) // No ramp needed
        out[i] = x->x_out_val;
    return(w+5);
}

static t_int *numbox_perform(t_int *w){
    t_numbox *x = (t_numbox *)(w[1]);
    t_sample *in      = (t_sample *)(w[2]);
    t_sample *out     = (t_sample *)(w[3]);
    t_int n           = (t_int)(w[4]);
    x->x_in_val = in[0];
    x->x_needs_update = 1;
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

// Tcl/Tk code for the preferences, based on the default IEMGUIs dialog
static void numbox_dialog_init(void){
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
            "       label $mytoplevel.para.interval.lab -text [_ \"Interval (ms)\"]\n"
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
    class_addmethod(numbox_class, (t_method)numbox_width, gensym("width"), A_FLOAT, 0);
    class_addmethod(numbox_class, (t_method)numbox_size, gensym("size"), A_FLOAT, 0);
    class_addmethod(numbox_class, (t_method)numbox_interval, gensym("rate"), A_FLOAT, 0);
    class_addmethod(numbox_class, (t_method)numbox_ramp, gensym("ramp"), A_FLOAT, 0);
    class_addmethod(numbox_class, (t_method)numbox_range, gensym("range"), A_FLOAT, A_FLOAT, 0);
//    class_addmethod(numbox_class, (t_method)numbox_bgcolor, gensym("bgcolor"), A_GIMME, 0);
//    class_addmethod(numbox_class, (t_method)numbox_fgcolor, gensym("fgcolor"), A_GIMME, 0);
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
