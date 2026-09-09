// Porres 2026

#include <m_pd.h>
#include "nlist.h"

typedef struct _nlistdelete{
    t_object    x_obj;
    t_symbol   *x_sym;
}t_nlistdelete;

static t_class *nlistdelete_class;

static void nlistdelete_set(t_nlistdelete *x, t_symbol *name){
    x->x_sym = (name == &s_) ? NULL : name;
}

static void nlistdelete_click(t_nlistdelete *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
    t_nlist *nlist = nlist_get(x->x_sym, gensym("delete"));
    if(nlist)
        nlist_open(nlist);
}

static void nlistdelete_delete(t_nlistdelete *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nlist *nlist = nlist_get(x->x_sym, gensym("delete"));
    if(!nlist || !ac)
        return;
    t_nlist_node **link = &nlist->x_root;
    for(int i = 0; i < ac; i++){
        if(av[i].a_type != A_FLOAT){
            pd_error(x, "[nlist.delete] index must be a number");
            return;
        }
        int index = (int)atom_getfloat(av + i);
        if(index < 0){
            post("[nlist.delete] index out of range");
            return;
        }
        t_nlist_node **item = link;
        while(*item && index > 0){
            item = &(*item)->next;
            index--;
        }
        if(!*item){
            post("[nlist.delete] index out of range");
            return;
        }
        if(i < ac - 1){
            if((*item)->type != 1){
                post("[nlist.delete] index out of range");
                return;
            }
            link = &(*item)->child;
        }
        else{
            t_nlist_node *deleted = *item;
            *item = deleted->next;
            deleted->next = NULL;
            nlist_clear_nodes(deleted);
        }
    }
    nlist->x_len = nlist_get_len(nlist->x_root);
    nlist->x_depth = nlist_get_depth(nlist->x_root);
    nlist_dirty(nlist);
    if(nlist->x_is_opened)
        nlist_do_update(nlist);
}

static void *nlistdelete_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nlistdelete *x = (t_nlistdelete *)pd_new(nlistdelete_class);
    x->x_sym = NULL;
    if(ac && av->a_type == A_SYMBOL)
        x->x_sym = atom_getsymbol(av);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("name"));
    return(x);
}

void setup_nlist0x2edelete(void){
    nlistdelete_class = class_new(gensym("nlist.delete"), (t_newmethod)(void *)nlistdelete_new,
        0, sizeof(t_nlistdelete), 0, A_GIMME, 0);
    class_addlist(nlistdelete_class, nlistdelete_delete);
    class_addmethod(nlistdelete_class, (t_method)nlistdelete_set, gensym("name"), A_SYMBOL, 0);
    class_addmethod(nlistdelete_class, (t_method)nlistdelete_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
}
