
#include <string.h>
#include "m_pd.h"
#include "mouse_gui.h"
#include "g_canvas.h"

typedef struct _mouse{
    t_object   x_obj;
    int        x_hzero;
    int        x_vzero;
    int        x_zero;
    int        x_wx;
    int        x_wy;
    t_glist   *x_glist;
    t_outlet  *x_horizontal;
    t_outlet  *x_vertical;
}t_mouse;

static t_class *mouse_class;

static void mouse_anything(void){ // dummy
}

static void mouse_updatepos(t_mouse *x){ // update position
    x->x_wx = x->x_glist->gl_screenx1;
    x->x_wy = x->x_glist->gl_screeny1;
}

static void mouse_doup(t_mouse *x, t_floatarg f){
    outlet_float(((t_object *)x)->ob_outlet, ((int)f ? 0 : 1));
}

static void mouse_dobang(t_mouse *x, t_floatarg h, t_floatarg v){
    outlet_float(x->x_vertical, (int)v - x->x_vzero);
    outlet_float(x->x_horizontal, (int)h - x->x_hzero);
}

static void mouse_dozero(t_mouse *x, t_floatarg f1, t_floatarg f2){
    if(x->x_zero){
        x->x_hzero = (int)f1;
        x->x_vzero = (int)f2;
        x->x_zero = 0;
    };
}

static void mouse__getscreen(t_mouse *x, t_float screenx, t_float screeny){
    // callback from tcl for requesting screen coords
    if(x->x_zero == 1)
        mouse_dozero(x, screenx, screeny);
    mouse_dobang(x, screenx, screeny);
}

static void mouse_zero(t_mouse *x){
    x->x_zero = 1;
    hammergui_getscreen();
}

static void mouse_reset(t_mouse *x){
    x->x_hzero = x->x_vzero = 0;
    hammergui_getscreen();
}

static void mouse_free(t_mouse *x){
    hammergui_stoppolling((t_pd *)x);
    hammergui_unbindmouse((t_pd *)x);
}

static void *mouse_new(void){
    t_mouse *x = (t_mouse *)pd_new(mouse_class);
    x->x_zero = 0;    outlet_new((t_object *)x, &s_float);
    x->x_horizontal = outlet_new((t_object *)x, &s_float);
    x->x_vertical = outlet_new((t_object *)x, &s_float);
    x->x_glist = (t_glist *)canvas_getcurrent();
    hammergui_bindmouse((t_pd *)x);
    hammergui_willpoll();
    mouse_reset(x);
    mouse_updatepos(x);
    hammergui_startpolling((t_pd *)x, 1);
    return(x);
}

void mouse_setup(void){
    mouse_class = class_new(gensym("mouse"), (t_newmethod)mouse_new,
            (t_method)mouse_free, sizeof(t_mouse), 0, 0);
    class_addanything(mouse_class, mouse_anything);
    class_addmethod(mouse_class, (t_method)mouse_doup, gensym("_up"), A_FLOAT, 0);
    class_addmethod(mouse_class, (t_method)mouse__getscreen, gensym("_getscreen"),
                    A_FLOAT, A_FLOAT, 0);
    class_addmethod(mouse_class, (t_method)mouse_dobang, gensym("_bang"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(mouse_class, (t_method)mouse_dozero, gensym("_zero"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(mouse_class, (t_method)mouse_zero, gensym("zero"), 0);
    class_addmethod(mouse_class, (t_method)mouse_reset, gensym("reset"), 0);
}
