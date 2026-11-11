// porres

#define MAX_SIZE 1024

#include <m_pd.h>
#include <g_canvas.h>
#include <buffer.h>
#include <else_alloca.h>
#include <string.h>
#include <stdlib.h>

#define FUNCTION_MINWIDTH    40
#define FUNCTION_MINHEIGHT   20
#define FUNCTION_SELBDWIDTH  2
#define HANDLE_SIZE          12

static t_class *function_class, *edit_proxy_class, *handle_class;
static t_widgetbehavior function_widgetbehavior;

typedef struct _edit_proxy{
    t_object    p_obj;
    t_symbol   *p_sym;
    t_clock    *p_clock;
    struct      _function *p_cnv;
}t_edit_proxy;

typedef struct _function{
    t_object        x_obj;
    t_glist        *x_glist;
    t_edit_proxy   *x_proxy;
    t_canvas       *x_cv;
    int             x_state;
    int             x_n_states;
    int             x_flag;
    int             x_s_flag;
    int             x_r_flag;
    int             x_sel;
    int             x_width;
    int             x_height;
    int             x_savestate;
    int             x_grabbed; // number of grabbed point, for moving it/deleting it
    int             x_shift;
    int             x_snd_set;
    int             x_rcv_set;
    int             x_edit;
    int             x_theme;
    int             x_transparent;
    t_symbol       *x_send;
    t_symbol       *x_receive;
    t_symbol       *x_snd_raw;
    t_symbol       *x_rcv_raw;
    float          *x_points;
    float          *x_dur;
    float           x_total_duration;
    float           x_min;
    float           x_max;
    float           x_min_point;
    float           x_max_point;
    float           x_pointer_x;
    float           x_pointer_y;
    unsigned char   x_fgcolor[3];
    unsigned char   x_bgcolor[3];
    char            x_tag_obj[32];
    char            x_tag_rect[32];
    char            x_tag_line[32];
    char            x_tag_dots[32];
    char            x_tag_in[32];
    char            x_tag_out[32];
    char            x_tag_IO[32];
    char            x_tag_handle[32];
    t_symbol       *x_bindname; // for Tcl color validation round-trip
// handle
    t_pd           *x_handle;
}t_function;

typedef struct _handle{
    t_pd            h_pd;
    t_function     *h_master;
    t_symbol       *h_bindsym;
    char            h_pathname[64], h_outlinetag[64];
    int             h_dragon;
    int             h_dragx, h_dragy; // click position (canvas coords)
    int             h_dx, h_dy;       // current (clamped) drag delta
}t_handle; // resizing handle

static void function_bang(t_function *x);
static void function_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2);

static int function_vis_check(t_function *x){
    return(gobj_shouldvis((t_gobj *)x, x->x_glist) && glist_isvisible(x->x_glist));
}

/* effective colors (theme aware) */
static void function_get_bgcolor(t_function *x, char *buf, size_t size){
    if(x->x_theme)
        snprintf(buf, size, "#%06x", THISGUI->i_backgroundcolor);
    else
        snprintf(buf, size, "#%2.2x%2.2x%2.2x",
            x->x_bgcolor[0], x->x_bgcolor[1], x->x_bgcolor[2]);
}

static void function_get_fgcolor(t_function *x, char *buf, size_t size){
    if(x->x_theme)
        snprintf(buf, size, "#%06x", THISGUI->i_foregroundcolor);
    else
        snprintf(buf, size, "#%2.2x%2.2x%2.2x",
            x->x_fgcolor[0], x->x_fgcolor[1], x->x_fgcolor[2]);
}

//////////////////////// DRAWING ////////////////////////////////////////////////////////
static void function_draw_handle(t_function *x, int state){
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
        function_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
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
        // canvasx/canvasy convert widget coords to canvas coords
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

static void function_draw_iolet(t_function *x){
    int xpos = text_xpix(&x->x_obj, x->x_glist), ypos = text_ypix(&x->x_obj, x->x_glist);
    t_canvas *cv = glist_getcanvas(x->x_glist);
    unsigned int color = x->x_sel ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor;
    if(x->x_edit && x->x_receive == &s_){ // Inlet
        char *tags_in[] = {x->x_tag_in, x->x_tag_IO, x->x_tag_obj};
        pdgui_vmess(0, "crr iiii rk rk rS", cv, "create", "rectangle",
            xpos, ypos, xpos + IOWIDTH, ypos + IHEIGHT,
            "-fill", color, "-outline", color, "-tags", 3, tags_in);
    }
    if(x->x_edit && x->x_send == &s_){ // Outlet
        char *tags_out[] = {x->x_tag_out, x->x_tag_IO, x->x_tag_obj};
        pdgui_vmess(0, "crr iiii rk rk rS", cv, "create", "rectangle",
            xpos, ypos + x->x_height - OHEIGHT, xpos + IOWIDTH, ypos + x->x_height,
            "-fill", color, "-outline", color, "-tags", 3, tags_out);
    }
}

static void function_draw_dots(t_function *x, t_glist *glist){
    float range = x->x_max - x->x_min;
    float min = x->x_min;
    int yvalue;
    float xscale = x->x_width/x->x_dur[x->x_n_states];
    float yscale = x->x_height;
    int xpos = text_xpix(&x->x_obj, glist), ypos = text_ypix(&x->x_obj, glist) + x->x_height;
    char fgcolor[20];
    function_get_fgcolor(x, fgcolor, sizeof(fgcolor));
    char bgcolor[20];
    function_get_bgcolor(x, bgcolor, sizeof(bgcolor));
    char *tags_dots[] = {x->x_tag_dots, x->x_tag_obj};
    for(int i = 0; i <= x->x_n_states; i++){
        yvalue = (int)((x->x_points[i] - min) / range * yscale);
        int xc = xpos + (int)(x->x_dur[i]*xscale);
        pdgui_vmess(0, "crr iiii ri rs rs rS", glist_getcanvas(glist), "create", "oval",
            xc - 3, ypos - yvalue - 3, xc + 3, ypos - yvalue + 3,
            "-width", 2,
            "-outline", fgcolor,
            "-fill", x->x_transparent ? "" : bgcolor,
            "-tags", 2, tags_dots);
    }
}

// fills 'at' with the x/y coordinates of the function line, returns the number of atoms
static int function_line_coords(t_function *x, t_atom *at, int xpos, int ypos){
    float range = x->x_max - x->x_min;
    float min = x->x_min;
    float xscale = x->x_width / x->x_dur[x->x_n_states];
    float yscale = x->x_height;
    for(int i = 0; i <= x->x_n_states; i++){
        SETFLOAT(at + 2*i, (int)(xpos + x->x_dur[i]*xscale));
        SETFLOAT(at + 2*i + 1, (int)(ypos + x->x_height - (x->x_points[i]-min) / range*yscale));
    }
    return(2 * (x->x_n_states + 1));
}

static void function_draw(t_function *x, t_glist *glist){
    t_canvas *cv = glist_getcanvas(x->x_glist);
    int xpos = text_xpix(&x->x_obj, glist), ypos = text_ypix(&x->x_obj, glist);
    char bgcolor[20];
    function_get_bgcolor(x, bgcolor, sizeof(bgcolor));
    char fgcolor[20];
    function_get_fgcolor(x, fgcolor, sizeof(fgcolor));
    // background
    char *tags_rect[] = {x->x_tag_rect, x->x_tag_obj};
    pdgui_vmess(0, "crr iiii rk ri rs rS", cv, "create", "rectangle",
        xpos, ypos, xpos + x->x_width, ypos + x->x_height,
        "-outline", x->x_sel ? THISGUI->i_selectcolor : THISGUI->i_foregroundcolor,
        "-width", x->x_sel ? FUNCTION_SELBDWIDTH : 1,
        "-fill", x->x_transparent ? "" : bgcolor,
        "-tags", 2, tags_rect);
    // function line
    int n = 2 * (x->x_n_states + 1);
    t_atom *at = ALLOCA(t_atom, n);
    function_line_coords(x, at, xpos, ypos);
    char *tags_line[] = {x->x_tag_line, x->x_tag_obj};
    pdgui_vmess(0, "crr A rs ri rS", cv, "create", "line", n, at,
        "-fill", fgcolor, "-width", 2, "-tags", 2, tags_line);
    FREEA(at, t_atom, n);
    function_draw_dots(x, glist);
    function_draw_iolet(x);
}

static void function_erase(t_function* x, t_glist* glist){
    pdgui_vmess(0, "crs", glist_getcanvas(glist), "delete", x->x_tag_obj);
}

static void function_update(t_function *x, t_glist *glist){
    int xpos = text_xpix(&x->x_obj, glist), ypos = text_ypix(&x->x_obj, glist);
    t_canvas *cv = glist_getcanvas(glist);
    pdgui_vmess(0, "crs iiii", cv, "coords", x->x_tag_rect,
        xpos, ypos, xpos + x->x_width, ypos + x->x_height);
    int n = 2 * (x->x_n_states + 1);
    t_atom *at = ALLOCA(t_atom, n);
    function_line_coords(x, at, xpos, ypos);
    pdgui_vmess(0, "crs A", cv, "coords", x->x_tag_line, n, at);
    FREEA(at, t_atom, n);
    pdgui_vmess(0, "crs", cv, "delete", x->x_tag_dots);
    function_draw_dots(x, glist);
}

// ------------------------ widgetbehaviour-----------------------------
static void function_getrect(t_gobj *z, t_glist *glist, int *xp1, int *yp1, int *xp2, int *yp2){
    t_function* x = (t_function*)z;
    int xpos = text_xpix(&x->x_obj, glist), ypos = text_ypix(&x->x_obj, glist);
    *xp1 = xpos, *yp1 = ypos;
    *xp2 = xpos + x->x_width, *yp2 = ypos + x->x_height;
}

static void function_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_function *x = (t_function *)z;
    x->x_obj.te_xpix += dx, x->x_obj.te_ypix += dy;
    pdgui_vmess(0, "crs ii", glist_getcanvas(x->x_glist), "move", x->x_tag_obj, dx, dy);
    canvas_fixlinesfor(glist, (t_text*)x);
}

