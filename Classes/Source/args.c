
#include "m_pd.h"
#include "g_canvas.h"

static t_class *args_class;

typedef struct _args
{
    t_object  x_obj;
    t_canvas  *x_canvas;
    int        x_argc;
//    int        x_dirty;
    t_atom    *x_argv;
} t_args;

static void copy_atoms(t_atom *src, t_atom *dst, int n)
{
    while(n--)
        *dst++ = *src++;
}

static void args_list(t_args *x, t_symbol *s, int argc, t_atom *argv)
{
    t_symbol *dummy = s;
    dummy = NULL;
    t_binbuf* b = x->x_canvas->gl_obj.te_binbuf;
    if(!x || !x->x_canvas || !b)
        return;
    t_atom name[1];
    SETSYMBOL(name, atom_getsymbol(binbuf_getvec(b)));
    binbuf_clear(b);
    binbuf_add(b, 1, name);
    binbuf_add(b, argc, argv);
    x->x_argc = argc;
    x->x_argv = getbytes(argc * sizeof(*(x->x_argv)));
    copy_atoms(argv, x->x_argv, argc);
//    if(x->x_dirty)
//        canvas_dirty(x->x_canvas, 1);
}

static void args_bang(t_args *x)
{
    if(x->x_argv)
        outlet_list(x->x_obj.ob_outlet, &s_list, x->x_argc, x->x_argv);
}

static void args_free(t_args *x)
{
    x->x_canvas = 0;
}

static void *args_new(void)
{
    t_args *x = (t_args *)pd_new(args_class);
    t_glist *glist = (t_glist *)canvas_getcurrent();
    x->x_canvas = (t_canvas*)glist_getcanvas(glist);
    while(!x->x_canvas->gl_env)
        x->x_canvas = x->x_canvas->gl_owner;
    int argc;
//    x->x_dirty = 0;
    t_atom *argv;
    canvas_setcurrent(x->x_canvas);
    canvas_getargs(&argc, &argv);
    x->x_argc = argc;
    x->x_argv = getbytes(argc * sizeof(*(x->x_argv)));
    copy_atoms(argv, x->x_argv, argc);
    canvas_unsetcurrent(x->x_canvas);
    outlet_new(&x->x_obj, 0);
    return (x);
}

void args_setup(void)
{
    args_class = class_new(gensym("args"), (t_newmethod)args_new,
                (t_method)args_free, sizeof(t_args), 0, 0);
    class_addlist(args_class, (t_method)args_list);
    class_addbang(args_class, (t_method)args_bang);
}
