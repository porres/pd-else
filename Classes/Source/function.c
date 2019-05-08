// based on [tof/brakpoints]

// memory realloc may crash but pointer seems ok! 'STATES' prevents it
#define STATES  100   // Remove this, make it work better...

#define BORDERWIDTH 3
#define IOHEIGHT 1

#include <m_pd.h>
#include <g_canvas.h>
#include <string.h>
#include <math.h>

t_widgetbehavior function_widgetbehavior;

static t_class *function_class;

typedef struct _function{
    t_object    x_obj;
    t_glist*    x_glist;
    int         x_state;
    int         x_n_states;
    t_float*    x_points;
    t_float*    x_duration;
    t_float     x_total_duration;
    t_symbol   *x_receive_sym;
    t_symbol   *x_send_sym;
    int         x_rcv_set;
    int         x_snd_set;
    int         x_flag;
    t_symbol   *x_rcv_unexpanded;
    t_symbol   *x_snd_unexpanded;
    t_float     x_min;
    t_float     x_max;
    t_float     x_min_point;
    t_float     x_max_point;
    t_int       x_states;   // bug ???
    unsigned char  x_fgcolor[3];
    unsigned char  x_bgcolor[3];
    int         x_width;
    int         x_init;
    int         x_height;
    int         x_numdoodles;
    int         x_grabbed; // for moving points
    int         x_shift; // move 100th
    float       x_pointer_x;
    float       x_pointer_y;
}t_function;

// REMOVE THIS!!! Only here for now to debug...
static void function_print(t_function* x){
    post("---===### function settings ###===---");
    post("x_n_states:     %d",   x->x_n_states);
    post("total duration:   %.2f", x->x_total_duration);
    post("min:              %.2f", x->x_min);
    post("max:              %.2f", x->x_max);
    
    post("args:");
    t_int i = 0;
    post("%d = %f", i, x->x_points[x->x_state = 0]);
    i++;
    t_int ac = x->x_n_states * 2 + 1;
    while(i < ac){
        t_float dur = x->x_duration[x->x_state+1] - x->x_duration[x->x_state];
        post("%d = %f", i, dur);
        i++;
        x->x_state++;
        post("%d = %f", i, x->x_points[x->x_state]);
        i++;
    }
    post("---===### function settings ###===---");
}

////////////////////////////////////////////////
static void function_set_min_max(t_function *x){
    t_float temp = x->x_max;
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
}

////// BANG  ///////////////////////////////////////////////////////////////////////////////////
static void function_bang(t_function *x){
    t_int ac = x->x_n_states * 2 + 1;
    t_atom at[ac];
// get 1st
    SETFLOAT(at, x->x_min_point = x->x_max_point = x->x_points[x->x_state = 0]);
    t_int i = 1;
// get rest
    while(i < ac){
        t_float dur = x->x_duration[x->x_state+1] - x->x_duration[x->x_state];
        SETFLOAT(at+i, dur); // duration
        i++;
        x->x_state++;
        t_float point = x->x_points[x->x_state];
        if(point < x->x_min_point)
            x->x_min_point = point;
        if(point > x->x_max_point)
            x->x_max_point = point;
        SETFLOAT(at+i, point);
        i++;
    }
    outlet_list(x->x_obj.ob_outlet, &s_list, ac, at);
    if(x->x_send_sym != &s_ && x->x_send_sym->s_thing)
        pd_list(x->x_send_sym->s_thing, &s_list, ac, at);
}

// GUI SHIT /////////////////////////////////////////////////////////////////////////////////////
static void function_set_new_size(t_function* x,int ns){
    if(ns > x->x_states){
        int newargs = ns*sizeof(t_float);
        x->x_duration = resizebytes(x->x_duration, x->x_states*sizeof(t_float), newargs);
        x->x_points = resizebytes(x->x_points, x->x_states*sizeof(t_float), newargs);
        x->x_states = ns;
    }
}

static void function_duration(t_function* x, t_float dur){
    if(dur < 1){
        post("function: minimum duration is 1 ms");
        return;
    }
    x->x_total_duration = dur;
    float f = dur/x->x_duration[x->x_n_states];
    for(t_int i = 1; i <= x->x_n_states; i++)
        x->x_duration[i] *= f;
}

static void function_generate(t_function *x, int ac, t_atom* av){
    t_float tdur = 0;
    x->x_duration[0] = 0;
    x->x_n_states = ac >> 1;
    function_set_new_size(x, ac >> 1);
    t_float* dur = x->x_duration;
    t_float* val = x->x_points;
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
        *val++;
        if(ac--){
            *val = atom_getfloat(av++);
            if(*val > x->x_max_point)
                x->x_max_point = *val;
            if(*val < x->x_min_point)
                x->x_min_point = *val;
        }
        else{
            *val = 0; // ??
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

// MORE GUI SHIT //////////////////////////////////////////////////////////////////////////
static void draw_inlet_and_outlet(t_function *x, t_glist *glist, int firsttime){
    int x_onset, y_onset;
    int xpos = text_xpix(&x->x_obj, glist);
    int ypos = text_ypix(&x->x_obj, glist);
// Inlet
    if(x->x_receive_sym == &s_){
        x_onset = xpos - BORDERWIDTH;
        y_onset = ypos - BORDERWIDTH + 1;
        if(firsttime)
            sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxi%d\n",
                     glist_getcanvas(glist),
                     x_onset,           y_onset,
                     x_onset + IOWIDTH, y_onset + IOHEIGHT,
                     x, 0);
        else
            sys_vgui(".x%lx.c coords %lxi%d %d %d %d %d\n",
                     glist_getcanvas(glist), x, 0,
                     x_onset,           y_onset,
                     x_onset + IOWIDTH, y_onset + IOHEIGHT);
    }
    if(x->x_send_sym == &s_){ // Outlet
        x_onset = xpos - BORDERWIDTH;
        y_onset = ypos + x->x_height - 1 + BORDERWIDTH; // + 2 here
        if(firsttime)
            sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %lxo%d \n",
                    glist_getcanvas(glist),
                    x_onset,           y_onset,
                    x_onset + IOWIDTH, y_onset - IOHEIGHT,
                    x, 0);
        else
            sys_vgui(".x%lx.c coords %lxo%d %d %d %d %d\n",
                    glist_getcanvas(glist), x, 0,
                    x_onset,           y_onset,
                    x_onset + IOWIDTH, y_onset - IOHEIGHT);
    }
}