static void function_select(t_gobj *z, t_glist *glist, int sel){
    t_function *x = (t_function *)z;
    t_handle *sh = (t_handle *)x->x_handle;
    t_canvas *cv = glist_getcanvas(glist);
    x->x_sel = sel;
    if(sel){
        pdgui_vmess(0, "crs rk ri", cv, "itemconfigure", x->x_tag_rect,
            "-outline", THISGUI->i_selectcolor, "-width", FUNCTION_SELBDWIDTH);
        pdgui_vmess(0, "crs rk rk", cv, "itemconfigure", x->x_tag_IO,
            "-outline", THISGUI->i_selectcolor, "-fill", THISGUI->i_selectcolor);
        if(x->x_edit && sh->h_pathname[0]){
            pdgui_vmess(0, "rr rk", sh->h_pathname, "configure",
                "-highlightbackground", THISGUI->i_selectcolor);
            pdgui_vmess(0, "rr rk", sh->h_pathname, "configure",
                "-bg", THISGUI->i_selectcolor);
        }
    }
    else{
        pdgui_vmess(0, "crs rk ri", cv, "itemconfigure", x->x_tag_rect,
            "-outline", THISGUI->i_foregroundcolor, "-width", 1);
        pdgui_vmess(0, "crs rk rk", cv, "itemconfigure", x->x_tag_IO,
            "-outline", THISGUI->i_foregroundcolor, "-fill", THISGUI->i_foregroundcolor);
        if(x->x_edit && sh->h_pathname[0]){
            pdgui_vmess(0, "rr rk", sh->h_pathname, "configure",
                "-highlightbackground", THISGUI->i_foregroundcolor);
            pdgui_vmess(0, "rr rk", sh->h_pathname, "configure",
                "-bg", THISGUI->i_gopcolor);
        }
    }
}

static void function_delete(t_gobj *z, t_glist *glist){
    canvas_deletelinesfor(glist, (t_text *)z);
}

static void function_vis(t_gobj *z, t_glist *glist, int vis){
    t_function *x = (t_function *)z;
    t_handle *sh = (t_handle *)x->x_handle;
    x->x_cv = glist_getcanvas(glist);
    sprintf(sh->h_pathname, ".x%lx.h%lx", (unsigned long)x->x_cv, (unsigned long)sh);
    if(x->x_edit && sh->h_pathname[0])
        pdgui_vmess(0, "rs", "destroy", sh->h_pathname);
    if(vis){
        function_draw(x, glist);
        function_draw_handle(x, x->x_edit);
    }
    else
        function_erase(x, glist);
}

static void function_move_dot(t_function* x, t_glist* glist){
    float dur;
    float xscale = x->x_dur[x->x_n_states] / x->x_width;
    float range = x->x_max - x->x_min;
    float min =  x->x_min;
    if((x->x_grabbed > 0) && (x->x_grabbed < x->x_n_states)){
        dur = (x->x_pointer_x - text_xpix(&x->x_obj,glist))*xscale;
        if(dur < x->x_dur[x->x_grabbed-1])
            dur = x->x_dur[x->x_grabbed-1];
        if(dur >x->x_dur[x->x_grabbed+1])
            dur = x->x_dur[x->x_grabbed+1];
        x->x_dur[x->x_grabbed] = dur;
    }
    float grabbed = 1.0f - (x->x_pointer_y - (float)text_ypix(&x->x_obj, glist)) / (float)x->x_height;
    grabbed = grabbed < 0.0 ? 0.0 : grabbed > 1.0 ? 1.0 : grabbed;
    x->x_points[x->x_grabbed] = grabbed * range + min;
    function_bang(x);
    function_update(x, glist);
}

static void function_motion(t_function *x, t_floatarg dx, t_floatarg dy){
    if(x->x_shift)
        dx *= 0.1, dy *= 0.1;
    x->x_pointer_x += dx, x->x_pointer_y += dy;
    function_move_dot(x, x->x_glist);
}

