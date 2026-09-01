// Porres 2026

#include <m_pd.h>
#include <string.h>
#include <stdlib.h>
#include "nlist.h"

typedef struct _nlistappend{
    t_object    x_obj;
    t_symbol   *x_sym;
}t_nlistappend;

static t_class *nlistappend_class;

static void nlistappend_name(t_nlistappend *x, t_symbol *name){
    x->x_sym = (name == &s_) ? NULL : name;
}

static void nlistappend_click(t_nlistappend *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
    t_nlist *nlist = nlist_get(x->x_sym, gensym("append"));
    if(nlist)
        nlist_open(nlist);
}

static void nlistappend_list(t_nlistappend *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nlist *nlist = nlist_get(x->x_sym, gensym("append"));
    if(!nlist || !ac)
        return;
    t_nlist temp;
    temp.x_root = NULL;
    temp.x_len = 0;
    temp.x_depth = 0;
    t_nlist_node *contents = nlist_parse_all(&temp, ac, av);
    if(!contents)
        return;
    int len = nlist_get_len(nlist->x_root);
    int path[1] = {len};
    nlist_insert_at(nlist, 1, path, contents);
    nlist_dirty(nlist);
    if(nlist->x_is_opened)
        nlist_do_update(nlist);
}

static void *nlistappend_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nlistappend *x = (t_nlistappend *)pd_new(nlistappend_class);
    x->x_sym = NULL;
    if(ac && av->a_type == A_SYMBOL)
        x->x_sym = atom_getsymbol(av);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("name"));
    return(x);
}

void setup_nlist0x2eappend(void){
    nlistappend_class = class_new(gensym("nlist.append"), (t_newmethod)(void *)nlistappend_new,
        0, sizeof(t_nlistappend), 0, A_GIMME, 0);
    class_addlist(nlistappend_class, nlistappend_list);
    class_addmethod(nlistappend_class, (t_method)nlistappend_name, gensym("name"), A_SYMBOL, 0);
    class_addmethod(nlistappend_class, (t_method)nlistappend_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
}
