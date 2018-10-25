
#include "m_pd.h"
#include "g_canvas.h"

static t_class *name_class;

typedef struct _name{
    t_object    x_obj;
    t_symbol   *x_sym;
    t_pd       *x_owner;
    t_canvas   *x_canvas;
}t_name;

static void *name_new(t_symbol *s){
    t_name *x = (t_name *)pd_new(name_class);
    t_glist *glist = (t_glist *)canvas_getcurrent();
    x->x_canvas = (t_canvas*)glist_getcanvas(glist);
    while(!x->x_canvas->gl_env)
        x->x_canvas = x->x_canvas->gl_owner;
    x->x_owner = (t_pd *)x->x_canvas;
    x->x_sym = s;
    if(*s->s_name)
        pd_bind(x->x_owner, s);
    return(x);
}

static void name_free(t_name *x){
    if(*x->x_sym->s_name)
        pd_unbind(x->x_owner, x->x_sym);
}

void name_setup(void){
    name_class = class_new(gensym("name"), (t_newmethod)name_new,
        (t_method)name_free, sizeof(t_name), CLASS_NOINLET, A_DEFSYM, 0);
}