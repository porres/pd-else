// Porres 2026

#include <m_pd.h>

#include "nlist.h"

typedef struct _nl_traverse{
    t_object    x_obj;
    t_symbol   *x_sym;
    t_outlet   *x_lvl_out;
}t_nl_traverse;

static t_class *nl_traverse_class;

static void nl_traverse_name(t_nl_traverse *x, t_symbol *name){
    x->x_sym = (name == &s_) ? NULL : name;
}

static void nl_traverse_click(t_nl_traverse *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
    t_nlist *nlist = nlist_get(x->x_sym, gensym("traverse"));
    if(nlist)
        nlist_open(nlist);
}

static void nl_traverse_level(t_nlist_node *node, int depth, int target,
    t_atom *path, t_outlet *outlet){
    int index = 0;
    while(node){
        SETFLOAT(&path[depth], index++);
        if(depth == target)
            outlet_list(outlet, &s_list, depth + 1, path);
        else if(node->child)
            nl_traverse_level(node->child, depth + 1, target, path, outlet);
        node = node->next;
    }
}

static void nl_traverse_bang(t_nl_traverse *x){
    t_nlist *nlist = nlist_get(x->x_sym, gensym("traverse"));
    if(nlist){
        int depth = nlist->x_depth;
        t_atom *path = getbytes((depth + 1) * sizeof(t_atom));
        for(int lvl = 0; lvl <= depth; lvl++){
            outlet_float(x->x_lvl_out, lvl);
            nl_traverse_level(nlist->x_root, 0, lvl, path, x->x_obj.ob_outlet);
        }
        freebytes(path, (depth + 1) * sizeof(t_atom));
    }
}

static void nl_traverse_float(t_nl_traverse *x, t_floatarg f){
    t_nlist *nlist = nlist_get(x->x_sym, gensym("traverse"));
    if(!nlist)
        return;
    int lvl = f < 0 ? 0 : (int)f;
    if(lvl > nlist->x_depth){
        post("[nlist.traverse] %d depth out of range", lvl);
        return;
    }
    t_atom *path = getbytes((nlist->x_depth + 1) * sizeof(t_atom));
    nl_traverse_level(nlist->x_root, 0, lvl, path, x->x_obj.ob_outlet);
    freebytes(path, (nlist->x_depth + 1) * sizeof(t_atom));
}

static void *nl_traverse_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nl_traverse *x = (t_nl_traverse *)pd_new(nl_traverse_class);
    x->x_sym = NULL;
    if(ac && av->a_type == A_SYMBOL)
        x->x_sym = atom_getsymbol(av);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("name"));
    outlet_new(&x->x_obj, &s_list);
    x->x_lvl_out = outlet_new((t_object *)x, &s_float);
    return(x);
}

void setup_nlist0x2etraverse(void){
    nl_traverse_class = class_new(gensym("nlist.traverse"),
        (t_newmethod)(void *)nl_traverse_new, 0, sizeof(t_nl_traverse), 0, A_GIMME, 0);
    class_addbang(nl_traverse_class, nl_traverse_bang);
    class_addfloat(nl_traverse_class, nl_traverse_float);
    class_addmethod(nl_traverse_class, (t_method)nl_traverse_name, gensym("name"), A_SYMBOL, 0);
    class_addmethod(nl_traverse_class, (t_method)nl_traverse_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
}
