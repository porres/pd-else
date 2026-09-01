// Porres 2026

#include <m_pd.h>
#include "nlist.h"

typedef struct _nl_iter{
    t_object    x_obj;
    t_symbol   *x_sym;
}t_nl_iter;

static t_class *nl_iter_class;

static void nl_iter_name(t_nl_iter *x, t_symbol *name){
    x->x_sym = (name == &s_) ? NULL : name;
}

static void nl_iter_click(t_nl_iter *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
    t_nlist *nlist = nlist_get(x->x_sym, gensym("iter"));
    if(nlist)
        nlist_open(nlist);
}

static void nl_iter_iterate(t_nlist_node *node, int depth, t_atom *path, t_outlet *out){
    int index = 0;
    while(node){
        SETFLOAT(&path[depth], index++);
        if(node->child)
            nl_iter_iterate(node->child, depth + 1, path, out);
        else
            outlet_list(out, &s_list, depth + 1, path);
        node = node->next;
    }
}

static void nl_iter_bang(t_nl_iter *x){
    t_nlist *nlist = nlist_get(x->x_sym, gensym("iter"));
    if(nlist){
        int depth = nlist->x_depth;
        t_atom *path = getbytes((depth + 1) * sizeof(t_atom));
        nl_iter_iterate(nlist->x_root, 0, path, x->x_obj.ob_outlet);
        freebytes(path, (depth + 1) * sizeof(t_atom));
    }
}

static void *nl_iter_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nl_iter *x = (t_nl_iter *)pd_new(nl_iter_class);
    x->x_sym = NULL;
    if(ac && av->a_type == A_SYMBOL)
        x->x_sym = atom_getsymbol(av);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("name"));
    outlet_new(&x->x_obj, &s_list);
    return(x);
}

void setup_nlist0x2eiter(void){
    nl_iter_class = class_new(gensym("nlist.iter"),
        (t_newmethod)(void *)nl_iter_new, 0, sizeof(t_nl_iter), 0, A_GIMME, 0);
    class_addbang(nl_iter_class, nl_iter_bang);
    class_addmethod(nl_iter_class, (t_method)nl_iter_name, gensym("name"), A_SYMBOL, 0);
    class_addmethod(nl_iter_class, (t_method)nl_iter_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
}
