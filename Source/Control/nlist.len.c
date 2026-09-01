// Porres 2026

#include <m_pd.h>
#include "nlist.h"

typedef struct _nlistlen{
    t_object    x_obj;
    t_symbol   *x_sym;
    t_outlet   *x_idx_out;
}t_nlistlen;

static t_class *nlistlen_class;

static void nlistlen_name(t_nlistlen *x, t_symbol *name){
    x->x_sym = (name == &s_) ? NULL : name;
}

static void nlistlen_click(t_nlistlen *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
    t_nlist *nlist = nlist_get(x->x_sym, gensym("len"));
    if(nlist)
        nlist_open(nlist);
}

static void nlistlen_level(t_nlist_node *node, int depth, int target,
    t_atom *path, t_outlet *len_out, t_outlet *idx_out){
    int index = 0;
    while(node){
        SETFLOAT(&path[depth], index);
        if(depth == target){
            int len = 0;
            t_nlist_node *child = node->child;
            while(child){
                len++;
                child = child->next;
            }
            if(len){
                outlet_list(idx_out, &s_list, depth + 1, path);
                outlet_float(len_out, len);
            }
        }
        else if(node->child)
            nlistlen_level(node->child, depth + 1, target,
                path, len_out, idx_out);
        index++;
        node = node->next;
    }
}

static void nlistlen_bang(t_nlistlen *x){
    t_nlist *nlist = nlist_get(x->x_sym, gensym("len"));
    if(nlist){
        outlet_bang(x->x_idx_out);
        outlet_float(x->x_obj.ob_outlet, nlist->x_len);
    }
}

static void nlistlen_float(t_nlistlen *x, t_floatarg f){
    int level = (int)f;
    if(level == 0){
        nlistlen_bang(x);
        return;
    }
    t_nlist *nlist = nlist_get(x->x_sym, gensym("len"));
    if(nlist){
        if(level > nlist->x_depth){
            post("[nlist.len] %d depth level out of range", level);
            return;
        }
        t_atom *path = getbytes(nlist->x_depth * sizeof(t_atom));
        nlistlen_level(nlist->x_root, 0, level - 1, path,
        x->x_obj.ob_outlet, x->x_idx_out);
        freebytes(path, nlist->x_depth * sizeof(t_atom));
    }
}

static void *nlistlen_new(t_symbol *s, int ac, t_atom* av){
    (void)s;
    t_nlistlen *x = (t_nlistlen *)pd_new(nlistlen_class);
    x->x_sym = NULL;
    if(ac && av->a_type == A_SYMBOL)
        x->x_sym = atom_getsymbol(av);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("name"));
    outlet_new(&x->x_obj, &s_float);
    x->x_idx_out = outlet_new((t_object *)x, &s_list);
    return(x);
}

void setup_nlist0x2elen(void){
    nlistlen_class = class_new(gensym("nlist.len"),
        (t_newmethod)(void *)nlistlen_new, 0, sizeof(t_nlistlen), 0, A_GIMME, 0);
    class_addbang(nlistlen_class, nlistlen_bang);
    class_addfloat(nlistlen_class, nlistlen_float);
    class_addmethod(nlistlen_class, (t_method)nlistlen_name, gensym("name"), A_SYMBOL, 0);
    class_addmethod(nlistlen_class, (t_method)nlistlen_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
}
