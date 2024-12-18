#include <m_pd.h>
#include <g_canvas.h>

static t_class *canvas_setname_class;

typedef struct _canvas_setname{
    t_object    x_obj;
    t_symbol   *x_sym;
    t_pd       *x_owner;
}t_canvas_setname;

static void *canvas_setname_new(t_symbol *s, t_floatarg f1, t_floatarg f2){
    t_canvas_setname *x = (t_canvas_setname *)pd_new(canvas_setname_class);
    t_canvas *cv = canvas_getcurrent();
    x->x_sym = s;
    int depth = (int)f1 < 0 ? 0 : (int)f1;
    while(depth-- && cv->gl_owner){
        if(f2 != 0)
            cv = canvas_getrootfor(cv->gl_owner);
        else
            cv = cv->gl_owner;
    }
    x->x_owner = (t_pd *)cv;
    if(*s->s_name)
        pd_bind(x->x_owner, s);
    return(x);
}

static void canvas_setname_free(t_canvas_setname *x){
    if(*x->x_sym->s_name)
        pd_unbind(x->x_owner, x->x_sym);
}

void setup_canvas0x2esetname(void){
    canvas_setname_class = class_new(gensym("canvas.setname"),
        (t_newmethod)canvas_setname_new, (t_method)canvas_setname_free,
            sizeof(t_canvas_setname), CLASS_NOINLET, A_DEFSYM, A_DEFFLOAT, A_DEFFLOAT, 0);
}
