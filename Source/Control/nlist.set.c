// Porres 2026

#include <m_pd.h>
#include <string.h>
#include <stdlib.h>
#include "nlist.h"

typedef struct _nlistset{
    t_object    x_obj;
    t_symbol   *x_sym;
    int         x_path_ac;
    int         *x_path;
}t_nlistset;

static t_class *nlistset_class;

static void nlistset_name(t_nlistset *x, t_symbol *name){
    x->x_sym = (name == &s_) ? NULL : name;
}

static void nlistset_click(t_nlistset *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
    t_nlist *nlist = nlist_get(x->x_sym, gensym("set"));
    if(nlist)
        nlist_open(nlist);
}

static void nlistset_set(t_nlistset *x, t_symbol *s, int ac, t_atom *av){
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
            pd_error(x, "[nlist.set] index must be a number");
            freebytes(x->x_path, sizeof(int) * ac);
            x->x_path = NULL;
            return;
        }
        int index = (int)atom_getfloat(av + i);
        if(index < 0){
            pd_error(x, "[nlist.set] index must be non-negative");
            freebytes(x->x_path, sizeof(int) * ac);
            x->x_path = NULL;
            return;
        }
        x->x_path[i] = index;
    }
    x->x_path_ac = ac;
}

static t_nlist_node *nlistset_new_list(void){
    t_nlist_node *node = (t_nlist_node *)getbytes(sizeof(*node));
    node->type = 1;
    node->child = NULL;
    node->next = NULL;
    return(node);
}

static void nlistset_free_nodes(t_nlist_node *node){
    while(node){
        t_nlist_node *next = node->next;
        if(node->type == 1)
            nlistset_free_nodes(node->child);
        freebytes(node, sizeof(*node));
        node = next;
    }
}

static t_nlist_node **nlistset_find_link(t_nlist_node **node, int index){
    while(*node && index > 0){
        node = &(*node)->next;
        index--;
    }
    return(node);
}

static int nlistset_len(t_nlist_node *node){
    int n = 0;
    while(node){
        n++;
        node = node->next;
    }
    return(n);
}

static int nlistset_depth(t_nlist_node *node, int depth){
    int maxdepth = depth;
    while(node){
        if(node->type == 1 && node->child){
            int d = nlistset_depth(node->child, depth + 1);
            if(d > maxdepth)
                maxdepth = d;
        }
        node = node->next;
    }
    return(maxdepth);
}

static void nlistset_list(t_nlistset *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nlist *nlist = nlist_get(x->x_sym, gensym("set"));
    if(!nlist)
        return;
    t_nlist_node **link = &nlist->x_root;
    for(int i = 0; i < x->x_path_ac; i++){
        link = nlistset_find_link(link, x->x_path[i]);
        if(!*link){
            pd_error(x, "[nlist.set] index out of range");
            return;
        }
        if(i < x->x_path_ac - 1){
            if((*link)->type != 1){
                pd_error(x, "[nlist.set] path exceeds list depth");
                return;
            }
            link = &(*link)->child;
        }
    }
    t_nlist temp;
    temp.x_root = NULL;
    temp.x_len = 0;
    temp.x_depth = 0;
    t_nlist_node *contents = nlist_parse_all(&temp, ac, av);
    t_nlist_node *replacement;
    if(contents && contents->next){
        replacement = nlistset_new_list();
        replacement->child = contents;
    }
    else
        replacement = contents;
    if(!replacement)
        return;
    t_nlist_node *old = *link;
    replacement->next = old->next;
    *link = replacement;
    old->next = NULL;
    nlistset_free_nodes(old);
    nlist->x_len = nlistset_len(nlist->x_root);
    nlist->x_depth = nlistset_depth(nlist->x_root, 0);
    nlist_do_update(nlist);
}

static void *nlistset_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nlistset *x = (t_nlistset *)pd_new(nlistset_class);
    x->x_sym = NULL;
    x->x_path_ac = 0;
    x->x_path = NULL;
    if(ac && av->a_type == A_SYMBOL){
        x->x_sym = atom_getsymbol(av);
        ac--, av++;
    }
    if(ac && av->a_type == A_FLOAT)
        nlistset_set(x, &s_, ac, av);
    else{
        t_atom at[1];
        SETFLOAT(at, 0);
        nlistset_set(x, &s_, 1, at);
    }
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("name"));
    return(x);
}

static void nlistset_free(t_nlistset *x){
    if(x->x_path)
        freebytes(x->x_path, sizeof(int) * x->x_path_ac);
}

void setup_nlist0x2eset(void){
    nlistset_class = class_new(gensym("nlist.set"), (t_newmethod)(void *)nlistset_new,
        (t_method)nlistset_free, sizeof(t_nlistset), 0, A_GIMME, 0);
    class_addlist(nlistset_class, nlistset_list);
    class_addmethod(nlistset_class, (t_method)nlistset_name, gensym("name"), A_SYMBOL, 0);
    class_addmethod(nlistset_class, (t_method)nlistset_set, gensym("set"), A_GIMME, 0);
    class_addmethod(nlistset_class, (t_method)nlistset_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
}
