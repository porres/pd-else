// Porres 2026

#include <m_pd.h>

#include "nlist.h"

typedef struct _nl_traverse{
    t_object    x_obj;
    t_symbol   *x_sym;
    t_outlet   *x_lvl_out;
}t_nl_traverse;

static t_class *nl_traverse_class;

static void nl_traverse_name(t_nl_traverse *x, t_symbol *name){
    x->x_sym = (name == &s_) ? NULL : name;
}

static void nl_traverse_click(t_nl_traverse *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
    t_nlist *nlist = nlist_get(x->x_sym, gensym("traverse"));
    if(nlist)
        nlist_open(nlist);
}

// ------------ Mutation-aware traversal ------------------------------------
//
// Pd's message passing is synchronous: an outlet_list() call here can run
// straight through something like [nlist.delete] and back before we return.
// So we never hold on to a t_nlist_node pointer (or a "next" pointer) across
// an outlet call - it might be dangling by the time we come back to use it.
//
// Instead, before every single emission we re-walk the tree from the live
// root, skipping over however many matches at this depth we've already
// emitted, and send out whichever match comes next. If a fresh scan can't
// find that many matches anymore, we're done - nothing more is sent, rather
// than fabricating an index that no longer exists. This makes the traversal
// genuinely reflect the tree as it stands at each individual step, so it
// stays correct even when something you're outputting to deletes, inserts,
// or otherwise reshapes the list in response.
//
// Cost: each emission re-scans from the root, so this is roughly O(n^2) in
// the number of matches at a given depth. Fine for interactively-sized
// lists; if you're driving genuinely huge nlists through this object,
// that's worth keeping in mind.
static int nl_traverse_emit_nth(t_nlist_node *node, int depth, int target,
    int *skip, t_atom *path, t_outlet *outlet){
    int index = 0;
    while(node){
        SETFLOAT(&path[depth], index++);
        if(depth == target){
            if(*skip == 0){
                outlet_list(outlet, &s_list, depth + 1, path);
                return(1);
            }
            (*skip)--;
        }
        else if(node->child){
            if(nl_traverse_emit_nth(node->child, depth + 1, target, skip, path, outlet))
                return(1);
        }
        node = node->next;
    }
    return(0);
}

// Emits every currently-existing match at "target" depth, re-deriving its
// position from the live root before each one.
static void nl_traverse_emit_level(t_nlist *nlist, int target, t_atom *path, t_outlet *outlet){
    int count = 0;
    while(1){
        int skip = count;
        if(!nl_traverse_emit_nth(nlist->x_root, 0, target, &skip, path, outlet))
            break;
        count++;
    }
}

static void nl_traverse_bang(t_nl_traverse *x){
    t_nlist *nlist = nlist_get(x->x_sym, gensym("traverse"));
    if(nlist){
        int depth = nlist->x_depth;
        t_atom *path = getbytes((depth + 1) * sizeof(t_atom));
        for(int lvl = 0; lvl <= depth; lvl++){
            outlet_float(x->x_lvl_out, lvl);
            nl_traverse_emit_level(nlist, lvl, path, x->x_obj.ob_outlet);
        }
        freebytes(path, (depth + 1) * sizeof(t_atom));
    }
}

static void nl_traverse_float(t_nl_traverse *x, t_floatarg f){
    t_nlist *nlist = nlist_get(x->x_sym, gensym("traverse"));
    if(!nlist)
        return;
    int lvl = f < 0 ? 0 : (int)f;
    if(lvl > nlist->x_depth){
        post("[nlist.traverse] %d depth out of range", lvl);
        return;
    }
    t_atom *path = getbytes((nlist->x_depth + 1) * sizeof(t_atom));
    nl_traverse_emit_level(nlist, lvl, path, x->x_obj.ob_outlet);
    freebytes(path, (nlist->x_depth + 1) * sizeof(t_atom));
}

static void *nl_traverse_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nl_traverse *x = (t_nl_traverse *)pd_new(nl_traverse_class);
    x->x_sym = NULL;
    if(ac && av->a_type == A_SYMBOL)
        x->x_sym = atom_getsymbol(av);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("name"));
    outlet_new(&x->x_obj, &s_list);
    x->x_lvl_out = outlet_new((t_object *)x, &s_float);
    return(x);
}

void setup_nlist0x2etraverse(void){
    nl_traverse_class = class_new(gensym("nlist.traverse"),
        (t_newmethod)(void *)nl_traverse_new, 0, sizeof(t_nl_traverse), 0, A_GIMME, 0);
    class_addbang(nl_traverse_class, nl_traverse_bang);
    class_addfloat(nl_traverse_class, nl_traverse_float);
    class_addmethod(nl_traverse_class, (t_method)nl_traverse_name, gensym("name"), A_SYMBOL, 0);
    class_addmethod(nl_traverse_class, (t_method)nl_traverse_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
}
