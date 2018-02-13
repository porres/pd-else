#include <stdio.h>
#include "m_pd.h"
#include "g_canvas.h"
#include "gui.h"

typedef struct _window{
    t_object   x_ob;
    t_symbol  *x_cvname;
    int        x_on;
}t_window;

static t_class *window_class;

static void window_dofocus(t_window *x, t_symbol *s, t_floatarg f){
    if((int)f){
        int on = (s == x->x_cvname);
        if(on != x->x_on) // ???
            outlet_float(((t_object *)x)->ob_outlet, x->x_on = on);
    }
    else
        if(x->x_on && s == x->x_cvname) // ???
            outlet_float(((t_object *)x)->ob_outlet, x->x_on = 0);
}

static void window_free(t_window *x){
    hammergui_unbindfocus((t_pd *)x); // HAMMER
}

static void *window_new(t_floatarg f){
    t_window *x = (t_window *)pd_new(window_class);
    t_glist *glist=(t_glist *)canvas_getcurrent();
    t_canvas *canvas=(t_canvas*)glist_getcanvas(glist);
    int depth = (int)f;
    if(depth < 0)
        depth = 0;
    while(depth && canvas){
        canvas = canvas->gl_owner;
        depth--;
    }
    char buf[32];
//  sprintf(buf, ".x%lx.c", (unsigned long)canvas_getcurrent());
    sprintf(buf, ".x%lx.c", (unsigned long)canvas);
    x->x_cvname = gensym(buf);
    x->x_on = 0;
    outlet_new((t_object *)x, &s_float);
    hammergui_bindfocus((t_pd *)x); // HAMMER
    return(x);
}

void window_setup(void){
    window_class = class_new(gensym("window"), (t_newmethod)window_new,
        (t_method)window_free, sizeof(t_window), CLASS_NOINLET, A_DEFFLOAT, 0);
    class_addmethod(window_class, (t_method)window_dofocus, gensym("_focus"), A_SYMBOL, A_FLOAT, 0);
}
