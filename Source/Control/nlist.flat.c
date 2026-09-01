// Porres 2026

#include <m_pd.h>
#include "nlist.h"

typedef struct _nlistflat{
    t_object    x_obj;
    t_symbol   *x_sym;
}t_nlistflat;

static t_class *nlistflat_class;

static void nlistflat_name(t_nlistflat *x, t_symbol *name){
    x->x_sym = (name == &s_) ? NULL : name;
}

static void nlistflat_click(t_nlistflat *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
    t_nlist *nlist = nlist_get(x->x_sym, gensym("flat"));
    if(nlist)
        nlist_open(nlist);
}

static int nlistflat_count(t_nlist_node *node){
    int n = 0;
    while(node){
        if(node->child)
            n += nlistflat_count(node->child);
        else
            n++;
        node = node->next;
    }
    return(n);
}

static void nlistflat_walk(t_nlist_node *node, t_atom *out, int *pos){
    while(node){
        if(node->child)
            nlistflat_walk(node->child, out, pos);
        else
            out[(*pos)++] = node->atom;
        node = node->next;
    }
}

static void nlistflat_bang(t_nlistflat *x){
    t_nlist *nlist = nlist_get(x->x_sym, gensym("flat"));
    if(nlist){
        int n = nlistflat_count(nlist->x_root);
        t_atom *out = getbytes(n * sizeof(t_atom));
        int pos = 0;
        nlistflat_walk(nlist->x_root, out, &pos);
        outlet_list(x->x_obj.ob_outlet, &s_list, pos, out);
        freebytes(out, n * sizeof(t_atom));
    }
}

static void *nlistflat_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nlistflat *x = (t_nlistflat *)pd_new(nlistflat_class);
    x->x_sym = NULL;
    if(ac && av->a_type == A_SYMBOL)
        x->x_sym = atom_getsymbol(av);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("name"));
    outlet_new(&x->x_obj, &s_list);
    return(x);
}

void setup_nlist0x2eflat(void){
    nlistflat_class = class_new(gensym("nlist.flat"),
        (t_newmethod)(void *)nlistflat_new, 0, sizeof(t_nlistflat), 0, A_GIMME, 0);
    class_addbang(nlistflat_class, nlistflat_bang);
    class_addmethod(nlistflat_class, (t_method)nlistflat_name, gensym("name"), A_SYMBOL, 0);
    class_addmethod(nlistflat_class, (t_method)nlistflat_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
}
