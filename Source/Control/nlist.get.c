// Porres 2026

#include <m_pd.h>
#include "nlist.h"

typedef struct _nlistget{
    t_object    x_obj;
    t_symbol   *x_sym;
    t_outlet   *x_typeout;
}t_nlistget;

static t_class *nlistget_class;

static void nlistget_set(t_nlistget *x, t_symbol *name){
    x->x_sym = (name == &s_) ? NULL : name;
}

static void nlistget_click(t_nlistget *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
    t_nlist *nlist = nlist_get(x->x_sym, gensym("get"));
    if(nlist)
        nlist_open(nlist);
}

static t_nlist_node *nlistget_find(t_nlist_node *node, int index){
    while(node && index > 0){
        node = node->next;
        index--;
    }
    return(node);
}

static int nlistget_count(t_nlist_node *node){
    int n = 0;
    while(node){
        if(node->type == 1){
            if(node->child)
                n += nlistget_count(node->child);
            else
                n++;
        }
        else
            n++;
        node = node->next;
    }
    return(n);
}

static void nlistget_atom(t_atom *atom, t_atom *out, int *pos, int open, int close){
    if(!open && !close){
        out[(*pos)++] = *atom;
        return;
    }
    char buf[MAXPDSTRING];
    char prefix[MAXPDSTRING];
    char suffix[MAXPDSTRING];
    int p = 0, q = 0;
    for(int i = 0; i < open && p < MAXPDSTRING - 1; i++)
        prefix[p++] = '[';
    prefix[p] = 0;
    for(int i = 0; i < close && q < MAXPDSTRING - 1; i++)
        suffix[q++] = ']';
    suffix[q] = 0;
    if(atom->a_type == A_SYMBOL)
        snprintf(buf, sizeof(buf), "%s%s%s",
            prefix, atom_getsymbol(atom)->s_name, suffix);
    else
        snprintf(buf, sizeof(buf), "%s%g%s",
            prefix, atom_getfloat(atom), suffix);
    t_atom *dst = out + *pos;    // SETSYMBOL expands its argument twice --
    SETSYMBOL(dst, gensym(buf)); // must never be a self-incrementing expression
    (*pos)++;
}

// `open`/`close` = how many '[' / ']' characters are owed to the very
// first / very last leaf atom of THIS sibling chain, inherited from
// enclosing list nodes above us. That inheritance only ever applies to
// the overall first (for open) or overall last (for close) element of
// the chain -- every other element gets none of it. Separately, a nested
// list node ALWAYS contributes exactly one bracket of its own around its
// own first/last leaf, regardless of where that list sits among its
// siblings (that's just the '[' / ']' that was consumed to create it).
static int nlistget_write(t_nlist_node *node, t_atom *out, int pos, int open, int close){
    t_nlist_node *first = node;
    t_nlist_node *last = node;
    while(last && last->next)
        last = last->next;
    while(node){
        int isfirst = node == first;
        int islast = node == last;
        int inherited_open = isfirst ? open : 0;
        int inherited_close = islast ? close : 0;
        if(node->type == 1){
            if(node->child)
                pos = nlistget_write(node->child, out, pos,
                    inherited_open + 1, inherited_close + 1);
            else{
                char buf[MAXPDSTRING];
                int p = 0;
                int totalopen = inherited_open + 1;
                int totalclose = inherited_close + 1;
                for(int i = 0; i < totalopen && p < MAXPDSTRING - 1; i++)
                    buf[p++] = '[';
                for(int i = 0; i < totalclose && p < MAXPDSTRING - 1; i++)
                    buf[p++] = ']';
                buf[p] = 0;
                t_atom *dst = out + pos;
                SETSYMBOL(dst, gensym(buf));
                pos++;
            }
        }
        else
            nlistget_atom(&node->atom, out, &pos, inherited_open, inherited_close);
        node = node->next;
    }
    return(pos);
}

static int nlistget_has_nested(t_nlist_node *node){
    while(node){
        if(node->type == 1)
            return(1);
        node = node->next;
    }
    return(0);
}

static void nlistget_get(t_nlistget *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nlist *nlist = nlist_get(x->x_sym, gensym("get"));
    if(!nlist)
        return;
    if(ac == 1 && av->a_type == A_FLOAT && atom_getfloat(av) == -1){
        int type = nlistget_has_nested(nlist->x_root) ? 2 : 1;
        int n = nlistget_count(nlist->x_root);
        if(!n){
            outlet_float(x->x_typeout, 1);
            outlet_list(x->x_obj.ob_outlet, &s_list, 0, NULL);
            return;
        }
        t_atom *out = (t_atom *)getbytes(sizeof(t_atom) * n);
        int outn = nlistget_write(nlist->x_root, out, 0, 0, 0);
        outlet_float(x->x_typeout, type);
        outlet_list(x->x_obj.ob_outlet, &s_list, outn, out);
        freebytes(out, sizeof(t_atom) * n);
        return;
    }
    t_nlist_node *node = nlist->x_root;
    for(int i = 0; i < ac; i++){
        if(av[i].a_type != A_FLOAT){
            pd_error(x, "[nlist.get] index must be a number");
            return;
        }
        int index = (int)atom_getfloat(av + i);
        if(index < 0){
            outlet_float(x->x_typeout, -1);
            return;
        }
        node = nlistget_find(node, index);
        if(!node){
            outlet_float(x->x_typeout, -1);
            return;
        }
        if(i < ac - 1){
            if(node->type != 1){
                outlet_float(x->x_typeout, -2);
                return;
            }
            node = node->child;
        }
    }
    if(node->type == 0){
        outlet_float(x->x_typeout, 0);
        outlet_list(x->x_obj.ob_outlet, &s_list, 1, &node->atom);
        return;
    }
    int type = nlistget_has_nested(node->child) ? 2 : 1;
    int n = nlistget_count(node->child);
    if(!n){
        outlet_float(x->x_typeout, 1);
        outlet_list(x->x_obj.ob_outlet, &s_list, 0, NULL);
        return;
    }
    t_atom *out = (t_atom *)getbytes(sizeof(t_atom) * n);
    int outn = nlistget_write(node->child, out, 0, 0, 0);
    outlet_float(x->x_typeout, type);
    outlet_list(x->x_obj.ob_outlet, &s_list, outn, out);
    freebytes(out, sizeof(t_atom) * n);
}

static void *nlistget_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nlistget *x = (t_nlistget *)pd_new(nlistget_class);
    x->x_sym = NULL;
    if(ac && av->a_type == A_SYMBOL)
        x->x_sym = atom_getsymbol(av);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("name"));
    outlet_new(&x->x_obj, &s_list);
    x->x_typeout = outlet_new(&x->x_obj, &s_float);
    return(x);
}

void setup_nlist0x2eget(void){
    nlistget_class = class_new(gensym("nlist.get"), (t_newmethod)(void *)nlistget_new,
        0, sizeof(t_nlistget), 0, A_GIMME, 0);
    class_addlist(nlistget_class, nlistget_get);
    class_addmethod(nlistget_class, (t_method)nlistget_set, gensym("name"), A_SYMBOL, 0);
    class_addmethod(nlistget_class, (t_method)nlistget_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
}