static int function_x_next_doodle(t_function *x, struct _glist *glist, int xpos,int ypos){
    float xscale, yscale;
    int dxpos, dypos;
    float minval = 100000.0;
    float tval;
    int i;
    int insertpos = -1;
    float ySize = x->x_max - x->x_min;
    float yBase =  x->x_min;
    if(xpos >text_xpix(&x->x_obj, glist) + x->x_width)
        xpos = text_xpix(&x->x_obj, glist) + x->x_width;
    xscale = x->x_width/x->x_duration[x->x_n_states];
    yscale = x->x_height;
    dxpos = text_xpix(&x->x_obj, glist); // + BORDERWIDTH; // why not ???
    dypos = text_ypix(&x->x_obj, glist) + BORDERWIDTH;
    for(i = 0; i <= x->x_n_states; i++){
        float dx2 = (dxpos + (x->x_duration[i] * xscale)) - xpos;
        float dy2 = (dypos + yscale - ((x->x_points[i] - yBase) / ySize * yscale)) - ypos;
        dx2 *= dx2;
        dy2 *= dy2;
        tval = sqrt(dx2+dy2);
        if(tval <= minval){
            minval = tval;
            insertpos = i;
        }
    }
    if(minval > 8 && insertpos >= 0){ // decide if we want to make a new one
        while (((dxpos + (x->x_duration[insertpos] * xscale)) - xpos) < 0)
            insertpos++;
        while (((dxpos + (x->x_duration[insertpos-1] * xscale)) - xpos) >0)
            insertpos--;
        if(x->x_n_states + 1 >= x->x_states)
            function_set_new_size(x, x->x_states + 1);
        for(i = x->x_n_states; i >= insertpos; i--){
            x->x_duration[i + 1] = x->x_duration[i];
            x->x_points[i + 1] = x->x_points[i];
        }
        x->x_duration[insertpos] = (float)(xpos-dxpos)/x->x_width*x->x_duration[x->x_n_states++];
        x->x_pointer_x = xpos;
        x->x_pointer_y = ypos;
    }
    else{
        x->x_pointer_x = text_xpix(&x->x_obj,glist) + x->x_duration[insertpos]*x->x_width/x->x_duration[x->x_n_states];
        x->x_pointer_y = ypos;
    }
    x->x_grabbed = insertpos;
    return insertpos;
}

static void function_doodles_create(t_function *x, t_glist *glist){
    float xscale, yscale;
    int xpos, ypos;
    float ySize = x->x_max - x->x_min;
    float yBase =  x->x_min;
    float yvalue;
    xscale = x->x_width/x->x_duration[x->x_n_states];
    yscale = x->x_height;
    xpos = text_xpix(&x->x_obj, glist);
    ypos = (int)(text_ypix(&x->x_obj, glist) + x->x_height);
    char fgcolor[20];
    sprintf(fgcolor, "#%2.2x%2.2x%2.2x", x->x_fgcolor[0], x->x_fgcolor[1], x->x_fgcolor[2]);
    int i;
    for(i = 0; i <= x->x_n_states; i++){
        yvalue = (x->x_points[i] - yBase) / ySize * yscale;
        sys_vgui(".x%lx.c create oval %d %d %d %d  -tags %lxD%d -fill %s\n",
                 (unsigned long)glist_getcanvas(glist),
                 (int)(xpos + (x->x_duration[i] * xscale) - 3),
                 (int)(ypos - yvalue - 3),
                 (int)(xpos + (x->x_duration[i] * xscale) + 3),
                 (int)(ypos - yvalue + 3),
                 (unsigned long)x,
                 i,
                 fgcolor);
    }
    x->x_numdoodles = i;
}

static void function_delete_doodles(t_function *x, t_glist *glist){
    for(t_int i = 0; i <= x->x_numdoodles; i++)
        sys_vgui(".x%lx.c delete %lxD%d\n",glist_getcanvas(glist), x, i);
}

static void function_update_doodles(t_function *x, t_glist *glist){
    function_delete_doodles(x, glist);
    function_doodles_create(x, glist); // LATER only create new doodles if necessary
}

