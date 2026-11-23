// Porres 2026

#include <m_pd.h>
#include "nlist.h"

typedef struct _nl_walk{
    t_object        x_obj;
    t_symbol       *x_sym;
    int             x_level;
    int             x_index;
    int             x_all_level;
    int             x_all_index;
    t_outlet       *x_info_out;
    t_outlet       *x_end_out;
}t_nl_walk;

static t_class *nl_walk_class;

static void nl_walk_name(t_nl_walk *x, t_symbol *name){
    x->x_sym = (name == &s_) ? NULL : name;
}

static void nl_walk_reset(t_nl_walk *x){
    x->x_index = 0;
    x->x_all_level = 0;
    x->x_all_index = 0;
}

// The walk cursor is (level, index), where index is the number of nodes at
// that depth, in depth-first order across the WHOLE tree, that come before
// the current one. It lives across messages, so unlike [nlist.iter] and
// [nlist.traverse] it is positional by design: if the list is reshaped
// between two "next" messages, the position is simply re-applied to the new
// tree.

// Number of nodes at depth 'target' in this sibling chain (and below),
// where 'depth' is the depth of 'node'.
static int nl_walk_count_at(t_nlist_node *node, int depth, int target){
    int n = 0;
    while(node){
        if(depth == target)
            n++;
        else if(node->child)
            n += nl_walk_count_at(node->child, depth + 1, target);
        node = node->next;
    }
    return(n);
}

// Counts the nodes at depth 'target' that come before the exact path
// target_path[] in depth-first order, using the same ordering as
// nl_walk_find_nth(). At every level, each earlier sibling contributes ALL
// the target-depth nodes in its subtree, and only then do we descend into the
// sibling the path points at.
// Returns 1 and adds to *count on success, 0 if the path does not exist.
static int nl_walk_count_before(t_nlist_node *node, int depth, int target,
    t_atom *target_path, int *count){
    int want = (int)atom_getfloat(target_path + depth);
    for(int i = 0; i < want; i++){
        if(!node)
            return(0);
        if(depth == target)
            (*count)++;
        else if(node->child)
            *count += nl_walk_count_at(node->child, depth + 1, target);
        node = node->next;
    }
    if(!node)
        return(0);                  // index out of range at this depth
    if(depth == target)
        return(1);
    if(!node->child)
        return(0);                  // prefix is not a (non-empty) list
    return(nl_walk_count_before(node->child, depth + 1, target, target_path, count));
}

static int nl_walk_find_nth(t_nlist_node *node, int depth, int target,
    int *skip, t_atom *path){
    int index = 0;
    while(node){
        SETFLOAT(&path[depth], index++);
        if(depth == target){
            if(*skip == 0)
                return(1);
            (*skip)--;
        }
        else if(node->child){
            if(nl_walk_find_nth(node->child, depth + 1, target, skip, path))
                return(1);
        }
        node = node->next;
    }
    return(0);
}

static void nl_walk_goto(t_nl_walk *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nlist *nlist = nlist_get(x->x_sym, gensym("walk"));
    if(!nlist || !ac)
        return;
    int level = ac - 1;
    int index = 0;
    t_atom at;
    SETFLOAT(&at, 0);
    if(level < 0 || level > nlist->x_depth)
        goto fail;
    for(int i = 0; i < ac; i++){
        t_float f = atom_getfloat(av + i);
        if(av[i].a_type != A_FLOAT || f < 0 || f != (int)f)
            goto fail;
    }
    if(!nl_walk_count_before(nlist->x_root, 0, level, av, &index))
        goto fail;
    x->x_level = level;
    x->x_index = index;
    x->x_all_level = level;
    x->x_all_index = index;
    SETFLOAT(&at, 1); // "goto 1" if success, "goto 0" if not.
fail:
    outlet_anything(x->x_info_out, gensym("goto"), 1, &at);
}

