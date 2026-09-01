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

// Collecting the paths first (below) and only firing outlet_list() once the
// whole walk is finished means nothing downstream -- e.g. a [nlist.set] that
// writes back into the very index just reported -- can free/replace a node
// this traversal is still standing on. Emitting during the walk (as before)
// held live pointers into the tree across a synchronous call into code that
// could mutate that same tree, which corrupted the walk.
typedef struct _nl_iter_collect{
    t_atom *paths;    // flat storage: count * (maxdepth + 1) atoms
    int    *lens;     // actual path length (<= maxdepth + 1) per leaf
    int     maxdepth;
    int     count;
}t_nl_iter_collect;

static void nl_iter_collect(t_nlist_node *node, int depth, t_atom *path,
t_nl_iter_collect *c){
    int index = 0;
    while(node){
        SETFLOAT(&path[depth], index++);
        if(node->child)
            nl_iter_collect(node->child, depth + 1, path, c);
        else{
            t_atom *dst = c->paths + (size_t)c->count * (c->maxdepth + 1);
            for(int i = 0; i <= depth; i++)
                dst[i] = path[i];
            c->lens[c->count] = depth + 1;
            c->count++;
        }
        node = node->next;
    }
}

static void nl_iter_bang(t_nl_iter *x){
    t_nlist *nlist = nlist_get(x->x_sym, gensym("iter"));
    if(!nlist)
        return;
    int n = nlist_count_leaves(nlist->x_root);
    if(!n)
        return;
    int maxdepth = nlist->x_depth;
    t_atom *scratch = getbytes((maxdepth + 1) * sizeof(t_atom));
    t_nl_iter_collect c;
    c.maxdepth = maxdepth;
    c.count = 0;
    c.paths = getbytes((size_t)n * (maxdepth + 1) * sizeof(t_atom));
    c.lens = getbytes(sizeof(int) * n);
    nl_iter_collect(nlist->x_root, 0, scratch, &c);
    freebytes(scratch, (maxdepth + 1) * sizeof(t_atom));
    // Traversal is fully done and nothing below touches nlist->x_root
    // anymore -- safe to fan out even if a receiver mutates this nlist.
    for(int i = 0; i < c.count; i++){
        t_atom *p = c.paths + (size_t)i * (maxdepth + 1);
        outlet_list(x->x_obj.ob_outlet, &s_list, c.lens[i], p);
    }
    freebytes(c.paths, (size_t)n * (maxdepth + 1) * sizeof(t_atom));
    freebytes(c.lens, sizeof(int) * n);
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