static void function_create(t_function *x, t_glist *glist){
    float xscale, yscale;
    int xpos, ypos;
    float ySize = x->x_max - x->x_min;
    float yBase =  x->x_min;
    xpos = text_xpix(&x->x_obj, glist);
    ypos = (int) text_ypix(&x->x_obj, glist);
    char bgcolor[20];
    sprintf(bgcolor, "#%2.2x%2.2x%2.2x", x->x_bgcolor[0], x->x_bgcolor[1], x->x_bgcolor[2]);
    sys_vgui(".x%lx.c create rectangle %d %d %d %d  -tags %lxS -fill %s -width %d\n",
             glist_getcanvas(glist),
             xpos - BORDERWIDTH,
             ypos - BORDERWIDTH,
             xpos + x->x_width + BORDERWIDTH,
             ypos + x->x_height + BORDERWIDTH, // + 2 here
             x,
             bgcolor,
             1); // BORDER_LINE_SIZE
    xscale = x->x_width / x->x_duration[x->x_n_states];
    yscale = x->x_height;
    sys_vgui(".x%lx.c create line", glist_getcanvas(glist));
    for(t_int i = 0; i <= x->x_n_states; i++){
        sys_vgui(" %d %d ", (int)(xpos + x->x_duration[i]*xscale),
                 (int)(ypos + x->x_height - (x->x_points[i]-yBase) / ySize*yscale));
    }
    char fgcolor[20];
    sprintf(fgcolor, "#%2.2x%2.2x%2.2x", x->x_fgcolor[0], x->x_fgcolor[1], x->x_fgcolor[2]);
    sys_vgui(" -tags %lxP -fill %s -width %d\n", x, fgcolor, 2);
    function_doodles_create(x, glist);
}

static void function_update(t_function *x, t_glist *glist){
    float xscale, yscale;
    int xpos = text_xpix(&x->x_obj, glist);
    int ypos = text_ypix(&x->x_obj, glist);
    float ySize = x->x_max - x->x_min;
    float yBase =  x->x_min;
    sys_vgui(".x%lx.c coords %lxS %d %d %d %d\n",
             glist_getcanvas(glist), x,
             xpos - BORDERWIDTH, ypos - BORDERWIDTH,
             xpos + x->x_width + BORDERWIDTH,
             ypos + x->x_height + BORDERWIDTH); // + 2 here
    xscale = x->x_width / x->x_duration[x->x_n_states];
    yscale = x->x_height;
    sys_vgui(".x%lx.c coords %lxP", glist_getcanvas(glist), x);
    for(t_int i = 0; i <= x->x_n_states; i++)
        sys_vgui(" %d %d ",(int)(xpos + x->x_duration[i]*xscale),
                 (int)(ypos + x->x_height - (x->x_points[i] - yBase) / ySize*yscale));
    sys_vgui("\n");
    function_update_doodles(x, glist);
    draw_inlet_and_outlet(x, glist, 0); // again ???
}

static void function_drawme(t_function *x, t_glist *glist, int firsttime){
    if(firsttime)
        function_create(x, glist);
    else
        function_update(x, glist);
    draw_inlet_and_outlet(x, glist, firsttime);
}

static void function_erase(t_function* x, t_glist* glist){
    sys_vgui(".x%lx.c delete %lxS\n", glist_getcanvas(glist), x);
    sys_vgui(".x%lx.c delete %lxP\n", glist_getcanvas(glist), x);
    sys_vgui(".x%lx.c delete %lxi0\n", glist_getcanvas(glist), x);
    sys_vgui(".x%lx.c delete %lxo0\n", glist_getcanvas(glist), x);
    sys_vgui(".x%lx.c delete %lxo1\n", glist_getcanvas(glist), x);
    sys_vgui(".x%lx.c delete %lxo2\n", glist_getcanvas(glist), x);
    function_delete_doodles(x,glist);
}

/* ------------------------ widgetbehaviour----------------------------- */
static void function_getrect(t_gobj *z, t_glist *owner, int *xp1, int *yp1, int *xp2, int *yp2){
    int width, height;
    t_function* s = (t_function*)z;
    width = s->x_width + BORDERWIDTH;
    height = s->x_height + BORDERWIDTH; // + 2 here
    *xp1 = text_xpix(&s->x_obj,owner) - BORDERWIDTH;
    *yp1 = text_ypix(&s->x_obj,owner) - BORDERWIDTH;
    *xp2 = text_xpix(&s->x_obj,owner) + width ; // + 4
    *yp2 = text_ypix(&s->x_obj,owner) + height ; // + 4
}

static void function_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_function *x = (t_function *)z;
    x->x_obj.te_xpix += dx; // x movement
    x->x_obj.te_ypix += dy; // y movement
// MOVE THE BLUE LINE
    sys_vgui(".x%lx.c coords %xSEL %d %d %d %d \n",
             glist_getcanvas(glist), x,
             x->x_obj.te_xpix - BORDERWIDTH,
             x->x_obj.te_ypix - BORDERWIDTH,
             x->x_obj.te_xpix + x->x_width + BORDERWIDTH,
             x->x_obj.te_ypix + x->x_height + BORDERWIDTH);
    function_drawme(x, glist, 0);
    canvas_fixlinesfor(glist, (t_text*) x);
}

static void function_select(t_gobj *z, t_glist *glist, int state){
    t_function *x = (t_function *)z;
    t_canvas * canvas = glist_getcanvas(glist);
    if(state){
        sys_vgui(".x%lx.c create rectangle %d %d %d %d -tags %xSEL -outline blue\n",
        canvas,
        x->x_obj.te_xpix - BORDERWIDTH,
        x->x_obj.te_ypix - BORDERWIDTH,
        x->x_obj.te_xpix + x->x_width + BORDERWIDTH,
        x->x_obj.te_ypix + x->x_height + BORDERWIDTH,
        x);
    }
    else
        sys_vgui(".x%lx.c delete %xSEL\n", canvas, x);
}

static void function_delete(t_gobj *z, t_glist *glist){
    t_text *x = (t_text *)z;
    canvas_deletelinesfor(glist, x);
}

static void function_vis(t_gobj *z, t_glist *glist, int vis){
    t_function* s = (t_function*)z;
    if(vis)
        function_drawme(s, glist, 1);
    else
        function_erase(s, glist);
}

