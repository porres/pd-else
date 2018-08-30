// porres 2018

// memory realloc often crashes but pointer seems ok! 'STATES' set to 100 prevents it
#define STATES  100   // Number of points

#define BACKGROUNDCOLOR "white"
#define FRONTCOLOR "black"
#define BORDER_LINE_SIZE 1
#define BORDERWIDTH 3
#define IOHEIGHT 1

#include <m_pd.h>
#include <g_canvas.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

t_widgetbehavior envelope_widgetbehavior;

static t_class *envelope_class;

typedef struct _envelope{
    t_object    x_obj;
    t_glist*    glist;
    int         x_state;
    int         x_last_state;
    t_float*    x_points;
    t_float*    x_duration;
    t_float     x_total_duration;
    t_symbol*   x_receive_sym;
    t_symbol*   x_send_sym;
    t_float     x_min;
    t_float     x_max;
    t_int       x_states;   // bug ???
// widget parameters
    int         x_width;
    int         x_height;
    int         x_numdoodles;
    int         x_grabbed; // for moving points
    int         x_shift; // move 100th
    float       x_pointer_x;
    float       x_pointer_y;
    t_clock*    x_numclock;
}t_envelope;

// REMOVE!!

static void envelope_print(t_envelope* x){
    post("---===### envelope settings ###===---");
    post("x_state:          %d",   x->x_state);
    post("x_x_last_state:     %d",   x->x_last_state);
    post("total duration:   %.2f", x->x_total_duration);
    post("min:              %.2f", x->x_min);
    post("max:              %.2f", x->x_max);
    post("---===### envelope settings ###===---");
}

////// BANG  ///////////////////////////////////////////////////////////////////////////////////

static void envelope_bang(t_envelope *x){
    t_int n;
    n = x->x_last_state + 1;
    t_int ac = n * 2 - 1;
    t_atom a[ac];
    t_int i;
    t_int j = 0;
    SETFLOAT(a+j, x->x_points[0]);
    j++;
    x->x_state = 1;
    for(i = 0; i < n - 1; i++){
        SETFLOAT(a+j, x->x_duration[x->x_state] - x->x_duration[x->x_state-1]);
        j++;
        SETFLOAT(a+j, x->x_points[x->x_state]);
        j++;
        x->x_state++;
    }
    outlet_list(x->x_obj.ob_outlet, &s_list, ac, a);
    if(x->x_send_sym != &s_ && x->x_send_sym->s_thing)
        pd_list(x->x_send_sym->s_thing, &s_list, ac, a);
}

// GUI SHIT //////////////////////////////////////////////////////////////////////////////////////////////

static void envelope_resize(t_envelope* x,int ns){
    if(ns > x->x_states){
        int newargs = ns*sizeof(t_float);
        x->x_duration = resizebytes(x->x_duration, x->x_states*sizeof(t_float), newargs);
        x->x_points = resizebytes(x->x_points, x->x_states*sizeof(t_float), newargs);
        x->x_states = ns;
    }
}

static void envelope_duration(t_envelope* x, t_float dur){
    if(dur < 1){
        post("envelope: minimum duration is 1 ms");
        return;
    }
    x->x_total_duration = dur;
    float f = dur/x->x_duration[x->x_last_state];
    int i;
    for(i = 1; i <= x->x_last_state; i++)
        x->x_duration[i] *= f;
}

