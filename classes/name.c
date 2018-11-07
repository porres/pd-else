
#include "m_pd.h"
#include "g_canvas.h"

static t_class *name_class;

typedef struct _name{
    t_object    x_obj;
    t_symbol   *x_sym;
    t_pd       *x_owner;
}t_name;

static void name_name(t_name *x, t_symbol *s){
    if(*s->s_name){
        if(*x->x_sym->s_name)
            pd_unbind(x->x_owner, x->x_sym);
        pd_bind(x->x_owner, x->x_sym = s);
    }
}

static void *name_new(t_symbol *s, t_float f){
    t_name *x = (t_name *)pd_new(name_class);
    t_glist *glist = (t_glist *)canvas_getcurrent();
    t_canvas *canvas = (t_canvas*)glist_getcanvas(glist);
    int depth = (int)f;
    if(depth < 0)
        depth = 0;
    while(!canvas->gl_env)
        canvas = canvas->gl_owner;
    while(depth--){
        if(canvas->gl_owner){
            canvas = canvas->gl_owner;
            while(!canvas->gl_env)
                canvas = canvas->gl_owner;
        }
    }
    x->x_owner = (t_pd *)canvas;
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
        (t_method)name_free, sizeof(t_name), 0, A_DEFSYM, A_DEFFLOAT, 0);
    class_addmethod(name_class, (t_method)name_name, gensym("name"), A_DEFSYMBOL, 0);
}