static void function_followpointer(t_function* x,t_glist* glist){
    float dur;
    float xscale = x->x_duration[x->x_n_states] / x->x_width;
    float ySize = x->x_max - x->x_min;
    float yBase =  x->x_min;
    if((x->x_grabbed > 0) && (x->x_grabbed < x->x_n_states)){
        dur = (x->x_pointer_x - text_xpix(&x->x_obj,glist))*xscale;
        if(dur < x->x_duration[x->x_grabbed-1])
            dur = x->x_duration[x->x_grabbed-1];
        if(dur >x->x_duration[x->x_grabbed+1])
            dur = x->x_duration[x->x_grabbed+1];
        x->x_duration[x->x_grabbed] = dur;
    }
    float grabbed = (1.0f - (x->x_pointer_y - (float)text_ypix(&x->x_obj,glist))/(float)x->x_height);
    if(grabbed < 0.0)
        grabbed = 0.0;
    else if(grabbed > 1.0)
        grabbed = 1.0;
    x->x_points[x->x_grabbed] = grabbed * ySize + yBase;
    function_bang(x);
}

static void function_motion(t_function *x, t_floatarg dx, t_floatarg dy){
    if(x->x_shift){
        x->x_pointer_x += dx / 1000.f;
        x->x_pointer_y += dy / 1000.f;
    }
    else{
        x->x_pointer_x += dx;
        x->x_pointer_y += dy;
    }
    function_followpointer(x,x->x_glist);
/* "resizing" (MAKE THIS HAPPEN IN EDIT MODE AND WHEN CLICKING THE BOTTOM RIGHT CORNER) !!!!!!!!
    else{
        x->x_width += dx;
        x->x_height += dy;
    } */
    function_update(x, x->x_glist);
}

static void function_key(t_function *x, t_floatarg f){
    if(f == 8.0 && x->x_grabbed < x->x_n_states &&  x->x_grabbed >0){
        for(t_int i = x->x_grabbed; i <= x->x_n_states; i++){
            x->x_duration[i] = x->x_duration[i+1];
            x->x_points[i] = x->x_points[i+1];
        }
        x->x_n_states--;
        x->x_grabbed--;
        function_update(x, x->x_glist);
        function_bang(x);
    }
}

static int function_newclick(t_function *x, struct _glist *glist,
            int xpos, int ypos, int shift, int alt, int dbl, int doit){
    alt = dbl = 0; // remove warning
// check if user wants to resize
    float wxpos = text_xpix(&x->x_obj, glist);
    float wypos = text_ypix(&x->x_obj, glist);
    if(doit){
        if((xpos >= wxpos) && (xpos <= wxpos + x->x_width) \
           && (ypos >= wypos ) && (ypos <= wypos + x->x_height)){
            function_x_next_doodle(x, glist, xpos, ypos);
            glist_grab(x->x_glist, &x->x_obj.te_g, (t_glistmotionfn) function_motion,
                (t_glistkeyfn) function_key, xpos, ypos);
            x->x_shift = shift;
            function_followpointer(x, glist);
            function_update(x, glist);
        }
    }
    return(1);
}

static void function_save(t_gobj *z, t_binbuf *b){
    t_function *x = (t_function *)z;
    t_binbuf *bb = x->x_obj.te_binbuf;
    int i;
    int n_args = binbuf_getnatom(bb); // number of arguments
    char buf[80];
    if(!x->x_snd_set){ // no send set, search arguments
        if(n_args > 0){ // we have arguments, let's search them
            if(x->x_flag){ // search for receive name in flags
                for(i = 0;  i < n_args; i++){
                    atom_string(binbuf_getvec(bb) + i, buf, 80);
                    if(!strcmp(buf, "-send")){
                        i++;
                        atom_string(binbuf_getvec(bb) + i, buf, 80);
                        x->x_snd_unexpanded = gensym(buf);
                        break;
                    }
                }
            }
            else{ // search receive argument
                int arg_n = 3; // receive argument number
                if(n_args >= arg_n){ // we have it, get it
                    atom_string(binbuf_getvec(bb) + arg_n, buf, 80);
                    x->x_snd_unexpanded = gensym(buf);
                }
            }
        }
    }
    if(x->x_snd_unexpanded == &s_) // if nothing found, set to "empty"
        x->x_snd_unexpanded = gensym("empty");
    if(!x->x_rcv_set){ // no receive set, search arguments
        if(n_args > 0){ // we have arguments, let's search them
            if(x->x_flag){ // search for receive name in flags
                for(i = 0;  i < n_args; i++){
                    atom_string(binbuf_getvec(bb) + i, buf, 80);
                    if(!strcmp(buf, "-receive")){
                        i++;
                        atom_string(binbuf_getvec(bb) + i, buf, 80);
                        post("buf = %s", buf);
                        x->x_rcv_unexpanded = gensym(buf);
                        break;
                    }
                }
            }
            else{ // search receive argument
                int arg_n = 4; // receive argument number
                if(n_args >= arg_n){ // we have it, get it
                    atom_string(binbuf_getvec(bb) + arg_n, buf, 80);
                    x->x_rcv_unexpanded = gensym(buf);
                }
            }
        }
    }
    if(x->x_rcv_unexpanded == &s_) // if nothing found, set to "empty"
        x->x_rcv_unexpanded = gensym("empty");
    binbuf_addv(b,
                "ssiis",
                gensym("#X"),
                gensym("obj"),
                (t_int)x->x_obj.te_xpix,
                (t_int)x->x_obj.te_ypix,
                atom_getsymbol(binbuf_getvec(x->x_obj.te_binbuf))
                );
    binbuf_addv(b, "iissffiiiiiiiiii",
                (t_int)x->x_width,
                (t_int)x->x_height,
                x->x_snd_unexpanded,
                x->x_rcv_unexpanded,
                x->x_min,
                x->x_max,
                (t_int)x->x_bgcolor[0],
                (t_int)x->x_bgcolor[1],
                (t_int)x->x_bgcolor[2],
                (t_int)x->x_fgcolor[0],
                (t_int)x->x_fgcolor[1],
                (t_int)x->x_fgcolor[2],
                x->x_init,
                0, // placeholder
                0, // placeholder
                0  // placeholder
                );
    binbuf_addv(b, "f",
                (t_float)x->x_points[x->x_state = 0]
                );
                i = 1;
                t_int ac = x->x_n_states * 2 + 1;
                while(i < ac){
                    t_float dur = x->x_duration[x->x_state+1] - x->x_duration[x->x_state];
                    binbuf_addv(b, "f",
                                (t_float)dur
                                );
                    i++;
                    x->x_state++;
                    binbuf_addv(b, "f",
                                (t_float)x->x_points[x->x_state]
                                );
                    i++;
                }
    binbuf_addv(b, ";");
}

