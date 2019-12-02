// porres 2019

#include "m_pd.h"
#include "g_canvas.h"

static t_class *savechange_class;

typedef struct _savechange{
    t_object   x_obj;
    t_canvas  *x_canvas;
}t_savechange;

static void savechange_float(t_savechange *x, t_floatarg f){
    canvas_dirty(x->x_canvas, (int)f != 0);
}

static void *savechange_new(t_floatarg f){
    t_savechange *x = (t_savechange *)pd_new(savechange_class);
    t_glist *glist = (t_glist *)canvas_getcurrent();
    x->x_canvas = (t_canvas*)glist_getcanvas(glist);
    int depth = f < 0 ? 0 : (int)f;
    while(!x->x_canvas->gl_env)
        x->x_canvas = x->x_canvas->gl_owner;
    while(depth--){
        if(x->x_canvas->gl_owner){
            x->x_canvas = x->x_canvas->gl_owner;
            while(!x->x_canvas->gl_env)
                x->x_canvas = x->x_canvas->gl_owner;
        }
    }
    return(x);
}

void savechange_setup(void){
    savechange_class = class_new(gensym("savechange"), (t_newmethod)savechange_new,
        0, sizeof(t_savechange), 0, A_DEFFLOAT, 0);
    class_addfloat(savechange_class, (t_method)savechange_float);
}
