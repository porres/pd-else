// porres 2019

#include "m_pd.h"
#include "g_canvas.h"

static t_class *dollarzero_class;

typedef struct _dollarzero{
    t_object         x_obj;
    t_glist        *x_canvas;
}t_dollarzero;

static void dollarzero_bang(t_dollarzero *x){
    t_symbol *out = canvas_realizedollar(x->x_canvas, gensym("$0"));
    outlet_symbol(x->x_obj.ob_outlet, out);
}

static void dollarzero_symbol(t_dollarzero *x, t_symbol *s){
    t_symbol *out = canvas_realizedollar(x->x_canvas, s);
    outlet_symbol(x->x_obj.ob_outlet, out);
}

static void *dollarzero_new(t_floatarg f){
    t_dollarzero *x = (t_dollarzero *)pd_new(dollarzero_class);
    int depth = f < 0 ? 0 : (int)f;
    x->x_canvas = canvas_getrootfor(canvas_getcurrent());
    while(depth-- && x->x_canvas->gl_owner)
        x->x_canvas = canvas_getrootfor(x->x_canvas->gl_owner);
    outlet_new(&x->x_obj, 0);
    return (x);
}

void dollarzero_setup(void){
    dollarzero_class = class_new(gensym("dollarzero"), (t_newmethod)dollarzero_new,
            0, sizeof(t_dollarzero), 0, A_DEFFLOAT, 0);
    class_addsymbol(dollarzero_class, dollarzero_symbol);
    class_addbang(dollarzero_class, dollarzero_bang);
}