///////////////////// METHODS /////////////////////
static void function_float(t_function *x, t_floatarg f){
    t_float val;
    if(f <= 0){
        val = x->x_points[0];
        outlet_float(x->x_obj.ob_outlet, val);
        if(x->x_send_sym != &s_)
            pd_float(x->x_send_sym->s_thing, f);
        return;
    }
    if(f >= 1){
        val = x->x_points[x->x_n_states];
        outlet_float(x->x_obj.ob_outlet, val);
        if(x->x_send_sym != &s_)
            pd_float(x->x_send_sym->s_thing, f);
        return;
    }
    f *= x->x_duration[x->x_n_states];
    if(x->x_state > x->x_n_states)
        x->x_state = x->x_n_states;
    while((x->x_state > 0) && (f < x->x_duration[x->x_state-1]))
        x->x_state--;
    while((x->x_state <  x->x_n_states) && (x->x_duration[x->x_state] < f))
        x->x_state++;
    val = x->x_points[x->x_state-1] + (f - x->x_duration[x->x_state-1])
    * (x->x_points[x->x_state] - x->x_points[x->x_state-1])
    / (x->x_duration[x->x_state] - x->x_duration[x->x_state-1]);
    outlet_float(x->x_obj.ob_outlet,val);
    if(x->x_send_sym != &s_)
        pd_float(x->x_send_sym->s_thing, val);
}

static void function_i(t_function *x, t_floatarg f){
    t_float val;
    if(f <= 0){
        val = x->x_points[0];
        outlet_float(x->x_obj.ob_outlet, val);
        if(x->x_send_sym != &s_)
            pd_float(x->x_send_sym->s_thing, f);
        return;
    }
    if(f >= x->x_duration[x->x_n_states]){
        val = x->x_points[x->x_n_states];
        outlet_float(x->x_obj.ob_outlet, val);
        if(x->x_send_sym != &s_)
            pd_float(x->x_send_sym->s_thing, f);
        return;
    }
    if(x->x_state > x->x_n_states)
        x->x_state = x->x_n_states;
    while((x->x_state > 0) && (f < x->x_duration[x->x_state-1]))
        x->x_state--;
    while((x->x_state <  x->x_n_states) && (x->x_duration[x->x_state] < f))
        x->x_state++;
    val = x->x_points[x->x_state-1] + (f - x->x_duration[x->x_state-1])
    * (x->x_points[x->x_state] - x->x_points[x->x_state-1])
    / (x->x_duration[x->x_state] - x->x_duration[x->x_state-1]);
    outlet_float(x->x_obj.ob_outlet,val);
    if(x->x_send_sym != &s_)
        pd_float(x->x_send_sym->s_thing, val);
}

static void function_set(t_function *x, t_symbol* s, int ac,t_atom *av){
    t_symbol *dummy = s;
    dummy = NULL;
    if(ac > 2 && ac % 2){
        function_generate(x, ac, av);
        if(glist_isvisible(x->x_glist))
            function_drawme(x, x->x_glist, 0);
    }
    else if(ac == 2){
        t_int i = (int)av->a_w.w_float;
        av++;
        if(i < 0)
            i = 0;
        if(i > x->x_n_states)
            i = x->x_n_states;
        t_float v = av->a_w.w_float;
        x->x_points[i] = v;
        if(v < x->x_min_point)
            x->x_min_point = x->x_min = v;
        if(v > x->x_max_point)
            x->x_max_point = x->x_max = v;
        if(glist_isvisible(x->x_glist))
            function_drawme(x, x->x_glist, 0);
    }
    else
        post("[function] wrong format for 'set' message");
}

static void function_list(t_function *x, t_symbol* s, int ac,t_atom *av){
    t_symbol *dummy = s;
    dummy = NULL;
    if(ac > 2 && ac % 2){
        function_generate(x, ac, av);
        if(glist_isvisible(x->x_glist))
            function_drawme(x, x->x_glist, 0);
        outlet_list(x->x_obj.ob_outlet, &s_list, ac, av);
        if(x->x_send_sym != &s_ && x->x_send_sym->s_thing)
            pd_list(x->x_send_sym->s_thing, &s_list, ac, av);
    }
    else if(ac == 2){
        t_int i = (int)av->a_w.w_float;
        av++;
        if(i < 0)
            i = 0;
        if(i > x->x_n_states)
            i = x->x_n_states;
        t_float v = av->a_w.w_float;
        x->x_points[i] = v;
        if(v < x->x_min_point)
            x->x_min_point = x->x_min = v;
        if(v > x->x_max_point)
            x->x_max_point = x->x_max = v;
        if(glist_isvisible(x->x_glist))
            function_drawme(x, x->x_glist, 0);
        function_bang(x);
    }
    else
        post("[function] wrong format for 'list' message");
}

