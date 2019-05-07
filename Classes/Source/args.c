
#include "m_pd.h"
#include "string.h"
#include "g_canvas.h"

static t_class *args_class;

typedef struct _args
{
    t_object  x_obj;
    t_canvas  *x_canvas;
    int        x_argc;
//    int        x_dirty;
    t_atom    *x_argv;
    int             x_break;
    char            x_separator;
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
    if(x->x_argc)
        freebytes(x->x_argv, x->x_argc * sizeof(x->x_argv));
    x->x_argc = argc;
    x->x_argv = getbytes(argc * sizeof(*(x->x_argv)));
    copy_atoms(argv, x->x_argv, argc);
//    if(x->x_dirty)
//        canvas_dirty(x->x_canvas, 1);
}

static void args_break(t_args *x, t_symbol *s, int ac, t_atom *av){
    if(!ac)
        outlet_anything(x->x_obj.ob_outlet, s, ac, av);
    else{
        int i = -1, first = 1, n;
        while(i < ac){
            // i is starting point & j is broken item
            int j = i + 1;
            // j starts as next item from previous iteration (and as 0 in the first iteration)
            for (j; j < ac; j++){
                if ((av+j)->a_type == A_SYMBOL && x->x_separator == (atom_getsymbol(av+j))->s_name[0]){
                    break;
                }
            }
            // n is number of extra elements in the broken message (that's why we have - 1)
            n = j - i - 1;
            if(first){
                if(n == 0) // it's a selector
                    if(!strcmp(s->s_name, "list")){ // if selector is list, do nothing
                    }
                    else
                        outlet_anything(x->x_obj.ob_outlet, s, n, av - 1); // output selector
                    else
                        outlet_anything(x->x_obj.ob_outlet, s, n, av);
                first = 0;
            }
            else
                outlet_anything(x->x_obj.ob_outlet, atom_getsymbol(av + i), n, av + i + 1);
            i = j;
        }
    }
}

static void args_bang(t_args *x)
{
    if(x->x_argv){
        if (x->x_break)
            args_break(x, &s_list, x->x_argc, x->x_argv);
        else
            outlet_list(x->x_obj.ob_outlet, &s_list, x->x_argc, x->x_argv);
    }
}

static void args_free(t_args *x)
{
    if(x->x_argc)
        freebytes(x->x_argv, x->x_argc * sizeof(x->x_argv));
}

static void *args_new(t_symbol *s, int ac, t_atom* av)
{
    t_symbol *dummy = s;
    dummy = NULL;
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
    if(ac && av->a_type == A_SYMBOL){
        x->x_separator = atom_getsymbol(av)->s_name[0];
        x->x_break = 1;
    }
    outlet_new(&x->x_obj, 0);
    return (x);
}

void args_setup(void)
{
    args_class = class_new(gensym("args"), (t_newmethod)args_new,
                (t_method)args_free, sizeof(t_args), 0, A_GIMME, 0);
    class_addlist(args_class, (t_method)args_list);
    class_addbang(args_class, (t_method)args_bang);
}