static void function_key(t_function *x, t_floatarg f){
    if(f == 8.0 && x->x_grabbed > 0 && x->x_grabbed < x->x_n_states){ // delete dot
        for(int i = x->x_grabbed; i <= x->x_n_states; i++){
            x->x_dur[i] = x->x_dur[i+1];
            x->x_points[i] = x->x_points[i+1];
        }
        x->x_n_states--;
        x->x_grabbed = -1;
        function_update(x, x->x_glist);
        function_bang(x);
    }
}

static void function_create_dot(t_function *x, t_glist *glist, int dot_x,int dot_y){
    x->x_grabbed = -1;
    float tval, minval = 100000000000000000000.0; // stupidly high number
    float range = x->x_max - x->x_min;
    float min =  x->x_min;
    float xpos = (float)text_xpix(&x->x_obj, glist), ypos = (float)text_ypix(&x->x_obj, glist);
    if(dot_x > xpos + x->x_width) // bullshit?
        dot_x = xpos + x->x_width;
    float xscale = x->x_width / x->x_dur[x->x_n_states];
    float yscale = x->x_height;
    for(int i = 0; i <= x->x_n_states; i++){
        float dx2 = (xpos + (x->x_dur[i] * xscale)) - dot_x;
        float dy2 = (ypos + yscale - ((x->x_points[i] - min) / range * yscale)) - dot_y;
        tval = sqrt(dx2*dx2 + dy2*dy2);
        if(tval <= minval){
            minval = tval;
            x->x_grabbed = i;
        }
    }
    if(minval > 8 && x->x_grabbed >= 0){ // create and grab new dot
        while(((xpos + x->x_dur[x->x_grabbed] * xscale) - dot_x) < 0)
            x->x_grabbed++;
        while(((xpos + x->x_dur[x->x_grabbed-1] * xscale) - dot_x) > 0)
            x->x_grabbed--;
        for(int i = x->x_n_states; i >= x->x_grabbed; i--){
            x->x_dur[i+1] = x->x_dur[i];
            x->x_points[i+1] = x->x_points[i];
        }
        x->x_dur[x->x_grabbed] = (float)(dot_x-xpos) / x->x_width * x->x_dur[x->x_n_states++];
        x->x_pointer_x = dot_x;
        x->x_pointer_y = dot_y;
        function_move_dot(x, glist);
    }
    else{ // don't create
        x->x_pointer_x = xpos + x->x_dur[x->x_grabbed] * x->x_width / x->x_dur[x->x_n_states];
        x->x_pointer_y = dot_y;
    }
}

static int function_click(t_function *x, t_glist *gl, int xpos, int ypos, int shift, int alt, int dbl, int doit){
    alt = dbl = 0; // remove warning
    if(doit){
        if(x->x_n_states + 1 > MAX_SIZE){
            pd_error(x, "[function]: too many lines, maximum is %d", MAX_SIZE);
            return(0);
        }
        function_create_dot(x, gl, xpos, ypos);
        glist_grab(x->x_glist, &x->x_obj.te_g, (t_glistmotionfn)function_motion, (t_glistkeyfn)function_key, xpos, ypos);
        x->x_shift = shift;
    }
    return(1);
}

static void function_save(t_gobj *z, t_binbuf *b){
    t_function *x = (t_function *)z;
    t_binbuf *bb = x->x_obj.te_binbuf;
    int i, n_args = binbuf_getnatom(bb); // number of arguments
    char buf[80];
    if(!x->x_snd_set){ // no send set, search arguments
        if(n_args > 0){ // we have arguments, let's search them
            if(x->x_flag){ // we got flags
                if(x->x_s_flag){ // we got a search flag, let's get it
                    for(i = 0;  i < n_args; i++){
                        atom_string(binbuf_getvec(bb) + i, buf, 80);
                        if(gensym(buf) == gensym("-send")){
                            i++;
                            atom_string(binbuf_getvec(bb) + i, buf, 80);
                            x->x_snd_raw = gensym(buf);
                            break;
                        }
                    }
                }
            }
            else{ // we got no flags, let's search for argument
                int arg_n = 3; // receive argument number
                if(n_args > arg_n){ // we have it, get it
                    atom_string(binbuf_getvec(bb) + arg_n, buf, 80);
                    x->x_snd_raw = gensym(buf);
                }
            }
        }
    }
    if(x->x_snd_raw == &s_) // if no symbol, set to "empty"
        x->x_snd_raw = gensym("empty");
    if(!x->x_rcv_set){ // no receive set, search arguments
        if(n_args > 0){ // we have arguments, let's search them
            if(x->x_flag){ // search for receive name in flags
                if(x->x_r_flag){ // we got a receive flag, let's get it
                    for(i = 0;  i < n_args; i++){
                        atom_string(binbuf_getvec(bb) + i, buf, 80);
                        if(gensym(buf) == gensym("-receive")){
                            i++;
                            atom_string(binbuf_getvec(bb) + i, buf, 80);
                            x->x_rcv_raw = gensym(buf);
                            break;
                        }
                    }
                }
            }
            else{ // we got no flags, let's search for argument
                int arg_n = 4; // receive argument number
                if(n_args >= arg_n){ // we have it, get it
                    atom_string(binbuf_getvec(bb) + arg_n, buf, 80);
                    x->x_rcv_raw = gensym(buf);
                }
            }
        }
    }
    if(x->x_rcv_raw == &s_) // if no symbol, set to "empty"
        x->x_rcv_raw = gensym("empty");
    binbuf_addv(b,
                "ssiis",
                gensym("#X"),
                gensym("obj"),
                (int)x->x_obj.te_xpix,
                (int)x->x_obj.te_ypix,
                atom_getsymbol(binbuf_getvec(x->x_obj.te_binbuf))
                );
    binbuf_addv(b, "iissffiiiiiiiiii",
                x->x_width,
                x->x_height,
                x->x_snd_raw,
                x->x_rcv_raw,
                x->x_min,
                x->x_max,
                (int)x->x_bgcolor[0],
                (int)x->x_bgcolor[1],
                (int)x->x_bgcolor[2],
                (int)x->x_fgcolor[0],
                (int)x->x_fgcolor[1],
                (int)x->x_fgcolor[2],
                x->x_savestate,
                x->x_theme,       // formerly the first placeholder
                x->x_transparent, // formerly the second placeholder
                0                 // placeholder
                );
    binbuf_addv(b, "f", (float)x->x_points[x->x_state = 0]);
                i = 1;
                int ac = x->x_n_states * 2 + 1;
                while(i < ac){
                    float dur = x->x_dur[x->x_state+1] - x->x_dur[x->x_state];
                    binbuf_addv(b, "f", (float)dur);
                    i++;
                    x->x_state++;
                    binbuf_addv(b, "f", (float)x->x_points[x->x_state]);
                    i++;
                }
    binbuf_addv(b, ";");
}