static void function_min(t_function *x, t_floatarg f){
    if(f <= x->x_min_point){
        x->x_min = f;
        if(glist_isvisible(x->x_glist))
            function_update(x, x->x_glist);
    }
}

static void function_max(t_function *x, t_floatarg f){
    if(f >= x->x_max_point){
        x->x_max = f;
        if(glist_isvisible(x->x_glist))
            function_update(x, x->x_glist);
    }
}

static void function_resize(t_function *x){
    x->x_max = x->x_max_point;
    x->x_min = x->x_min_point;
    if(glist_isvisible(x->x_glist))
        function_update(x, x->x_glist);
}

static void function_init(t_function *x, t_floatarg f){
    x->x_init = (int)(f != 0);
}

static void function_height(t_function *x, t_floatarg f){
    x->x_height = f < 20 ? 20 : f;
    function_update(x, x->x_glist);
    canvas_fixlinesfor(x->x_glist, (t_text*) x);
}

static void function_width(t_function *x, t_floatarg f){
    x->x_width = f < 40 ? 40 : f;
    function_update(x, x->x_glist);
}

static void function_send(t_function *x, t_symbol *s){
    t_symbol *snd = canvas_realizedollar(x->x_glist, x->x_snd_unexpanded = s);
    if(s == gensym("empty"))
        snd = &s_;
    x->x_send_sym = snd;
    function_erase(x, x->x_glist);
    function_drawme(x, x->x_glist, 1);
}

static void function_set_send(t_function *x, t_symbol *s){
    x->x_snd_set = 1;
    function_send(x, s);
}

static void function_receive(t_function *x, t_symbol *s){
    t_symbol *rcv = canvas_realizedollar(x->x_glist, x->x_rcv_unexpanded = s);
    function_erase(x, x->x_glist);
    if(s == gensym("empty"))
        rcv = &s_;
    if(rcv != &s_){
        if(x->x_receive_sym != &s_)
            pd_unbind(&x->x_obj.ob_pd, x->x_receive_sym);
        pd_bind(&x->x_obj.ob_pd, x->x_receive_sym = rcv);
    }
    else{
        if(x->x_receive_sym != &s_)
            pd_unbind(&x->x_obj.ob_pd, x->x_receive_sym);
        x->x_receive_sym = rcv;
    }
    function_drawme(x, x->x_glist, 1);
}

static void function_set_receive(t_function *x, t_symbol *s){
    x->x_rcv_set = 1;
    function_receive(x, s);
}

static void function_fgcolor(t_function *x, t_float r, t_float g, t_float b){
    x->x_fgcolor[0] = r < 0 ? 0 : r > 255 ? 255 : (int)r;
    x->x_fgcolor[1] = g < 0 ? 0 : g > 255 ? 255 : (int)g;
    x->x_fgcolor[2] = b < 0 ? 0 : b > 255 ? 255 : (int)b;
    function_erase(x, x->x_glist);
    function_drawme(x, x->x_glist, 1);
    function_update(x, x->x_glist);
}

static void function_bgcolor(t_function *x, t_float r, t_float g, t_float b){
    x->x_bgcolor[0] = r < 0 ? 0 : r > 255 ? 255 : (int)r;
    x->x_bgcolor[1] = g < 0 ? 0 : g > 255 ? 255 : (int)g;
    x->x_bgcolor[2] = b < 0 ? 0 : b > 255 ? 255 : (int)b;
    function_erase(x, x->x_glist);
    function_drawme(x, x->x_glist, 1);
}

static void function_loadbang(t_function *x, t_floatarg action){
    if(action == LB_LOAD && x->x_init)
        function_bang(x);
}

