#include "m_pd.h"
#include "g_canvas.h"

static t_class *args_class;

typedef struct _args{
  t_object  x_obj;
  t_canvas  *x_canvas;
}t_args;

static void args_list(t_args *x, t_symbol*s, int ac, t_atom*av){
    t_binbuf * b = x->x_canvas->gl_obj.te_binbuf;
    if(!x || !x->x_canvas || !b)
        return;
    t_atom name[1];
    s = atom_getsymbol(binbuf_getvec(b));
    SETSYMBOL(name, s);
    binbuf_clear(b);
    binbuf_add(b, 1, name);
    binbuf_add(b, ac, av);
}

static void args_anything(t_args *x, t_symbol *s, int ac, t_atom *av){
    t_binbuf *b = x->x_canvas->gl_obj.te_binbuf;
    if(!x || !b || !x->x_canvas)
        return;
    t_atom name[1];
    t_atom selector[1];
    SETSYMBOL(name, atom_getsymbol(binbuf_getvec(b)));
    binbuf_clear(b);
    binbuf_add(b, 1, name);
    SETSYMBOL(selector, s);
    binbuf_add(b, 1, selector);
    binbuf_add(b, ac, av);
}

static void args_bang(t_args *x){
    if(!x->x_canvas)
        return;
    int ac = 0;
    t_atom *av = 0;
    t_binbuf *b = x->x_canvas->gl_obj.te_binbuf;
    if(b){
        ac = binbuf_getnatom(b)-1;
        av = binbuf_getvec(b)+1;
    }
    else{ // init bang?
        canvas_setcurrent(x->x_canvas);
        canvas_getargs(&ac, &av);
        canvas_unsetcurrent(x->x_canvas);
    }
    if(av)
        outlet_list(x->x_obj.ob_outlet, &s_list, ac, av);
}

static void args_free(t_args *x){
  x->x_canvas = 0;
}

static void *args_new(void){
    t_args *x = (t_args *)pd_new(args_class);
    t_glist *glist = (t_glist *)canvas_getcurrent();
    x->x_canvas = (t_canvas*)glist_getcanvas(glist);
    while(!x->x_canvas->gl_env)
        x->x_canvas = x->x_canvas->gl_owner;
    outlet_new(&x->x_obj, &s_list);
    return (x);
}

void args_setup(void){
    args_class = class_new(gensym("args"), (t_newmethod)args_new,
            (t_method)args_free, sizeof(t_args), 0, 0);
    class_addlist(args_class, (t_method)args_list);
    class_addanything(args_class, (t_method)args_anything);
    class_addbang(args_class, (t_method)args_bang);
}
