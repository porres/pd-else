
#include "m_pd.h"
#include <math.h>
#include "g_canvas.h"

static t_class *canvas_mouse_class, *canvas_mouse_proxy_class;

typedef struct _canvas_mouse_proxy{
    t_object    p_obj;
    t_symbol   *p_sym;
    t_clock    *p_clock;
    struct      _canvas_mouse *p_parent;
}t_canvas_mouse_proxy;

typedef struct _canvas_mouse{
    t_object                x_obj;
    t_canvas_mouse_proxy   *x_proxy;
    t_outlet*               x_outlet_x;
    t_outlet*               x_outlet_y;
}t_canvas_mouse;

static void canvas_mouse_proxy_any(t_canvas_mouse_proxy *p, t_symbol*s, int ac, t_atom *av){
    ac = 0;
    if(p->p_parent){
        if(s == gensym("motion")){
            outlet_float(p->p_parent->x_outlet_x, av->a_w.w_float);
            outlet_float(p->p_parent->x_outlet_y, (av+1)->a_w.w_float);
        }
        else if(s == gensym("mouse")){
            int click = (int)((av+2)->a_w.w_float);
            if(click == 1){
                outlet_float(p->p_parent->x_outlet_x, av->a_w.w_float);
                outlet_float(p->p_parent->x_outlet_y, (av+1)->a_w.w_float);
                outlet_float(p->p_parent->x_obj.ob_outlet, click);
            }
        }
        else if(s == gensym("mouseup")){
            int release = (int)((av+2)->a_w.w_float);
            if(release == 1)
                outlet_float(p->p_parent->x_obj.ob_outlet, 0);
        };
    }
}

static void canvas_mouse_proxy_free(t_canvas_mouse_proxy *p){
    pd_unbind(&p->p_obj.ob_pd, p->p_sym);
    clock_free(p->p_clock);
    pd_free(&p->p_obj.ob_pd);
}

static t_canvas_mouse_proxy * canvas_mouse_proxy_new(t_canvas_mouse *x, t_symbol*s){
    t_canvas_mouse_proxy *p = (t_canvas_mouse_proxy*)pd_new(canvas_mouse_proxy_class);
    p->p_parent = x;
    pd_bind(&p->p_obj.ob_pd, p->p_sym = s);
    p->p_clock = clock_new(p, (t_method)canvas_mouse_proxy_free);
    return(p);
}

static void canvas_mouse_free(t_canvas_mouse *x){
    x->x_proxy->p_parent = NULL;
    clock_delay(x->x_proxy->p_clock, 0);
}

static void *canvas_mouse_new(t_symbol *s, int argc, t_atom *argv){
    s = NULL;
    t_canvas_mouse *x = (t_canvas_mouse *)pd_new(canvas_mouse_class);
    t_glist *glist = (t_glist *)canvas_getcurrent();
    t_canvas *canvas = (t_canvas*)glist_getcanvas(glist);
    int depth = 0;
    if(argc && argv->a_type == A_FLOAT){
        float f = argv->a_w.w_float;
        depth = f < 0 ? 0 : (int)f;
    }
    while(depth--){
        if(canvas->gl_owner)
            canvas = canvas->gl_owner;
    }
    char buf[MAXPDSTRING];
    snprintf(buf, MAXPDSTRING-1, ".x%lx", (unsigned long)canvas);
    buf[MAXPDSTRING-1] = 0;
    x->x_proxy = canvas_mouse_proxy_new(x, gensym(buf));
    outlet_new(&x->x_obj, 0);
    x->x_outlet_x =  outlet_new(&x->x_obj, &s_);
    x->x_outlet_y =  outlet_new(&x->x_obj, &s_);
    return(x);
}

void setup_canvas0x2emouse(void){
    canvas_mouse_class = class_new(gensym("canvas.mouse"), (t_newmethod)canvas_mouse_new,
        (t_method)canvas_mouse_free, sizeof(t_canvas_mouse), CLASS_NOINLET, A_GIMME, 0);
    canvas_mouse_proxy_class = class_new(0, 0, 0, sizeof(t_canvas_mouse_proxy),
        CLASS_NOINLET | CLASS_PD, 0);
    class_addanything(canvas_mouse_proxy_class, canvas_mouse_proxy_any);
}
