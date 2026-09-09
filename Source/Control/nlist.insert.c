// Porres 2026

#include <m_pd.h>
#include <string.h>
#include <stdlib.h>
#include "nlist.h"

typedef struct _nlistinsert{
    t_object    x_obj;
    t_symbol   *x_sym;
    int         x_path_ac;
    int         *x_path;
}t_nlistinsert;

static t_class *nlistinsert_class;

static void nlistinsert_name(t_nlistinsert *x, t_symbol *name){
    x->x_sym = (name == &s_) ? NULL : name;
}

static void nlistinsert_click(t_nlistinsert *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
    t_nlist *nlist = nlist_get(x->x_sym, gensym("insert"));
    if(nlist)
        nlist_open(nlist);
}

static void nlistinsert_set(t_nlistinsert *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(!ac)
        return;
    if(x->x_path){
        freebytes(x->x_path, sizeof(int) * x->x_path_ac);
        x->x_path = NULL;
        x->x_path_ac = 0;
    }
    x->x_path = (int *)getbytes(sizeof(int) * ac);
    for(int i = 0; i < ac; i++){
        if(av[i].a_type != A_FLOAT){
            pd_error(x, "[nlist.insert] index must be a number");
            freebytes(x->x_path, sizeof(int) * ac);
            x->x_path = NULL;
            return;
        }
        int index = (int)atom_getfloat(av + i);
        if(index < 0){
            pd_error(x, "[nlist.insert] index must be non-negative");
            freebytes(x->x_path, sizeof(int) * ac);
            x->x_path = NULL;
            return;
        }
        x->x_path[i] = index;
    }
    x->x_path_ac = ac;
}

static t_nlist_node **nlistinsert_find_link(t_nlist_node **node, int index){
    while(*node && index > 0){
        node = &(*node)->next;
        index--;
    }
    return(node);
}

static void nlistinsert_list(t_nlistinsert *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nlist *nlist = nlist_get(x->x_sym, gensym("insert"));
    if(!nlist || !ac)
        return;
    t_nlist_node **link = &nlist->x_root;
    for(int i = 0; i < x->x_path_ac; i++){
        if(i == x->x_path_ac - 1)
            break;
        link = nlistinsert_find_link(link, x->x_path[i]);
        if(!*link){
            pd_error(x, "[nlist.insert] index out of range");
            return;
        }
        if((*link)->type != 1){
            pd_error(x, "[nlist.insert] path exceeds list depth");
            return;
        }
        link = &(*link)->child;
    }
    if(x->x_path_ac)
        link = nlistinsert_find_link(link, x->x_path[x->x_path_ac - 1]);
    t_nlist temp;
    temp.x_root = NULL;
    temp.x_len = 0;
    temp.x_depth = 0;
    t_nlist_node *contents = nlist_parse_all(&temp, ac, av);
    if(!contents)
        return;
    t_nlist_node *last = contents;
    while(last->next)
        last = last->next;
    last->next = *link;
    *link = contents;
    nlist->x_len = nlist_get_len(nlist->x_root);
    nlist->x_depth = nlist_get_depth(nlist->x_root);
    nlist_dirty(nlist);
    if(nlist->x_is_opened)
        nlist_do_update(nlist);
}

static void *nlistinsert_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nlistinsert *x = (t_nlistinsert *)pd_new(nlistinsert_class);
    x->x_sym = NULL;
    x->x_path_ac = 0;
    x->x_path = NULL;
    if(ac && av->a_type == A_SYMBOL){
        x->x_sym = atom_getsymbol(av);
        ac--, av++;
    }
    if(ac && av->a_type == A_FLOAT)
        nlistinsert_set(x, &s_, ac, av);
    else{
        t_atom at[1];
        SETFLOAT(at, 0);
        nlistinsert_set(x, &s_, 1, at);
    }
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("name"));
    return(x);
}

static void nlistinsert_free(t_nlistinsert *x){
    if(x->x_path)
        freebytes(x->x_path, sizeof(int) * x->x_path_ac);
}

void setup_nlist0x2einsert(void){
    nlistinsert_class = class_new(gensym("nlist.insert"), (t_newmethod)(void *)nlistinsert_new,
        (t_method)nlistinsert_free, sizeof(t_nlistinsert), 0, A_GIMME, 0);
    class_addlist(nlistinsert_class, nlistinsert_list);
    class_addmethod(nlistinsert_class, (t_method)nlistinsert_name, gensym("name"), A_SYMBOL, 0);
    class_addmethod(nlistinsert_class, (t_method)nlistinsert_set, gensym("set"), A_GIMME, 0);
    class_addmethod(nlistinsert_class, (t_method)nlistinsert_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
}
