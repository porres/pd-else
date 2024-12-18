// porres

#include <m_pd.h>
#include <g_canvas.h>

static t_class *canvas_setname_class;

typedef struct _canvas_setname{
    t_object    x_obj;
    t_symbol   *x_bindsym;
    t_canvas   *x_cv;
    t_int       x_depth;
    t_int       x_mode;
    t_pd       *x_owner;
}t_setname;

static void setname_name(t_setname *x, t_symbol *s){
    if(x->x_bindsym != &s_)
        pd_unbind(x->x_owner, x->x_bindsym);
    x->x_bindsym = s;
    if(x->x_bindsym != &s_)
        pd_bind(x->x_owner, x->x_bindsym);
}

static void setname_setowner(t_setname *x){
    t_canvas *cv = x->x_cv;
    int depth = x->x_depth;
    if(x->x_mode)
        cv = canvas_getrootfor(cv->gl_owner);
    while(depth-- && cv->gl_owner){
        if(x->x_mode)
            cv = canvas_getrootfor(cv->gl_owner);
        else
            cv = cv->gl_owner;
    }
    x->x_owner = (t_pd *)cv;
}

static void setname_reset(t_setname *x){
    if(x->x_bindsym != &s_)
        pd_unbind(x->x_owner, x->x_bindsym);
    setname_setowner(x);
    if(x->x_bindsym != &s_)
        pd_bind(x->x_owner, x->x_bindsym);
}

static void setname_depth(t_setname *x, t_floatarg f){
    x->x_depth = (int)f < 0 ? 0 : (int)f;
    setname_reset(x);
}

static void setname_mode(t_setname *x, t_floatarg f){
    x->x_mode = (int)(f != 0);
    setname_reset(x);
}

static void canvas_setname_free(t_setname *x){
    if(x->x_bindsym != &s_)
        pd_unbind(x->x_owner, x->x_bindsym);
}

static void *canvas_setname_new(t_symbol *s, t_floatarg f1, t_floatarg f2){
    t_setname *x = (t_setname *)pd_new(canvas_setname_class);
    x->x_depth = (int)f1 < 0 ? 0 : (int)f1;
    x->x_mode = (int)(f2 != 0);
    x->x_cv = canvas_getcurrent();
    setname_setowner(x);
    x->x_bindsym = s;
    if(x->x_bindsym != &s_)
        pd_bind(x->x_owner, x->x_bindsym);
    return(x);
}

void setup_canvas0x2esetname(void){
    canvas_setname_class = class_new(gensym("canvas.setname"),
        (t_newmethod)canvas_setname_new, (t_method)canvas_setname_free,
            sizeof(t_setname), CLASS_DEFAULT, A_DEFSYM, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(canvas_setname_class, (t_method)setname_depth, gensym("depth"), A_FLOAT, 0);
    class_addmethod(canvas_setname_class, (t_method)setname_mode, gensym("mode"), A_FLOAT, 0);
    class_addmethod(canvas_setname_class, (t_method)setname_name, gensym("name"), A_SYMBOL, 0);
}
