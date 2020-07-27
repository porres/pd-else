// porres 2020

#include "m_pd.h"
#include "g_canvas.h"

static t_class *realizedollar_class;

typedef struct _realizedollar{
    t_object    x_obj;
    t_glist    *x_cv;
}t_realizedollar;

static void realizedollar_symbol(t_realizedollar *x, t_symbol *s){
    outlet_symbol(x->x_obj.ob_outlet, canvas_realizedollar(x->x_cv, s));
}

static void *realizedollar_new(t_floatarg f){
    t_realizedollar *x = (t_realizedollar *)pd_new(realizedollar_class);
    int depth = f < 0 ? 0 : (int)f;
    x->x_cv = canvas_getrootfor(canvas_getcurrent());
    while(depth-- && x->x_cv->gl_owner)
        x->x_cv = canvas_getrootfor(x->x_cv->gl_owner);
    outlet_new(&x->x_obj, &s_);
    return(x);
}

void realizedollar_setup(void){
    realizedollar_class = class_new(gensym("realizedollar"), (t_newmethod)realizedollar_new,
        0, sizeof(t_realizedollar), 0, A_DEFFLOAT, 0);
    class_addsymbol(realizedollar_class, realizedollar_symbol);
}