///////////////////// METHODS /////////////////////////////////////////////////////////////
static void function_bang(t_function *x){
    int ac = x->x_n_states * 2 + 1;
    t_atom* at = ALLOCA(t_atom, ac);
    SETFLOAT(at, x->x_min_point = x->x_max_point = x->x_points[x->x_state = 0]); // get 1st
    for(int i = 1; i < ac; i++){ // get the rest
        float dur = x->x_dur[x->x_state+1] - x->x_dur[x->x_state];
        SETFLOAT(at+i, dur); // duration
        i++, x->x_state++;
        float point = x->x_points[x->x_state];
        if(point < x->x_min_point)
            x->x_min_point = point;
        if(point > x->x_max_point)
            x->x_max_point = point;
        SETFLOAT(at+i, point);
    }
    outlet_list(x->x_obj.ob_outlet, &s_list, ac, at);
    if(x->x_send != &s_ && x->x_send->s_thing)
        pd_list(x->x_send->s_thing, &s_list, ac, at);
    FREEA(at, t_atom, ac);
}

static void function_duration(t_function* x, t_floatarg dur){
    if(dur < 1){
        post("function: minimum duration is 1 ms");
        return;
    }
    if(dur != x->x_total_duration){
        x->x_total_duration = dur;
        float f = dur/x->x_dur[x->x_n_states];
        for(int i = 1; i <= x->x_n_states; i++)
            x->x_dur[i] *= f;
    }
}

static void function_float(t_function *x, t_floatarg f){
    float val;
    if(f <= 0){
        val = x->x_points[0];
        outlet_float(x->x_obj.ob_outlet, val);
        if(x->x_send != &s_)
            pd_float(x->x_send->s_thing, f);
        return;
    }
    if(f >= 1){
        val = x->x_points[x->x_n_states];
        outlet_float(x->x_obj.ob_outlet, val);
        if(x->x_send != &s_ && x->x_send->s_thing)
            pd_float(x->x_send->s_thing, f);
        return;
    }
    f *= x->x_dur[x->x_n_states];
    if(x->x_state > x->x_n_states)
        x->x_state = x->x_n_states;
    while((x->x_state > 0) && (f < x->x_dur[x->x_state-1]))
        x->x_state--;
    while((x->x_state <  x->x_n_states) && (x->x_dur[x->x_state] < f))
        x->x_state++;
    val = x->x_points[x->x_state-1] + (f-x->x_dur[x->x_state-1]) * (x->x_points[x->x_state] - x->x_points[x->x_state-1])
        / (x->x_dur[x->x_state] - x->x_dur[x->x_state-1]);
    outlet_float(x->x_obj.ob_outlet,val);
    if(x->x_send != &s_ && x->x_send->s_thing)
        pd_float(x->x_send->s_thing, val);
}

static void function_i(t_function *x, t_floatarg f){
    float val;
    if(f <= 0){
        val = x->x_points[0];
        outlet_float(x->x_obj.ob_outlet, val);
        if(x->x_send != &s_)
            pd_float(x->x_send->s_thing, f);
        return;
    }
    if(f >= x->x_dur[x->x_n_states]){
        val = x->x_points[x->x_n_states];
        outlet_float(x->x_obj.ob_outlet, val);
        if(x->x_send != &s_)
            pd_float(x->x_send->s_thing, f);
        return;
    }
    if(x->x_state > x->x_n_states)
        x->x_state = x->x_n_states;
    while((x->x_state > 0) && (f < x->x_dur[x->x_state-1]))
        x->x_state--;
    while((x->x_state <  x->x_n_states) && (x->x_dur[x->x_state] < f))
        x->x_state++;
    float b = x->x_points[x->x_state-1];
    float c = x->x_points[x->x_state];
    float dur_m1 = x->x_dur[x->x_state-1];
    float dur = x->x_dur[x->x_state];
    float frac = (f-dur_m1)/(dur-dur_m1);
    val = interp_lin(frac, b, c);
    if(x->x_send != &s_)
        pd_float(x->x_send->s_thing, val);
    outlet_float(x->x_obj.ob_outlet, val);
}

static void function_set_beeakpoints(t_function *x, int ac, t_atom* av){
    float tdur = 0;
    x->x_dur[0] = 0;
    x->x_n_states = ac >> 1;
    float* dur = x->x_dur;
    float* val = x->x_points;
    // get 1st value
    *val = atom_getfloat(av++);
    x->x_max_point = x->x_min_point = *val;
    *dur = 0.0;
    dur++;
    ac--;
    // get others
    while(ac--){
        tdur += atom_getfloat(av++);
        *dur++ = tdur;
        if(ac--){
            *++val = atom_getfloat(av++);
            if(*val > x->x_max_point)
                x->x_max_point = *val;
            if(*val < x->x_min_point)
                x->x_min_point = *val;
        }
        else{
            *++val = 0;
            if(*val > x->x_max_point)
                x->x_max_point = *val;
            if(*val < x->x_min_point)
                x->x_min_point = *val;
        }
    }
    if(x->x_min_point < x->x_min)
        x->x_min = x->x_min_point;
    if(x->x_max_point > x->x_max)
        x->x_max = x->x_max_point;
}

static void function_set(t_function *x, t_symbol* s, int ac,t_atom *av){
    if((ac >> 1) > MAX_SIZE){
        pd_error(x, "[function]: too many lines, maximum is %d", MAX_SIZE);
        return;
    }
    s = NULL;
    if(ac > 2 && ac % 2){
        function_set_beeakpoints(x, ac, av);
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
            function_update(x, x->x_glist);
    }
    else if(ac == 2){ // set a single point value
        int i = (int)av->a_w.w_float;
        av++;
        if(i < 0)
            i = 0;
        if(i > x->x_n_states)
            i = x->x_n_states;
        float v = av->a_w.w_float;
        x->x_points[i] = v;
        if(v < x->x_min_point)
            x->x_min_point = x->x_min = v;
        if(v > x->x_max_point)
            x->x_max_point = x->x_max = v;
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
            function_update(x, x->x_glist);
    }
    else
        post("[function] wrong format for 'set' message");
}

static void function_list(t_function *x, t_symbol* s, int ac, t_atom *av){
    if(!ac){
        function_bang(x);
        return;
    }
    if(ac == 1){
        if(av->a_type == A_FLOAT)
            function_float(x, atom_getfloat(av));
        return;
    }
    if((ac >> 1) > MAX_SIZE){
        pd_error(x, "[function]: too many lines, maximum is %d", MAX_SIZE);
        return;
    }
    s = NULL;
    if(ac > 2 && ac % 2){
        function_set_beeakpoints(x, ac, av);
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
            function_update(x, x->x_glist);
        outlet_list(x->x_obj.ob_outlet, &s_list, ac, av);
        if(x->x_send != &s_ && x->x_send->s_thing)
            pd_list(x->x_send->s_thing, &s_list, ac, av);
    }
    else if(ac == 2){ // set a single point value
        int i = (int)av->a_w.w_float;
        av++;
        if(i < 0)
            i = 0;
        if(i > x->x_n_states)
            i = x->x_n_states;
        float v = x->x_points[i] = av->a_w.w_float;
        if(v < x->x_min_point)
            x->x_min_point = x->x_min = v;
        if(v > x->x_max_point)
            x->x_max_point = x->x_max = v;
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
            function_update(x, x->x_glist);
        function_bang(x);
    }
    else
        post("[function] wrong format for 'list' message");
}

static void function_min(t_function *x, t_floatarg f){
    if(f <= x->x_min_point){
        if(x->x_min != f){
            x->x_min = f;
            if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
                function_update(x, x->x_glist);
        }
    }
}