///////////////////// NEW / FREE / SETUP /////////////////////
static void *function_new(t_symbol *s, int ac, t_atom* av){
    t_function *x = (t_function *)pd_new(function_class);
    outlet_new(&x->x_obj, &s_anything);
    t_symbol *cursym = s; // get rid of warning
    x->x_state = 0;
    x->x_states = STATES;
    x->x_grabbed = 0;
    x->x_glist = (t_glist*) canvas_getcurrent();
    x->x_points = getbytes(x->x_states*sizeof(t_float));
    x->x_duration = getbytes(x->x_states*sizeof(t_float));
    t_int envset = 0;
    t_float initialDuration = 0;
// Default Args
    x->x_width = 200;
    x->x_height = 100;
    x->x_rcv_set = x->x_snd_set = x->x_flag = 0;
    x->x_receive_sym = x->x_rcv_unexpanded = &s_;
    x->x_send_sym = x->x_snd_unexpanded = &s_;
    x->x_init = 0;
    x->x_min = 0;
    x->x_max = 1;
    x->x_bgcolor[0] = 220;
    x->x_bgcolor[1] = 220;
    x->x_bgcolor[2] = 220;
    x->x_fgcolor[0] = 50;
    x->x_fgcolor[1] = 50;
    x->x_fgcolor[2] = 50;
    t_atom a[3];
    SETFLOAT(a, 0);
    SETFLOAT(a+1, 1000);
    SETFLOAT(a+2, 0);
////////////////////////////////// GET ARGS ///////////////////////////////////////////
    t_int v;
    if(ac && av->a_type == A_FLOAT){ // 1ST Width
        v = (int)av->a_w.w_float;
        x->x_width = v < 40 ? 40 : v; // min width is 40
        ac--; av++;
        if(ac && av->a_type == A_FLOAT){ // 2ND Height
            v = (int)av->a_w.w_float;
            x->x_height = v < 20 ? 20 : v; // min height is 20
            ac--; av++;
            if(ac && av->a_type == A_SYMBOL){ // 3RD Send
                if(av->a_w.w_symbol == gensym("empty")){ //  sets empty symbol
                    ac--; av++;
                }
                else{
                    x->x_send_sym = av->a_w.w_symbol;
                    ac--; av++;
                }
                if(ac && av->a_type == A_SYMBOL){ // 4TH Receive
                    if(av->a_w.w_symbol == gensym("empty")){ //  sets empty symbol
                        ac--; av++;
                    }
                    else{
                        pd_bind(&x->x_obj.ob_pd, x->x_receive_sym = av->a_w.w_symbol);
                        ac--; av++;
                    }
                    if(ac && av->a_type == A_FLOAT){ // 5TH Min
                        x->x_min = av->a_w.w_float;
                        ac--; av++;
                        if(ac && av->a_type == A_FLOAT){ // 6TH Max
                            x->x_max = av->a_w.w_float;
                            ac--; av++;
                            if(ac && av->a_type == A_FLOAT){ // BG Red
                                x->x_bgcolor[0] = (unsigned char)av->a_w.w_float;
                                ac--; av++;
                                if(ac && av->a_type == A_FLOAT){ // BG Green
                                    x->x_bgcolor[1] = (unsigned char)av->a_w.w_float;
                                    ac--; av++;
                                    if(ac && av->a_type == A_FLOAT){ // BG Blue
                                        x->x_bgcolor[2] = (unsigned char)av->a_w.w_float;
                                        ac--; av++;
                                        if(ac && av->a_type == A_FLOAT){ // FG Red
                                            x->x_fgcolor[0] = (unsigned char)av->a_w.w_float;
                                            ac--; av++;
                                            if(ac && av->a_type == A_FLOAT){ // FG Green
                                                x->x_fgcolor[1] = (unsigned char)av->a_w.w_float;
                                                ac--; av++;
                                                if(ac && av->a_type == A_FLOAT){ // FG Blue
                                                    x->x_fgcolor[2] = (unsigned char)av->a_w.w_float;
                                                    ac--; av++;
                                                    if(ac && av->a_type == A_FLOAT){ // Init
                                                        x->x_init = (int)(av->a_w.w_float != 0);
                                                        ac--; av++;
                                                        if(ac && av->a_type == A_FLOAT){ // placeholder
                                                            ac--; av++;
                                                            if(ac && av->a_type == A_FLOAT){ // placeholder
                                                                ac--; av++;
                                                                if(ac && av->a_type == A_FLOAT){ // placeholder
                                                                    ac--; av++;
                                                                    if(ac && av->a_type == A_FLOAT){ // Set Env
                                                                        int i = 0;
                                                                        int j = ac;
                                                                        while(j && (av+i)->a_type == A_FLOAT){
                                                                            i++;
                                                                            j--;
                                                                        }
                                                                        if(i % 2){
                                                                            envset = 1;
                                                                            function_generate(x, i, av);
                                                                        }
                                                                        av += i;
                                                                        ac -= i;
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
            cursym = atom_getsymbolarg(0, ac, av);
            if(!strcmp(cursym->s_name, "-duration")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    t_float curfloat = atom_getfloatarg(1, ac, av);
                    initialDuration = curfloat < 0 ? 0 : curfloat;
                    ac -= 2;
                    av += 2;
                }
                else
                    goto errstate;
            }
            else if(!strcmp(cursym->s_name, "-init")){
                x->x_init = 1;
                ac--;
                av++;
            }
            else if(!strcmp(cursym->s_name, "-width")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    t_float curfloat = atom_getfloatarg(1, ac, av);
                    x->x_width = curfloat < 40 ? 40 : curfloat; // min width is 40
                    ac -= 2;
                    av += 2;
                }
                else
                    goto errstate;
            }
            else if(!strcmp(cursym->s_name, "-height")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    t_float curfloat = atom_getfloatarg(1, ac, av);
                    x->x_height = curfloat < 20 ? 20 : curfloat; // min width is 20
                    ac -= 2;
                    av += 2;
                }
                else
                    goto errstate;
            }
            else if(!strcmp(cursym->s_name, "-send")){
                if(ac >= 2 && (av+1)->a_type == A_SYMBOL){
                    x->x_flag = 1;
                    x->x_send_sym = atom_getsymbolarg(1, ac, av);
                    ac -= 2;
                    av += 2;
                }
                else
                    goto errstate;
            }
            else if(!strcmp(cursym->s_name, "-receive")){
                if(ac >= 2 && (av+1)->a_type == A_SYMBOL){
                    x->x_flag = 1;
                    pd_bind(&x->x_obj.ob_pd, x->x_receive_sym = atom_getsymbolarg(1, ac, av));
                    ac -= 2;
                    av += 2;
                }
                else
                    goto errstate;
            }
            else if(!strcmp(cursym->s_name, "-min")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    x->x_min = atom_getfloatarg(1, ac, av);
                    ac -= 2;
                    av += 2;
                }
                else
                    goto errstate;
            }
            else if(!strcmp(cursym->s_name, "-max")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    x->x_max = atom_getfloatarg(1, ac, av);
                    ac -= 2;
                    av += 2;
                }
                else
                    goto errstate;
            }
            else if(!strcmp(cursym->s_name, "-bgcolor")){
                if(ac >= 4 && (av+1)->a_type == A_FLOAT
                   && (av+2)->a_type == A_FLOAT
                   && (av+3)->a_type == A_FLOAT){
                    t_int r = (t_int)atom_getfloatarg(1, ac, av);
                    t_int g = (t_int)atom_getfloatarg(2, ac, av);
                    t_int b = (t_int)atom_getfloatarg(3, ac, av);
                    x->x_bgcolor[0] = r < 0 ? 0 : r > 255 ? 255 : r;
                    x->x_bgcolor[1] = g < 0 ? 0 : g > 255 ? 255 : g;
                    x->x_bgcolor[2] = b < 0 ? 0 : b > 255 ? 255 : b;
                    ac -= 4;
                    av += 4;
                }
                else
                    goto errstate;
            }
            else if(!strcmp(cursym->s_name, "-fgcolor")){
                if(ac >= 4 && (av+1)->a_type == A_FLOAT
                   && (av+2)->a_type == A_FLOAT
                   && (av+3)->a_type == A_FLOAT){
                    t_int r = (t_int)atom_getfloatarg(1, ac, av);
                    t_int g = (t_int)atom_getfloatarg(2, ac, av);
                    t_int b = (t_int)atom_getfloatarg(3, ac, av);
                    x->x_fgcolor[0] = r < 0 ? 0 : r > 255 ? 255 : r;
                    x->x_fgcolor[1] = g < 0 ? 0 : g > 255 ? 255 : g;
                    x->x_fgcolor[2] = b < 0 ? 0 : b > 255 ? 255 : b;
                    ac -= 4;
                    av += 4;
                }
                else
                    goto errstate;
            }
            else if(!strcmp(cursym->s_name, "-set")){
                if(ac >= 4 && (av+1)->a_type == A_FLOAT
                   && (av+2)->a_type == A_FLOAT
                   && (av+3)->a_type == A_FLOAT){
                    ac--;
                    av++;
                    int i = 0;
                    int j = ac;
                    while(j && (av+i)->a_type == A_FLOAT){
                        i++;
                        j--;
                    }
                    if(i % 2){
                        envset = 1;
                        function_generate(x, i, av);
                    }
                    else
                        pd_error(x, "[function]: needs an odd list of floats");
                    av += i;
                    ac -= i;
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    };
    function_set_min_max(x);
    if(!envset){
        function_generate(x, 3, a);
        function_set_min_max(x);
    }
    if(initialDuration > 0)
        function_duration(x, initialDuration);
/////////////////////////////////////////////////////////////////////////////////////
     return(x);
errstate:
    pd_error(x, "[function]: improper args");
    return NULL;
}

static void function_free(t_function *x){
    if(x->x_receive_sym != &s_)
         pd_unbind(&x->x_obj.ob_pd, x->x_receive_sym);
}

void function_setup(void){
    function_class = class_new(gensym("function"), (t_newmethod)function_new,
        (t_method)function_free, sizeof(t_function), 0, A_GIMME,0);
    class_addbang(function_class, function_bang);
    class_addfloat(function_class, function_float);
    class_addlist(function_class, function_list);
    class_addmethod(function_class, (t_method)function_loadbang,
                    gensym("loadbang"), A_DEFFLOAT, 0);
    class_addmethod(function_class, (t_method)function_resize, gensym("resize"), A_GIMME, 0);
    class_addmethod(function_class, (t_method)function_set, gensym("set"), A_GIMME, 0);
    class_addmethod(function_class, (t_method)function_i, gensym("i"), A_FLOAT, 0);
    class_addmethod(function_class, (t_method)function_init, gensym("init"), A_FLOAT, 0);
    class_addmethod(function_class, (t_method)function_height, gensym("height"), A_FLOAT, 0);
    class_addmethod(function_class, (t_method)function_width, gensym("width"), A_FLOAT, 0);
    class_addmethod(function_class, (t_method)function_min, gensym("min"), A_FLOAT, 0);
    class_addmethod(function_class, (t_method)function_max, gensym("max"), A_FLOAT, 0);
    class_addmethod(function_class, (t_method)function_duration, gensym("duration"), A_FLOAT, 0);
    class_addmethod(function_class, (t_method)function_set_send, gensym("send"), A_SYMBOL, 0);
    class_addmethod(function_class, (t_method)function_set_receive, gensym("receive"), A_SYMBOL, 0);
    class_addmethod(function_class, (t_method)function_bgcolor, gensym("bgcolor"),
                    A_FLOAT, A_FLOAT, A_FLOAT, 0);
    class_addmethod(function_class, (t_method)function_fgcolor, gensym("fgcolor"),
                    A_FLOAT, A_FLOAT, A_FLOAT, 0);
///
    class_addmethod(function_class, (t_method)function_print, gensym("print"), 0);
    class_addmethod(function_class, (t_method)function_motion, gensym("motion"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(function_class, (t_method)function_key, gensym("key"), A_FLOAT, 0);
// widget
    class_setwidget(function_class, &function_widgetbehavior);
    class_setsavefn(function_class, function_save);
    function_widgetbehavior.w_getrectfn  = function_getrect;
    function_widgetbehavior.w_displacefn = function_displace;
    function_widgetbehavior.w_selectfn   = function_select;
    function_widgetbehavior.w_activatefn = NULL;
    function_widgetbehavior.w_deletefn   = function_delete;
    function_widgetbehavior.w_visfn      = function_vis;
    function_widgetbehavior.w_clickfn    = (t_clickfn)function_newclick;
}