static void envelope_generate(t_envelope *x, int ac, t_atom* av){
    t_float* dur;
    t_float* val;
    t_float tdur = 0;
    if(!ac)
        return;
    x->x_duration[0] = 0;
    x->x_last_state = ac >> 1;
    envelope_resize(x, ac >> 1);
    dur = x->x_duration;
    val = x->x_points;
// get the first value
    if(ac){
        *val = atom_getfloat(av++);
        *dur = 0.0;
        x->x_max = *val;
        x->x_min = *val;
    }
    dur++;
    val++;
    ac--;
// get the following
    for(; ac > 0; ac--){ // change this to while
        tdur += atom_getfloat(av++);
        *dur++ = tdur;
        ac--;
        if(ac > 0){
            *val = atom_getfloat(av++);
            if (*val > x->x_max)
                x->x_max = *val;
            if (*val < x->x_min)
                x->x_min = *val;
            *val++;
        }
        else{
            *val = 0;
            if (*val > x->x_max)
                x->x_max = *val;
            if (*val < x->x_min)
                x->x_min = *val;
            *val++;
        }
    }
    if(x->x_max == x->x_min){
        if(x->x_max == 0)
            x->x_max = 1;
        else{
            if(x->x_max >0){
                x->x_min = 0;
                if (x->x_max < 1)
                    x->x_max = 1;
            }
            else
                x->x_max = 0;
        }
    }
}

// MORE GUI SHIT /////////////////////////////////////////////////////////////////////////////////////