// Outlets can re-enter this object (feedback loops, [until]) or mutate/delete
// the nlist. So: find the target and commit ALL state first, then fire the
// outlet, and never touch 'nlist' or 'x' state again afterwards.
static void nl_walk_next(t_nl_walk *x){
    t_nlist *nlist = nlist_get(x->x_sym, gensym("walk"));
    if(!nlist)
        return;
    // size the buffer from the level we'll actually write to, and remember it,
    // since nlist->x_depth may be different by the time we free
    int size = (x->x_level >= 0 ? x->x_level : nlist->x_depth) + 1;
    t_atom *path = getbytes(size * sizeof(t_atom));
    int found = 0, level = 0;
    if(x->x_level >= 0){
        level = x->x_level;
        int skip = x->x_index;
        if((found = nl_walk_find_nth(nlist->x_root, 0, level, &skip, path)))
            x->x_index++;
    }
    else{
        while(x->x_all_level <= nlist->x_depth){
            level = x->x_all_level;
            int skip = x->x_all_index;
            if((found = nl_walk_find_nth(nlist->x_root, 0, level, &skip, path))){
                x->x_all_index++;
                break;
            }
            x->x_all_level++;
            x->x_all_index = 0;
        }
    }
    t_outlet *out = x->x_obj.ob_outlet;
    t_outlet *end_out = x->x_end_out;
    if(found)
        outlet_list(out, &s_list, level + 1, path);
    freebytes(path, size * sizeof(t_atom));
    if(!found)
        outlet_bang(end_out);
}

static void nl_walk_skip(t_nl_walk *x){
    t_nlist *nlist = nlist_get(x->x_sym, gensym("walk"));
    if(!nlist)
        return;
    int level = x->x_level >= 0 ? x->x_level : x->x_all_level;
    int index = x->x_level >= 0 ? x->x_index : x->x_all_index;
    if(level > nlist->x_depth)
        return;
    int size = level + 1;
    t_atom *path = getbytes(size * sizeof(t_atom));
    int skip = index;
    int found = nl_walk_find_nth(nlist->x_root, 0, level, &skip, path);
    t_outlet *info_out = x->x_info_out;
    t_outlet *end_out = x->x_end_out;
    if(found){ // commit state BEFORE the outlet fires (re-entrancy safe)
        if(x->x_level >= 0)
            x->x_index++;
        else
            x->x_all_index++;
        outlet_anything(info_out, gensym("skip"), level + 1, path);
    }
    freebytes(path, size * sizeof(t_atom));
    if(!found)
        outlet_bang(end_out);
}

static void nl_walk_float(t_nl_walk *x, t_floatarg f){
    t_nlist *nlist = nlist_get(x->x_sym, gensym("walk"));
    if(!nlist)
        return;
    int level = (int)f;
    if(level < 0)
        x->x_level = -1;
    else if(level > nlist->x_depth){
        post("[nlist.walk] %d depth out of range", level);
        return;
    }
    else
        x->x_level = level;
    nl_walk_reset(x);
}

static void nl_walk_click(t_nl_walk *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
    t_nlist *nlist = nlist_get(x->x_sym, gensym("walk"));
    if(nlist)
        nlist_open(nlist);
}

static void *nl_walk_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nl_walk *x = (t_nl_walk *)pd_new(nl_walk_class);
    x->x_sym = NULL;
    x->x_level = 0;
    x->x_index = 0;
    x->x_all_level = 0;
    x->x_all_index = 0;
    if(ac && av->a_type == A_SYMBOL)
        x->x_sym = atom_getsymbol(av);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("name"));
    outlet_new(&x->x_obj, &s_list);
    x->x_info_out = outlet_new((t_object *)x, &s_anything);
    x->x_end_out = outlet_new((t_object *)x, &s_bang);
    return(x);
}

void setup_nlist0x2ewalk(void){
    nl_walk_class = class_new(gensym("nlist.walk"),
        (t_newmethod)(void *)nl_walk_new, 0, sizeof(t_nl_walk), 0, A_GIMME, 0);
    class_addbang(nl_walk_class, nl_walk_next);
    class_addfloat(nl_walk_class, nl_walk_float);
    class_addmethod(nl_walk_class, (t_method)nl_walk_next, gensym("next"), 0);
    class_addmethod(nl_walk_class, (t_method)nl_walk_skip, gensym("skip"), 0);
    class_addmethod(nl_walk_class, (t_method)nl_walk_reset, gensym("reset"), 0);
    class_addmethod(nl_walk_class, (t_method)nl_walk_goto, gensym("goto"), A_GIMME, 0);
    class_addmethod(nl_walk_class, (t_method)nl_walk_name, gensym("name"), A_SYMBOL, 0);
    class_addmethod(nl_walk_class, (t_method)nl_walk_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
}