static void function_max(t_function *x, t_floatarg f){
    if(f >= x->x_max_point){
        if(x->x_max != f){
            x->x_max = f;
            if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
                function_update(x, x->x_glist);
        }
    }
}

static void function_resize(t_function *x){
    if(x->x_max != x->x_max_point || x->x_min != x->x_min_point){
        x->x_max = x->x_max_point;
        x->x_min = x->x_min_point;
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist))
            function_update(x, x->x_glist);
    }
}

static void function_savestate(t_function *x, t_floatarg f){
    int state = (int)(f != 0);
    if(x->x_savestate != state)
        x->x_savestate = state;
}

static void function_dim(t_function *x, t_floatarg f1, t_floatarg f2){
    int width = f1 < FUNCTION_MINWIDTH ? FUNCTION_MINWIDTH : (int)f1;
    int height = f2 < FUNCTION_MINHEIGHT ? FUNCTION_MINHEIGHT : (int)f2;
    if(x->x_width != width || x->x_height != height){
        x->x_width = width;
        x->x_height = height;
        if(function_vis_check(x)){
            function_erase(x, x->x_glist);
            function_draw(x, x->x_glist);
            canvas_fixlinesfor(x->x_glist, (t_text*)x);
            function_draw_handle(x, x->x_edit);
        }
    }
}

static void function_height(t_function *x, t_floatarg f){
    function_dim(x, x->x_width, f);
}

static void function_width(t_function *x, t_floatarg f){
    function_dim(x, f, x->x_height);
}

static void function_send(t_function *x, t_symbol *s){
    t_symbol *snd = s == gensym("empty") ? &s_ : canvas_realizedollar(x->x_glist, s);
    if(x->x_send != snd){
        x->x_snd_raw = s;
        x->x_send = snd;
        x->x_snd_set = 1;
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist)){
            if(x->x_send == &s_)
                function_draw_iolet(x);
            else
                pdgui_vmess(0, "crs", glist_getcanvas(x->x_glist), "delete", x->x_tag_out);
        }
    }
}

static void function_receive(t_function *x, t_symbol *s){
    t_symbol *rcv = s == gensym("empty") ? &s_ : canvas_realizedollar(x->x_glist, s);
    if(rcv != x->x_receive){
        x->x_rcv_raw = s;
        x->x_rcv_set = 1;
        if(x->x_receive != &s_)
            pd_unbind(&x->x_obj.ob_pd, x->x_receive);
        if(rcv != &s_)
            pd_bind(&x->x_obj.ob_pd, rcv);
        x->x_receive = rcv;
        if(glist_isvisible(x->x_glist) && gobj_shouldvis((t_gobj *)x, x->x_glist)){
            if(x->x_receive == &s_)
                function_draw_iolet(x);
            else
                pdgui_vmess(0, "crs", glist_getcanvas(x->x_glist), "delete", x->x_tag_in);
        }
    }
}

/* ---- color helpers (mirror [knob]/[pad]) ---- */

/* parse "#rrggbb" back into the RGB byte array so save() stays RGB based */
static void function_parse_hex(unsigned char *color, const char *hex){
    if(hex && hex[0] == '#' && strlen(hex) == 7){
        char pair[3] = {0};
        pair[0] = hex[1]; pair[1] = hex[2];
        color[0] = (unsigned char)strtol(pair, NULL, 16);
        pair[0] = hex[3]; pair[1] = hex[4];
        color[1] = (unsigned char)strtol(pair, NULL, 16);
        pair[0] = hex[5]; pair[1] = hex[6];
        color[2] = (unsigned char)strtol(pair, NULL, 16);
    }
}

/* send color string to Tcl; `which` = 0 for bg, 1 for fg */
static void function_send_color_validate(t_function *x, const char *color, int which){
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

static void function_validate(t_function *x, t_floatarg v, t_symbol *color, t_floatarg which){
    if(!(int)v){
        pd_error(0, "[function] invalid color '%s'", color->s_name);
        return;
    }
    if((int)which == 0){ // bg
        function_parse_hex(x->x_bgcolor, color->s_name);
        if(function_vis_check(x) && !x->x_transparent){
            t_canvas *cv = glist_getcanvas(x->x_glist);
            pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_rect, "-fill", color->s_name);
            pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_dots, "-fill", color->s_name);
        }
    }
    else{ // fg
        function_parse_hex(x->x_fgcolor, color->s_name);
        if(function_vis_check(x)){
            t_canvas *cv = glist_getcanvas(x->x_glist);
            pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_line, "-fill", color->s_name);
            pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_dots, "-outline", color->s_name);
        }
    }
}

static void function_fgcolor(t_function *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(!ac)
        return;
    x->x_theme = 0; // explicit color overrides theme
    if(av->a_type == A_SYMBOL){
        function_send_color_validate(x, atom_getsymbol(av)->s_name, 1);
        return;
    }
    if(ac < 3)
        return;
    int r = (int)atom_getfloatarg(0, ac, av);
    int g = (int)atom_getfloatarg(1, ac, av);
    int b = (int)atom_getfloatarg(2, ac, av);
    r = r < 0 ? 0 : r > 255 ? 255 : r;
    g = g < 0 ? 0 : g > 255 ? 255 : g;
    b = b < 0 ? 0 : b > 255 ? 255 : b;
    if(x->x_fgcolor[0] != r || x->x_fgcolor[1] != g || x->x_fgcolor[2] != b){
        x->x_fgcolor[0] = r;
        x->x_fgcolor[1] = g;
        x->x_fgcolor[2] = b;
        if(function_vis_check(x)){
            t_canvas *cv = glist_getcanvas(x->x_glist);
            char color[20];
            sprintf(color, "#%2.2x%2.2x%2.2x", r, g, b);
            pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_line, "-fill", color);
            pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_dots, "-outline", color);
        }
    }
}

static void function_bgcolor(t_function *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(!ac)
        return;
    x->x_theme = 0; // explicit color overrides theme
    if(av->a_type == A_SYMBOL){
        function_send_color_validate(x, atom_getsymbol(av)->s_name, 0);
        return;
    }
    if(ac < 3)
        return;
    int r = (int)atom_getfloatarg(0, ac, av);
    int g = (int)atom_getfloatarg(1, ac, av);
    int b = (int)atom_getfloatarg(2, ac, av);
    r = r < 0 ? 0 : r > 255 ? 255 : r;
    g = g < 0 ? 0 : g > 255 ? 255 : g;
    b = b < 0 ? 0 : b > 255 ? 255 : b;
    if(x->x_bgcolor[0] != r || x->x_bgcolor[1] != g || x->x_bgcolor[2] != b){
        x->x_bgcolor[0] = r;
        x->x_bgcolor[1] = g;
        x->x_bgcolor[2] = b;
        if(function_vis_check(x) && !x->x_transparent){
            t_canvas *cv = glist_getcanvas(x->x_glist);
            char color[20];
            sprintf(color, "#%2.2x%2.2x%2.2x", r, g, b);
            pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_rect, "-fill", color);
            pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_dots, "-fill", color);
        }
    }
}

