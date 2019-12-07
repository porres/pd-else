// porres 2019

#include "m_pd.h"
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
    t_outlet               *x_outlet_x;
    t_outlet               *x_outlet_y;
    t_canvas               *x_canvas;
    int                     x_pos;
}t_canvas_mouse;

static void canvas_mouse_proxy_any(t_canvas_mouse_proxy *p, t_symbol*s, int ac, t_atom *av){
    ac = 0;
    if(p->p_parent){
        t_canvas *cnv = p->p_parent->x_canvas;
        float x = av->a_w.w_float;
        float y = (av+1)->a_w.w_float;
        if(p->p_parent->x_pos){
            x -= (float)cnv->gl_obj.te_xpix;
            y -= (float)cnv->gl_obj.te_ypix;
        }
        if(s == gensym("motion")){
            outlet_float(p->p_parent->x_outlet_x, x);
            outlet_float(p->p_parent->x_outlet_y, y);
        }
        else if(s == gensym("mouse")){
            if(((av+2)->a_w.w_float) == 1){
                outlet_float(p->p_parent->x_outlet_x, x);
                outlet_float(p->p_parent->x_outlet_y, y);
                outlet_float(p->p_parent->x_obj.ob_outlet, 1);
            }
        }
        else if(s == gensym("mouseup")){
            if((av+2)->a_w.w_float)
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

static void *canvas_mouse_new(t_floatarg f1, t_floatarg f2){
    t_canvas_mouse *x = (t_canvas_mouse *)pd_new(canvas_mouse_class);
    t_glist *glist = (t_glist *)canvas_getcurrent();
    t_canvas *canvas = (t_canvas*)glist_getcanvas(glist);
    x->x_canvas = canvas;
    int depth = f1 < 0 ? 0 : (int)f1;
    x->x_pos = f2 != 0;
    while(depth--){
        if(canvas->gl_owner){
            x->x_canvas = canvas;
            canvas = canvas->gl_owner;
        }
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
        (t_method)canvas_mouse_free, sizeof(t_canvas_mouse), CLASS_NOINLET,
        A_DEFFLOAT, A_DEFFLOAT, 0);
    canvas_mouse_proxy_class = class_new(0, 0, 0, sizeof(t_canvas_mouse_proxy),
        CLASS_NOINLET | CLASS_PD, 0);
    class_addanything(canvas_mouse_proxy_class, canvas_mouse_proxy_any);
}
