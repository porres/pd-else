// Porres 2026

#include <m_pd.h>
#include <string.h>
#include <stdlib.h>
#include "nlist.h"

typedef struct _nlistprepend{
    t_object    x_obj;
    t_symbol   *x_sym;
}t_nlistprepend;

static t_class *nlistprepend_class;

static void nlistprepend_name(t_nlistprepend *x, t_symbol *name){
    x->x_sym = (name == &s_) ? NULL : name;
}

static void nlistprepend_click(t_nlistprepend *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
    t_nlist *nlist = nlist_get(x->x_sym, gensym("prepend"));
    if(nlist)
        nlist_open(nlist);
}

static void nlistprepend_list(t_nlistprepend *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nlist *nlist = nlist_get(x->x_sym, gensym("prepend"));
    if(!nlist || !ac)
        return;
    t_nlist temp;
    temp.x_root = NULL;
    temp.x_len = 0;
    temp.x_depth = 0;
    t_nlist_node *contents = nlist_parse_all(&temp, ac, av);
    if(!contents)
        return;
    int path[1] = {0};
    nlist_insert_at(nlist, 1, path, contents);
    nlist_dirty(nlist);
    if(nlist->x_is_opened)
        nlist_do_update(nlist);
}

static void *nlistprepend_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nlistprepend *x = (t_nlistprepend *)pd_new(nlistprepend_class);
    x->x_sym = NULL;
    if(ac && av->a_type == A_SYMBOL)
        x->x_sym = atom_getsymbol(av);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("name"));
    return(x);
}

void setup_nlist0x2eprepend(void){
    nlistprepend_class = class_new(gensym("nlist.prepend"), (t_newmethod)(void *)nlistprepend_new,
        0, sizeof(t_nlistprepend), 0, A_GIMME, 0);
    class_addlist(nlistprepend_class, nlistprepend_list);
    class_addmethod(nlistprepend_class, (t_method)nlistprepend_name, gensym("name"), A_SYMBOL, 0);
    class_addmethod(nlistprepend_class, (t_method)nlistprepend_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
}