static void function_colors(t_function *x, t_symbol *bg, t_symbol *fg){
    t_atom at[1];
    SETSYMBOL(at, bg);
    function_bgcolor(x, NULL, 1, at);
    SETSYMBOL(at, fg);
    function_fgcolor(x, NULL, 1, at);
}

static void function_theme(t_function *x, t_floatarg f){
    int theme = (int)(f != 0);
    if(theme == x->x_theme)
        return;
    x->x_theme = theme;
    if(function_vis_check(x)){
        char bgcolor[20], fgcolor[20];
        function_get_bgcolor(x, bgcolor, sizeof(bgcolor));
        function_get_fgcolor(x, fgcolor, sizeof(fgcolor));
        t_canvas *cv = glist_getcanvas(x->x_glist);
        pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_rect, "-fill",
            x->x_transparent ? "" : bgcolor);
        pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_line, "-fill", fgcolor);
        pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_dots, "-fill",
            x->x_transparent ? "" : bgcolor);
        pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_dots, "-outline", fgcolor);
    }
}

static void function_transparent(t_function *x, t_floatarg f){
    int transp = (int)(f != 0);
    if(transp == x->x_transparent)
        return;
    x->x_transparent = transp;
    if(function_vis_check(x)){
        char bgcolor[20];
        function_get_bgcolor(x, bgcolor, sizeof(bgcolor));
        t_canvas *cv = glist_getcanvas(x->x_glist);
        pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_rect, "-fill",
            transp ? "" : bgcolor);
        pdgui_vmess(0, "crs rs", cv, "itemconfigure", x->x_tag_dots, "-fill",
            transp ? "" : bgcolor);
    }
}

// --------------------- handle ---------------------------------------------------
static void handle__click_callback(t_handle *sh, t_floatarg f, t_floatarg xarg, t_floatarg yarg){
    int click = (int)f;
    t_function *x = sh->h_master;
    if(sh->h_dragon && click == 0){
        char buf[128];
        snprintf(buf, sizeof(buf), ".x%lx.c delete %s", (unsigned long)x->x_cv, sh->h_outlinetag);
        pdgui_vmess(buf, NULL);
        int width = x->x_width + sh->h_dx;
        int height = x->x_height + sh->h_dy;
        width = width < FUNCTION_MINWIDTH ? FUNCTION_MINWIDTH : width;
        height = height < FUNCTION_MINHEIGHT ? FUNCTION_MINHEIGHT : height;
        if(width != x->x_width || height != x->x_height){
            t_atom undo[2], redo[2];
            SETFLOAT(undo+0, x->x_width);
            SETFLOAT(undo+1, x->x_height);
            SETFLOAT(redo+0, width);
            SETFLOAT(redo+1, height);
            pd_undo_set_objectstate(x->x_glist, (t_pd*)x, gensym("dim"), 2, undo, 2, redo);
            function_dim(x, width, height);
            canvas_dirty(x->x_glist, 1);
        }
        function_draw_handle(x, 1); // also restores the handle's highlight color
    }
    else if(!sh->h_dragon && click){
        int x1, y1, x2, y2;
        function_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        pdgui_vmess(0, "crr iiii rk ri rs",
            x->x_cv, "create", "rectangle",
            x1, y1, x2, y2,
            "-outline", THISGUI->i_selectcolor,
            "-width", FUNCTION_SELBDWIDTH,
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
        t_function *x = sh->h_master;
        int dx = (int)xarg - sh->h_dragx, dy = (int)yarg - sh->h_dragy;
        if(x->x_width + dx < FUNCTION_MINWIDTH)
            dx = FUNCTION_MINWIDTH - x->x_width;
        if(x->x_height + dy < FUNCTION_MINHEIGHT)
            dy = FUNCTION_MINHEIGHT - x->x_height;
        int x1, y1, x2, y2;
        function_getrect((t_gobj *)x, x->x_glist, &x1, &y1, &x2, &y2);
        char buf[128];
        snprintf(buf, sizeof(buf), ".x%lx.c coords %s %d %d %d %d",
            (unsigned long)x->x_cv, sh->h_outlinetag, x1, y1, x2 + dx, y2 + dy);
        pdgui_vmess(buf, NULL);
        sh->h_dx = dx;
        sh->h_dy = dy;
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
            if(function_vis_check(p->p_cnv)){
                t_canvas *cv = glist_getcanvas(p->p_cnv->x_glist);
                function_draw_handle(p->p_cnv, edit);
                if(edit)
                    function_draw_iolet(p->p_cnv);
                else{
                    pdgui_vmess(0, "crs", cv, "delete", p->p_cnv->x_tag_in);
                    pdgui_vmess(0, "crs", cv, "delete", p->p_cnv->x_tag_out);
                }
            }
        }
    }
}

static void edit_proxy_free(t_edit_proxy *p){
    pd_unbind(&p->p_obj.ob_pd, p->p_sym);
    clock_free(p->p_clock);
    pd_free(&p->p_obj.ob_pd);
}

static t_edit_proxy * edit_proxy_new(t_function *x, t_symbol *s){
    t_edit_proxy *p = (t_edit_proxy*)pd_new(edit_proxy_class);
    p->p_cnv = x;
    pd_bind(&p->p_obj.ob_pd, p->p_sym = s);
    p->p_clock = clock_new(p, (t_method)edit_proxy_free);
    return(p);
}

static void function_free(t_function *x){
    if(x->x_receive != &s_)
         pd_unbind(&x->x_obj.ob_pd, x->x_receive);
    pd_unbind(&x->x_obj.ob_pd, x->x_bindname);
    if(x->x_handle){
        pd_unbind(x->x_handle, ((t_handle *)x->x_handle)->h_bindsym);
        pd_free(x->x_handle);
    }
    x->x_proxy->p_cnv = NULL;
    clock_delay(x->x_proxy->p_clock, 0);
    pdgui_stub_deleteforkey(x);
}

///////////////////// NEW / FREE / SETUP /////////////////////
static void *function_new(t_symbol *s, int ac, t_atom* av){
    t_function *x = (t_function *)pd_new(function_class);
    outlet_new(&x->x_obj, &s_anything);
    t_symbol *sym = s; // get rid of warning
    x->x_state = 0;
    x->x_grabbed = 0;
    t_canvas *cv = canvas_getcurrent();
    x->x_glist = (t_glist*)cv;
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING-1, ".x%lx", (unsigned long)cv);
    buf[MAXPDSTRING-1] = 0;
    x->x_proxy = edit_proxy_new(x, gensym(buf));
    x->x_edit = cv->gl_edit;
    x->x_cv = glist_getcanvas(x->x_glist);
    x->x_points = getbytes((MAX_SIZE+1)*sizeof(float));
    x->x_dur = getbytes((MAX_SIZE+1)*sizeof(float));
    int envset = 0;
    float initialDuration = 0;