static void draw_inlet_and_outlet(t_envelope *x, t_glist *glist, int firsttime){
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
// Outlet
    if(x->x_send_sym == &s_){
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

static int envelope_x_next_doodle(t_envelope *x, struct _glist *glist, int xpos,int ypos){
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
    xscale = x->x_width/x->x_duration[x->x_last_state];
    yscale = x->x_height;
    dxpos = text_xpix(&x->x_obj, glist); // + BORDERWIDTH; // why not ???
    dypos = text_ypix(&x->x_obj, glist) + BORDERWIDTH;
    for(i = 0; i <= x->x_last_state; i++){
        float dx2 = (dxpos + (x->x_duration[i] * xscale)) - xpos;
        float dy2 = (dypos + yscale - ((x->x_points[i] - yBase) / ySize * yscale)) - ypos;
        dx2 *= dx2;
        dy2 *= dy2;
        tval = sqrt(dx2+dy2);
        if (tval <= minval){
            minval = tval;
            insertpos = i;
        }
    }
// decide if we want to make a new one
    if(minval > 8 && insertpos >= 0){
        while (((dxpos + (x->x_duration[insertpos] * xscale)) - xpos) < 0)
            insertpos++;
        while (((dxpos + (x->x_duration[insertpos-1] * xscale)) - xpos) >0)
            insertpos--;
        if(x->x_last_state + 1 >= x->x_states)
            envelope_resize(x, x->x_states + 1);
        for(i = x->x_last_state; i >= insertpos; i--){
            x->x_duration[i + 1] = x->x_duration[i];
            x->x_points[i + 1] = x->x_points[i];
        }
        x->x_duration[insertpos] = (float)(xpos-dxpos)/x->x_width*x->x_duration[x->x_last_state++];
        x->x_pointer_x = xpos;
        x->x_pointer_y = ypos;
    }
    else{
        x->x_pointer_x = text_xpix(&x->x_obj,glist) + x->x_duration[insertpos]*x->x_width/x->x_duration[x->x_last_state];
        x->x_pointer_y = ypos;
    }
    x->x_grabbed = insertpos;
    return insertpos;
}

static void envelope_create_doodles(t_envelope *x, t_glist *glist){
    float xscale, yscale;
    int xpos, ypos;
    int i;
    char guistr[255];
    float ySize = x->x_max - x->x_min;
    float yBase =  x->x_min;
    float yvalue;
    xscale = x->x_width/x->x_duration[x->x_last_state];
    yscale = x->x_height;
    xpos = text_xpix(&x->x_obj, glist);
    ypos = (int) (text_ypix(&x->x_obj, glist) + x->x_height);
    for(i = 0; i <= x->x_last_state; i++){
        yvalue = (x->x_points[i] - yBase) / ySize * yscale;
        sprintf(guistr,".x%lx.c create oval %d %d %d %d -tags %lxD%d", glist_getcanvas(glist),
                (int)(xpos + (x->x_duration[i] * xscale) - 2),
                (int)(ypos - yvalue - 2),
                (int)(xpos + (x->x_duration[i] * xscale) + 2),
                (int)(ypos - yvalue + 2),
                x, i);
        strcat(guistr," -fill "FRONTCOLOR"\n");
        sys_vgui("%s", guistr);
    }
    x->x_numdoodles = i;
}

static void envelope_delete_doodles(t_envelope *x, t_glist *glist){
    int i;
    for(i = 0; i <= x->x_numdoodles; i++)
        sys_vgui(".x%lx.c delete %lxD%d\n",glist_getcanvas(glist), x, i);
}

static void envelope_update_doodles(t_envelope *x, t_glist *glist){
// LATER only create new doodles if necessary
    envelope_delete_doodles(x, glist);
    envelope_create_doodles(x, glist);
}

static void envelope_delnum(t_envelope *x){
    sys_vgui(".x%lx.c delete %lxT\n", glist_getcanvas(x->glist), x);
}

static void envelope_create(t_envelope *x, t_glist *glist){
    int i;
    float xscale, yscale;
    int xpos, ypos;
    float ySize = x->x_max - x->x_min;
    float yBase =  x->x_min;
    xpos = text_xpix(&x->x_obj, glist);
    ypos = (int) text_ypix(&x->x_obj, glist);
    x->x_numclock = clock_new(x, (t_method) envelope_delnum);
    sys_vgui(".x%lx.c create rectangle %d %d %d %d  -tags %lxS -fill %s -width %d\n",
             glist_getcanvas(glist), xpos - BORDERWIDTH, ypos - BORDERWIDTH,
             xpos + x->x_width + BORDERWIDTH,
             ypos + x->x_height + BORDERWIDTH, // + 2 here
             x, BACKGROUNDCOLOR, BORDER_LINE_SIZE);
    xscale = x->x_width / x->x_duration[x->x_last_state];
    yscale = x->x_height;
    sys_vgui(".x%lx.c create line", glist_getcanvas(glist));
    for(i = 0; i <= x->x_last_state; i++){
        sys_vgui(" %d %d ", (int)(xpos + x->x_duration[i]*xscale),
                 (int)(ypos + x->x_height - (x->x_points[i]-yBase) / ySize*yscale));
    }
    sys_vgui(" -tags %lxP -fill %s\n", x, FRONTCOLOR);
    envelope_create_doodles(x, glist);
}

static void envelope_update(t_envelope *x, t_glist *glist){
    int i;
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
    xscale = x->x_width / x->x_duration[x->x_last_state];
    yscale = x->x_height;
    sys_vgui(".x%lx.c coords %lxP", glist_getcanvas(glist), x);
    for(i = 0; i <= x->x_last_state; i++)
        sys_vgui(" %d %d ",(int)(xpos + x->x_duration[i]*xscale),
                 (int)(ypos + x->x_height - (x->x_points[i] - yBase) / ySize*yscale));
    sys_vgui("\n");
    envelope_update_doodles(x, glist);
    draw_inlet_and_outlet(x, glist, 0); // again ???
}

static void envelope_drawme(t_envelope *x, t_glist *glist, int firsttime){
    if(firsttime)
        envelope_create(x, glist);
    else
        envelope_update(x, glist);
    draw_inlet_and_outlet(x, glist, firsttime);
}

static void envelope_erase(t_envelope* x, t_glist* glist){
    sys_vgui(".x%lx.c delete %lxS\n", glist_getcanvas(glist), x);
    sys_vgui(".x%lx.c delete %lxP\n", glist_getcanvas(glist), x);
    if(x->x_receive_sym == &s_){
        sys_vgui(".x%lx.c delete %lxi0\n", glist_getcanvas(glist), x);
        sys_vgui(".x%lx.c delete %lxo0\n", glist_getcanvas(glist), x);
        sys_vgui(".x%lx.c delete %lxo1\n", glist_getcanvas(glist), x);
        sys_vgui(".x%lx.c delete %lxo2\n", glist_getcanvas(glist), x);
    }
    envelope_delete_doodles(x,glist);
}

/* ------------------------ widgetbehaviour----------------------------- */

static void envelope_getrect(t_gobj *z, t_glist *owner, int *xp1, int *yp1, int *xp2, int *yp2){
    int width, height;
    t_envelope* s = (t_envelope*)z;
    width = s->x_width + BORDERWIDTH;
    height = s->x_height + BORDERWIDTH; // + 2 here
    *xp1 = text_xpix(&s->x_obj,owner) - BORDERWIDTH;
    *yp1 = text_ypix(&s->x_obj,owner) - BORDERWIDTH;
    *xp2 = text_xpix(&s->x_obj,owner) + width ; // + 4
    *yp2 = text_ypix(&s->x_obj,owner) + height ; // + 4
}

static void envelope_displace(t_gobj *z, t_glist *glist, int dx, int dy){
    t_envelope *x = (t_envelope *)z;
    x->x_obj.te_xpix += dx; // x movement
    x->x_obj.te_ypix += dy; // y movement
// MOVE THE BLUE LINE
    sys_vgui(".x%lx.c coords %xSEL %d %d %d %d \n",
             glist_getcanvas(glist), x,
             x->x_obj.te_xpix - BORDERWIDTH,
             x->x_obj.te_ypix - BORDERWIDTH,
             x->x_obj.te_xpix + x->x_width + BORDERWIDTH,
             x->x_obj.te_ypix + x->x_height + BORDERWIDTH);
    envelope_drawme(x, glist, 0);
    canvas_fixlinesfor(glist, (t_text*) x);
}

static void envelope_select(t_gobj *z, t_glist *glist, int state){
    t_envelope *x = (t_envelope *)z;
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

static void envelope_delete(t_gobj *z, t_glist *glist){
    t_text *x = (t_text *)z;
    canvas_deletelinesfor(glist, x);
}

static void envelope_vis(t_gobj *z, t_glist *glist, int vis){
    t_envelope* s = (t_envelope*)z;
    if(vis)
        envelope_drawme(s, glist, 1);
    else
        envelope_erase(s, glist);
}

static void envelope_followpointer(t_envelope* x,t_glist* glist){
    float dur;
    float xscale = x->x_duration[x->x_last_state] / x->x_width;
    float ySize = x->x_max - x->x_min;
    float yBase =  x->x_min;
    if((x->x_grabbed > 0) && (x->x_grabbed < x->x_last_state)){
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
    if(1)
        envelope_bang(x);
}

static void envelope_motion(t_envelope *x, t_floatarg dx, t_floatarg dy){
    if(x->x_shift){
        x->x_pointer_x += dx / 1000.f;
        x->x_pointer_y += dy / 1000.f;
    }
    else{
        x->x_pointer_x += dx;
        x->x_pointer_y += dy;
    }
    envelope_followpointer(x,x->glist);
/* "if resizing" (MAKE THIS HAPPEN IN EDIT MODE AND WHEN CLICKING THE BOTTOM RIGHT CORNER) !!!!!!!!
    else{
        x->x_width += dx;
        x->x_height += dy;
    } */
    envelope_update(x, x->glist);
}

static void envelope_key(t_envelope *x, t_floatarg f){
    if(f == 8.0 && x->x_grabbed < x->x_last_state &&  x->x_grabbed >0){
        int i;
        for(i = x->x_grabbed; i <= x->x_last_state; i++){
            x->x_duration[i] = x->x_duration[i+1];
            x->x_points[i] = x->x_points[i+1];
        }
        x->x_last_state--;
        x->x_grabbed--;
        envelope_update(x, x->glist);
        if(1)
            envelope_bang(x);
    }
}

static int envelope_newclick(t_envelope *x, struct _glist *glist,
            int xpos, int ypos, int shift, int alt, int dbl, int doit){
    alt = 0; // remove warning
// check if user wants to resize
    float wxpos = text_xpix(&x->x_obj, glist);
    float wypos = text_ypix(&x->x_obj, glist);
    if(doit){
        if((xpos >= wxpos) && (xpos <= wxpos + x->x_width) \
           && (ypos >= wypos ) && (ypos <= wypos + x->x_height)){
            envelope_x_next_doodle(x, glist, xpos, ypos);
            glist_grab(x->glist, &x->x_obj.te_g, (t_glistmotionfn) envelope_motion,
                       (t_glistkeyfn) envelope_key, xpos, ypos);
            x->x_shift = shift;
            envelope_followpointer(x, glist);
            envelope_update(x, glist);
        }
    }
    return(1);
}

///////////////////// METHODS /////////////////////

static void envelope_list(t_envelope *x, t_symbol* s, int ac,t_atom *av){
    t_symbol *dummy = s;
    dummy = NULL;
    if(ac > 2 && ac % 2){
        envelope_generate(x, ac, av);
        if(glist_isvisible(x->glist))
            envelope_drawme(x, x->glist, 0);
        if(1){
            outlet_list(x->x_obj.ob_outlet, &s_list, ac, av);
            if(x->x_send_sym != &s_ && x->x_send_sym->s_thing)
                pd_list(x->x_send_sym->s_thing, &s_list, ac, av);
        }
    }
    else
        post("[envelope] needs an odd list of at least 3 floats");
}

static void envelope_min(t_envelope *x, t_floatarg f){
    x->x_min = f;
    envelope_update(x, x->glist);
}

static void envelope_max(t_envelope *x, t_floatarg f){
    x->x_max = f;
    envelope_update(x, x->glist);
}

static void envelope_height(t_envelope *x, t_floatarg f){
    x->x_height = f < 20 ? 20 : f;
    envelope_update(x, x->glist);
}

static void envelope_width(t_envelope *x, t_floatarg f){
    x->x_width = f < 40 ? 40 : f;
    envelope_update(x, x->glist);
}

static void envelope_send(t_envelope *x, t_symbol *s){
    if(s != &s_)
        x->x_send_sym = s;
}

static void envelope_receive(t_envelope *x, t_symbol *s){
    if(s != &s_){
        if(x->x_receive_sym != &s_)
            pd_unbind(&x->x_obj.ob_pd, x->x_receive_sym);
        pd_bind(&x->x_obj.ob_pd, x->x_receive_sym = s);
    }
}

///////////////////// NEW / FREE / SETUP /////////////////////

static void *envelope_new(t_symbol *s, int ac, t_atom* av){
    t_envelope *x = (t_envelope *)pd_new(envelope_class);
    t_symbol *cursym = s; // get rid of warning
    x->x_state = 0;
    x->x_states = STATES;
    x->x_points = getbytes(x->x_states*sizeof(t_float));
    x->x_duration = getbytes(x->x_states*sizeof(t_float));
// widget
    x->x_grabbed = 0;
    x->glist = (t_glist*) canvas_getcurrent();
// Default Args
    x->x_width = 200;
    x->x_height = 100;
    t_float initialDuration = 0;
    x->x_receive_sym = x->x_send_sym = &s_;
    t_atom a[3];
    SETFLOAT(a, 0);
    SETFLOAT(a+1, 1000);
    SETFLOAT(a+2, 0);
    envelope_generate(x, 3, a);
/////////////////////////////////////////////////////////////////////////////////////
    int symarg = 0;
    int n = 0;
    while(ac > 0){
        if((av+n)->a_type == A_FLOAT && !symarg){
            n++;
            ac--;
        }
        else if((av+n)->a_type == A_SYMBOL){
            if(!symarg){
                symarg = 1;
                if(n > 2)
                    envelope_generate(x, n, av);
                av += n;
                n = 0;
            }
            cursym = atom_getsymbolarg(0, ac, av);
            if(!strcmp(cursym->s_name, "-duration")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    t_float curfloat = atom_getfloatarg(1, ac, av);
                    initialDuration = curfloat < 0 ? 0 : curfloat; // min width is 40
                    ac -= 2;
                    av += 2;
                }
                else
                    goto errstate;
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
            else if(!strcmp(cursym->s_name, "-min")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    x->x_min = atom_getfloatarg(1, ac, av);
                    envelope_update(x, x->glist);
                    ac -= 2;
                    av += 2;
                }
                else
                    goto errstate;
            }
            else if(!strcmp(cursym->s_name, "-max")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    x->x_max = atom_getfloatarg(1, ac, av);
                    envelope_update(x, x->glist);
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
                    x->x_send_sym = atom_getsymbolarg(1, ac, av);
                    ac -= 2;
                    av += 2;
                }
                else
                    goto errstate;
            }
            else if(!strcmp(cursym->s_name, "-receive")){
                if(ac >= 2 && (av+1)->a_type == A_SYMBOL){
                    pd_bind(&x->x_obj.ob_pd, x->x_receive_sym = atom_getsymbolarg(1, ac, av));
                    ac -= 2;
                    av += 2;
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
    if(!symarg && n > 2)
        envelope_generate(x, n, av);
     if(initialDuration > 0)
        envelope_duration(x, initialDuration);
/////////////////////////////////////////////////////////////////////////////////////
     outlet_new(&x->x_obj, &s_anything);
     return (x);
errstate:
    pd_error(x, "[envelope]: improper args");
    return NULL;
}

static void envelope_free(t_envelope *x){
    if(x->x_receive_sym != &s_)
         pd_unbind(&x->x_obj.ob_pd, x->x_receive_sym); 
}

void envelope_setup(void){
    envelope_class = class_new(gensym("envelope"), (t_newmethod)envelope_new,
        (t_method)envelope_free, sizeof(t_envelope), 0, A_GIMME,0);
    class_addbang(envelope_class, envelope_bang);
    class_addlist(envelope_class, envelope_list);
    class_addmethod(envelope_class, (t_method)envelope_height, gensym("height"), A_FLOAT, A_NULL);
    class_addmethod(envelope_class, (t_method)envelope_width, gensym("width"), A_FLOAT, A_NULL);
    class_addmethod(envelope_class, (t_method)envelope_min, gensym("min"), A_FLOAT, A_NULL);
    class_addmethod(envelope_class, (t_method)envelope_max, gensym("max"), A_FLOAT, A_NULL);
    class_addmethod(envelope_class, (t_method)envelope_duration, gensym("duration"), A_FLOAT, A_NULL);
    class_addmethod(envelope_class, (t_method)envelope_send, gensym("send"), A_SYMBOL, A_NULL);
    class_addmethod(envelope_class, (t_method)envelope_receive, gensym("receive"), A_SYMBOL, A_NULL);
///
    class_addmethod(envelope_class, (t_method)envelope_print, gensym("print"), A_NULL);
    class_addmethod(envelope_class, (t_method)envelope_motion, gensym("motion"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(envelope_class, (t_method)envelope_key, gensym("key"), A_FLOAT, 0);
// widget
    envelope_widgetbehavior.w_getrectfn  = envelope_getrect;
    envelope_widgetbehavior.w_displacefn = envelope_displace;
    envelope_widgetbehavior.w_selectfn   = envelope_select;
    envelope_widgetbehavior.w_activatefn = NULL;
    envelope_widgetbehavior.w_deletefn   = envelope_delete;
    envelope_widgetbehavior.w_visfn      = envelope_vis;
    envelope_widgetbehavior.w_clickfn    = (t_clickfn)envelope_newclick;
    class_setwidget(envelope_class, &envelope_widgetbehavior);
}
