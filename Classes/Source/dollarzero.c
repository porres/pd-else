// porres 2019

#include "m_pd.h"
#include "g_canvas.h"

static t_class *dollarzero_class;

typedef struct _dollarzero{
    t_object         x_obj;
    t_glist        *x_glist;
}t_dollarzero;

static void dollarzero_bang(t_dollarzero *x){
    t_symbol *out = canvas_realizedollar(x->x_glist, gensym("$0"));
    outlet_symbol(x->x_obj.ob_outlet, out);
}

static void dollarzero_symbol(t_dollarzero *x, t_symbol *s){
    t_symbol *out = canvas_realizedollar(x->x_glist, s);
    outlet_symbol(x->x_obj.ob_outlet, out);
}

static void *dollarzero_new(t_floatarg f){
    t_dollarzero *x = (t_dollarzero *)pd_new(dollarzero_class);
    t_canvas *canvas = canvas_getcurrent();
    int depth = f < 0 ? 0 : (int)f;
    while(!canvas->gl_env)
        canvas = canvas->gl_owner;
    while(depth--){
        if(canvas->gl_owner){
            canvas = canvas->gl_owner;
            while(!canvas->gl_env)
                canvas = canvas->gl_owner;
        }
    }
    x->x_glist = canvas;
    outlet_new(&x->x_obj, 0);
    return (x);
}

void dollarzero_setup(void){
    dollarzero_class = class_new(gensym("dollarzero"), (t_newmethod)dollarzero_new,
            0, sizeof(t_dollarzero), 0, A_DEFFLOAT, 0);
    class_addsymbol(dollarzero_class, dollarzero_symbol);
    class_addbang(dollarzero_class, dollarzero_bang);
}