// Default Args
    x->x_width = 200;
    x->x_height = 100;
    x->x_rcv_set = x->x_snd_set = x->x_flag = x->x_s_flag = x->x_r_flag = 0;
    x->x_receive = x->x_rcv_raw = x->x_send = x->x_snd_raw = &s_;
    x->x_savestate = 0;
    x->x_theme = 0;
    x->x_transparent = 0;
    x->x_min = 0;
    x->x_max = 1;
    x->x_bgcolor[0] = x->x_bgcolor[1] = x->x_bgcolor[2] = 220;
    x->x_fgcolor[0] = x->x_fgcolor[1] = x->x_fgcolor[2] = 50;
    t_atom a[3];
    SETFLOAT(a, 0);
    SETFLOAT(a+1, 1000);
    SETFLOAT(a+2, 0);
////////////////////////////////// GET ARGS ///////////////////////////////////////////
    if(ac && av->a_type == A_FLOAT){ // 1ST Width
        int w = (int)av->a_w.w_float;
        x->x_width = w < 40 ? 40 : w; // min width is 40
        ac--, av++;
        if(ac && av->a_type == A_FLOAT){ // 2ND Height
            int h = (int)av->a_w.w_float;
            x->x_height = h < 20 ? 20 : h; // min height is 20
            ac--, av++;
            if(ac && av->a_type == A_SYMBOL){ // 3RD Send
                if(av->a_w.w_symbol == gensym("empty")){ //  sets empty symbol
                    ac--, av++;
                }
                else{
                    x->x_send = av->a_w.w_symbol;
                    ac--, av++;
                }
                if(ac && av->a_type == A_SYMBOL){ // 4TH Receive
                    if(av->a_w.w_symbol == gensym("empty")){ //  sets empty symbol
                        ac--, av++;
                    }
                    else{
                        pd_bind(&x->x_obj.ob_pd, x->x_receive = av->a_w.w_symbol);
                        ac--, av++;
                    }
                    if(ac && av->a_type == A_FLOAT){ // 5TH Min
                        x->x_min = av->a_w.w_float;
                        ac--, av++;
                        if(ac && av->a_type == A_FLOAT){ // 6TH Max
                            x->x_max = av->a_w.w_float;
                            ac--, av++;
                            if(ac && av->a_type == A_FLOAT){ // BG Red
                                x->x_bgcolor[0] = (unsigned char)av->a_w.w_float;
                                ac--, av++;
                                if(ac && av->a_type == A_FLOAT){ // BG Green
                                    x->x_bgcolor[1] = (unsigned char)av->a_w.w_float;
                                    ac--, av++;
                                    if(ac && av->a_type == A_FLOAT){ // BG Blue
                                        x->x_bgcolor[2] = (unsigned char)av->a_w.w_float;
                                        ac--, av++;
                                        if(ac && av->a_type == A_FLOAT){ // FG Red
                                            x->x_fgcolor[0] = (unsigned char)av->a_w.w_float;
                                            ac--, av++;
                                            if(ac && av->a_type == A_FLOAT){ // FG Green
                                                x->x_fgcolor[1] = (unsigned char)av->a_w.w_float;
                                                ac--, av++;
                                                if(ac && av->a_type == A_FLOAT){ // FG Blue
                                                    x->x_fgcolor[2] = (unsigned char)av->a_w.w_float;
                                                    ac--, av++;
                                                    if(ac && av->a_type == A_FLOAT){ // Init
                                                        x->x_savestate = (int)(av->a_w.w_float != 0);
                                                        ac--, av++;
                                                        if(ac && av->a_type == A_FLOAT){ // Theme
                                                            x->x_theme = (int)(av->a_w.w_float != 0);
                                                            ac--, av++;
                                                            if(ac && av->a_type == A_FLOAT){ // Transparent
                                                                x->x_transparent = (int)(av->a_w.w_float != 0);
                                                                ac--, av++;
                                                                if(ac && av->a_type == A_FLOAT){ // placeholder
                                                                    ac--, av++;
                                                                    if(ac && av->a_type == A_FLOAT){ // Set Env
                                                                        int i = 0;
                                                                        int j = ac;
                                                                        while(j && (av+i)->a_type == A_FLOAT){
                                                                            i++; j--;
                                                                        }
                                                                        if(i % 2 == 0)
                                                                            pd_error(x, "[function]: needs an odd list of floats");
                                                                        else if(i >> 1 > MAX_SIZE){
                                                                            pd_error(x, "[function]: too many lines, maximum is %d", MAX_SIZE);
                                                                            goto errstate;
                                                                        }
                                                                        else{
                                                                            envset = 1;
                                                                            if(x->x_savestate)
                                                                                function_set_beeakpoints(x, i, av);
                                                                            else
                                                                                function_set_beeakpoints(x, 3, a);
                                                                        }
                                                                        av+=i, ac-=i;
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
    }
    while(ac > 0){
        if(av->a_type == A_SYMBOL){
            sym = atom_getsymbolarg(0, ac, av);
            if(sym == gensym("-duration")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    x->x_flag = 1;
                    float curfloat = atom_getfloatarg(1, ac, av);
                    initialDuration = curfloat < 0 ? 0 : curfloat;
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(sym == gensym("-savestate")){
                x->x_flag = 1;
                x->x_savestate = 1;
                ac--, av++;
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
            else if(sym == gensym("-width")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    x->x_flag = 1;
                    float curfloat = atom_getfloatarg(1, ac, av);
                    x->x_width = curfloat < 40 ? 40 : curfloat; // min width is 40
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(sym == gensym("-height")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    x->x_flag = 1;
                    float curfloat = atom_getfloatarg(1, ac, av);
                    x->x_height = curfloat < 20 ? 20 : curfloat; // min width is 20
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(sym == gensym("-send")){
                if(ac >= 2 && (av+1)->a_type == A_SYMBOL){
                    x->x_flag = x->x_s_flag = 1;
                    x->x_send = atom_getsymbolarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(sym == gensym("-receive")){
                if(ac >= 2 && (av+1)->a_type == A_SYMBOL){
                    x->x_flag = x->x_r_flag = 1;
                    pd_bind(&x->x_obj.ob_pd, x->x_receive = atom_getsymbolarg(1, ac, av));
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(sym == gensym("-min")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    x->x_flag = 1;
                    x->x_min = atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(sym == gensym("-max")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    x->x_flag = 1;
                    x->x_max = atom_getfloatarg(1, ac, av);
                    ac-=2, av+=2;
                }
                else goto errstate;
            }
            else if(sym == gensym("-bgcolor")){
                if(ac >= 4 && (av+1)->a_type == A_FLOAT
                   && (av+2)->a_type == A_FLOAT
                   && (av+3)->a_type == A_FLOAT){
                    x->x_flag = 1;
                    int r = (int)atom_getfloatarg(1, ac, av);
                    int g = (int)atom_getfloatarg(2, ac, av);
                    int b = (int)atom_getfloatarg(3, ac, av);
                    x->x_bgcolor[0] = r < 0 ? 0 : r > 255 ? 255 : r;
                    x->x_bgcolor[1] = g < 0 ? 0 : g > 255 ? 255 : g;
                    x->x_bgcolor[2] = b < 0 ? 0 : b > 255 ? 255 : b;
                    ac-=4, av+=4;
                }
                else goto errstate;
            }
            else if(sym == gensym("-fgcolor")){
                if(ac >= 4 && (av+1)->a_type == A_FLOAT
                   && (av+2)->a_type == A_FLOAT
                   && (av+3)->a_type == A_FLOAT){
                    x->x_flag = 1;
                    int r = (int)atom_getfloatarg(1, ac, av);
                    int g = (int)atom_getfloatarg(2, ac, av);
                    int b = (int)atom_getfloatarg(3, ac, av);
                    x->x_fgcolor[0] = r < 0 ? 0 : r > 255 ? 255 : r;
                    x->x_fgcolor[1] = g < 0 ? 0 : g > 255 ? 255 : g;
                    x->x_fgcolor[2] = b < 0 ? 0 : b > 255 ? 255 : b;
                    ac-=4, av+=4;
                }
                else goto errstate;
            }
            else if(sym == gensym("-set")){
                if(ac >= 4 && (av+1)->a_type == A_FLOAT
                   && (av+2)->a_type == A_FLOAT
                   && (av+3)->a_type == A_FLOAT){
                    x->x_flag = 1;
                    ac--, av++;
                    int i = 0, j = ac;
                    while(j && (av+i)->a_type == A_FLOAT){
                        i++;
                        j--;
                    }
                    if(i % 2 == 0)
                        pd_error(x, "[function]: needs an odd list of floats");
                    else if(i >> 1 > MAX_SIZE){
                        pd_error(x, "[function]: too many lines, maximum is %d", MAX_SIZE);
                        goto errstate;
                    }
                    else{
                        envset = 1;
                        function_set_beeakpoints(x, i, av);
                    }
                    av+=i, ac -= i;
                }
                else goto errstate;
            }
            else goto errstate;
        }
        else goto errstate;
    };
    if(!envset)
        function_set_beeakpoints(x, 3, a);
// set min/max
    float temp = x->x_max;
    if(x->x_min > x->x_max){
        x->x_max = x->x_min;
        x->x_min = temp;
    }
    else if(x->x_max == x->x_min){
        if(x->x_max == 0) // max & min = 0
            x->x_max = 1;
        else{
            if(x->x_max > 0){
                x->x_min = 0; // set min to 0
                if(x->x_max < 1)
                    x->x_max = 1; // set max to 1
            }
            else
                x->x_max = 0;
        }
    }
    if(initialDuration > 0)
        function_duration(x, initialDuration);
// tags
    sprintf(x->x_tag_obj, "%pOBJ", x);
    sprintf(x->x_tag_rect, "%pRECT", x);
    sprintf(x->x_tag_line, "%pLINE", x);
    sprintf(x->x_tag_dots, "%pDOTS", x);
    sprintf(x->x_tag_in, "%pIN", x);
    sprintf(x->x_tag_out, "%pOUT", x);
    sprintf(x->x_tag_IO, "%pIO", x);
    sprintf(x->x_tag_handle, "%pHANDLE", x);
// bindname (needed for the Tcl color validation round-trip)
    snprintf(buf, MAXPDSTRING-1, "#%lx", (long)x);
    buf[MAXPDSTRING-1] = 0;
    pd_bind(&x->x_obj.ob_pd, x->x_bindname = gensym(buf));
// handle
    x->x_handle = pd_new(handle_class);
    t_handle *sh = (t_handle *)x->x_handle;
    sh->h_master = x;
    sh->h_dragon = 0;
    sh->h_pathname[0] = 0;
    char hbuf[64];
    sprintf(hbuf, "_h%lx", (unsigned long)sh);
    pd_bind(x->x_handle, sh->h_bindsym = gensym(hbuf));
    sprintf(sh->h_outlinetag, "h%lx", (unsigned long)sh);
    return(x);
errstate:
    pd_error(x, "[function]: improper args");
    return(NULL);
}

void function_setup(void){
    function_class = class_new(gensym("function"), (t_newmethod)function_new,
        (t_method)function_free, sizeof(t_function), 0, A_GIMME,0);
    class_addlist(function_class, function_list);
//    class_addmethod(function_class, (t_method)function_loadbang, gensym("loadbang"), A_DEFFLOAT, 0);
    class_addmethod(function_class, (t_method)function_resize, gensym("resize"), 0);
    class_addmethod(function_class, (t_method)function_set, gensym("set"), A_GIMME, 0);
    class_addmethod(function_class, (t_method)function_i, gensym("i"), A_FLOAT, 0);
    class_addmethod(function_class, (t_method)function_savestate, gensym("savestate"), A_FLOAT, 0);
    class_addmethod(function_class, (t_method)function_dim, gensym("dim"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(function_class, (t_method)function_height, gensym("height"), A_FLOAT, 0);
    class_addmethod(function_class, (t_method)function_width, gensym("width"), A_FLOAT, 0);
    class_addmethod(function_class, (t_method)function_min, gensym("min"), A_FLOAT, 0);
    class_addmethod(function_class, (t_method)function_max, gensym("max"), A_FLOAT, 0);
    class_addmethod(function_class, (t_method)function_duration, gensym("duration"), A_FLOAT, 0);
    class_addmethod(function_class, (t_method)function_send, gensym("send"), A_SYMBOL, 0);
    class_addmethod(function_class, (t_method)function_receive, gensym("receive"), A_SYMBOL, 0);
    class_addmethod(function_class, (t_method)function_bgcolor, gensym("bgcolor"), A_GIMME, 0);
    class_addmethod(function_class, (t_method)function_fgcolor, gensym("fgcolor"), A_GIMME, 0);
    class_addmethod(function_class, (t_method)function_colors, gensym("colors"), A_SYMBOL, A_SYMBOL, 0);
    class_addmethod(function_class, (t_method)function_theme, gensym("theme"), A_FLOAT, 0);
    class_addmethod(function_class, (t_method)function_transparent, gensym("transparent"), A_FLOAT, 0);
    class_addmethod(function_class, (t_method)function_validate, gensym("_validate"),
        A_FLOAT, A_SYMBOL, A_FLOAT, 0);
    class_addmethod(function_class, (t_method)function_motion, gensym("motion"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(function_class, (t_method)function_key, gensym("key"), A_FLOAT, 0);
    edit_proxy_class = class_new(0, 0, 0, sizeof(t_edit_proxy), CLASS_NOINLET | CLASS_PD, 0);
    class_addanything(edit_proxy_class, edit_proxy_any);
    handle_class = class_new(gensym("_handle"), 0, 0, sizeof(t_handle), CLASS_PD, 0);
    class_addmethod(handle_class, (t_method)handle__click_callback, gensym("_click"), A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(handle_class, (t_method)handle__motion_callback, gensym("_motion"), A_FLOAT, A_FLOAT, 0);
    class_setwidget(function_class, &function_widgetbehavior);
    class_setsavefn(function_class, function_save);
    function_widgetbehavior.w_getrectfn  = function_getrect;
    function_widgetbehavior.w_displacefn = function_displace;
    function_widgetbehavior.w_selectfn   = function_select;
    function_widgetbehavior.w_deletefn   = function_delete;
    function_widgetbehavior.w_visfn      = function_vis;
    function_widgetbehavior.w_clickfn    = (t_clickfn)function_click;
    #include "../Extra/else_tcl_utils.h"
    pdgui_vmess(else_tcl_utils, NULL);
}
