// Porres 2026

#include <m_pd.h>

#include "nlist.h"

typedef struct _nl_sweep{
    t_object    x_obj;
    t_symbol   *x_sym;
    t_outlet   *x_lvl_out;
}t_nl_sweep;

static t_class *nl_sweep_class;

static void nl_sweep_name(t_nl_sweep *x, t_symbol *name){
    x->x_sym = (name == &s_) ? NULL : name;
}

static void nl_sweep_click(t_nl_sweep *x, t_floatarg xpos, t_floatarg ypos,
t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos, (void)ypos, (void)shift, (void)ctrl, (void)alt;
    t_nlist *nlist = nlist_get(x->x_sym, gensym("sweep"));
    if(nlist)
        nlist_open(nlist);
}

// ------------ Mutation-aware traversal ------------------------------------
//
// Same approach as [nlist.iter]. Pd's message passing is synchronous, so an
// outlet_list() here can run straight through [nlist.set], [nlist.delete],
// [nlist.insert] etc. and back before we return.
//
//  - We never hold a node pointer across an outlet call: before every
//    emission the list is re-resolved by name and re-scanned from the live
//    root.
//  - The cursor is an identity (unique node IDs), not a count. Before
//    emitting a node at the target depth we note the ID of the next one at
//    that depth. After the outlet returns we resume at that one if it still
//    exists (so anything the receiver set/inserted in between is NOT
//    visited), else right after the node we just sent if it still exists,
//    else we stop.
//
// In bang mode the depth is re-read on every level, so levels that appear
// (or disappear) while we're running are handled.
//
// Cost: each emission re-scans from the root, so this is roughly O(n^2) in
// the number of matches at a given depth. Fine for interactively-sized lists.
static void nl_sweep_emit_level(t_symbol *sym, int target, t_outlet *outlet){
    t_nlist_cursor c;
    nlist_cursor_init(&c);
    t_atom *path;
    int cap, depth;
    while(nlist_cursor_next(&c, sym, NLIST_SEL_LEVEL, target, &path, &cap, &depth, NULL)){
        outlet_list(outlet, &s_list, depth + 1, path);
        freebytes(path, cap * sizeof(t_atom));
    }
}

static void nl_sweep_bang(t_nl_sweep *x){
    t_symbol *sym = x->x_sym;
    if(!nlist_get(sym, gensym("sweep")))
        return;
    for(int lvl = 0; ; lvl++){
        t_nlist *nlist = nlist_find(sym); // re-resolve + re-read depth each level
        if(!nlist || lvl > nlist->x_depth)
            break;
        outlet_float(x->x_lvl_out, lvl);
        nl_sweep_emit_level(sym, lvl, x->x_obj.ob_outlet);
    }
}

static void nl_sweep_float(t_nl_sweep *x, t_floatarg f){
    t_symbol *sym = x->x_sym;
    t_nlist *nlist = nlist_get(sym, gensym("sweep"));
    if(!nlist)
        return;
    int lvl = f < 0 ? 0 : (int)f;
    if(lvl > nlist->x_depth){
        post("[nlist.sweep] %d depth out of range", lvl);
        return;
    }
    nl_sweep_emit_level(sym, lvl, x->x_obj.ob_outlet);
}

static void *nl_sweep_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_nl_sweep *x = (t_nl_sweep *)pd_new(nl_sweep_class);
    x->x_sym = NULL;
    if(ac && av->a_type == A_SYMBOL)
        x->x_sym = atom_getsymbol(av);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("name"));
    outlet_new(&x->x_obj, &s_list);
    x->x_lvl_out = outlet_new((t_object *)x, &s_float);
    return(x);
}

void setup_nlist0x2esweep(void){
    nl_sweep_class = class_new(gensym("nlist.sweep"),
        (t_newmethod)(void *)nl_sweep_new, 0, sizeof(t_nl_sweep), 0, A_GIMME, 0);
    class_addbang(nl_sweep_class, nl_sweep_bang);
    class_addfloat(nl_sweep_class, nl_sweep_float);
    class_addmethod(nl_sweep_class, (t_method)nl_sweep_name, gensym("name"), A_SYMBOL, 0);
    class_addmethod(nl_sweep_class, (t_method)nl_sweep_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, 0);
}
