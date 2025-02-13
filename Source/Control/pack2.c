// porres

#include <m_pd.h>

static t_class *pack2_class;
static t_class* pack2_inlet_class;

struct _pack2_inlet;

typedef struct _pack2{
    t_object             x_obj;
    t_int                x_n;
    t_atom*              x_vec;
    t_atom*              x_out;
    struct _pack2_inlet *x_ins;
    t_symbol            *x_ignore;
}t_pack2;

typedef struct _pack2_inlet{
    t_class*  x_pd;
    t_atom*   x_atoms;
    t_int     x_max;
    t_pack2*  x_owner;
    int       x_idx; //index of inlet
    t_symbol *x_ignore;
}t_pack2_inlet;

static void pack2_out(t_pack2 *x){
    for(int i = 0; i < x->x_n; ++i)
        x->x_out[i] = x->x_vec[i];
    outlet_anything(x->x_obj.ob_outlet, &s_list, x->x_n, x->x_out);
}

static void pack2_copy(int ndest, t_atom* dest, int nsrc, t_atom* src){
    for(int i = 0; i < ndest && i < nsrc; ++i){
        if(src[i].a_type == A_FLOAT){
            if(dest[i].a_type == A_SYMBOL)
                dest[i].a_type = A_FLOAT;
            dest[i].a_w.w_float = src[i].a_w.w_float;
        }
        else if(src[i].a_type == A_SYMBOL){
            if(dest[i].a_type == A_FLOAT)
                dest[i].a_type = A_SYMBOL;
            dest[i].a_w.w_symbol = src[i].a_w.w_symbol; 
        }
    }
}

static void pack2_inlet_float(t_pack2_inlet *x, float f){
    if(x->x_atoms->a_type == A_SYMBOL)
        x->x_atoms->a_type = A_FLOAT;
    x->x_atoms->a_w.w_float = f;
    pack2_out(x->x_owner);
}

static void pack2_inlet_symbol(t_pack2_inlet *x, t_symbol* s){
    if(x->x_atoms->a_type == A_FLOAT)
        x->x_atoms->a_type = A_SYMBOL;
    x->x_atoms->a_w.w_symbol = s;
    pack2_out(x->x_owner);
}

static void pack2_inlet_list(t_pack2_inlet *x, t_symbol* s, int ac, t_atom* av){
    x->x_ignore = s;
    if(!ac && x->x_idx != 0){
        pd_error(x, "pack2: secondary inlet doesn't expect bang");
        return;
    }
    else if(x->x_idx != 0){
        pd_error(x, "pack2: secondary inlet doesn't expect list");
        return;
    }
    else if(!ac){
        pack2_out(x->x_owner);
        return;
    }
    if(ac == 1){
        if(av->a_type == A_FLOAT)
            pack2_inlet_float(x, atom_getfloat(av));
        else if(av->a_type == A_SYMBOL)
            pack2_inlet_symbol(x, atom_getsymbol(av));
    }
    else{
        pack2_copy(x->x_max, x->x_atoms, ac, av);
        pack2_out(x->x_owner);
    }
}

static void pack2_inlet_set(t_pack2_inlet *x, t_symbol* s, int ac, t_atom* av){
    x->x_ignore = s;
    pack2_copy(x->x_max, x->x_atoms, ac = 1, av);
}

static void *pack2_new(t_symbol *s, int ac, t_atom *av){
    t_pack2 *x = (t_pack2 *)pd_new(pack2_class);
    x->x_ignore = s;
    int i;
    t_atom defarg[2];
    if(!ac){
        av = defarg;
        ac = 2;
        SETFLOAT(&defarg[0], 0);
        SETFLOAT(&defarg[1], 0);
    }
    x->x_n = ac;
    x->x_vec = (t_atom *)getbytes(ac * sizeof(*x->x_vec));
    x->x_out = (t_atom *)getbytes(ac * sizeof(*x->x_out));
    x->x_ins = (t_pack2_inlet *)getbytes(ac * sizeof(*x->x_ins));
    for(i = 0; i < x->x_n; ++i){
        if(av[i].a_type == A_FLOAT){
            x->x_vec[i].a_type      = A_FLOAT;
            x->x_vec[i].a_w.w_float = av[i].a_w.w_float;
            x->x_ins[i].x_pd = pack2_inlet_class;
            x->x_ins[i].x_atoms = x->x_vec+i;
            x->x_ins[i].x_max = x->x_n-i;
            x->x_ins[i].x_owner = x;
            x->x_ins[i].x_idx = i;
            inlet_new((t_object *)x, &(x->x_ins[i].x_pd), 0, 0);
        }
        else if(av[i].a_type == A_SYMBOL){
            if(av[i].a_w.w_symbol == gensym("f") ||
               av[i].a_w.w_symbol == gensym("float")){ 
                x->x_vec[i].a_type = A_FLOAT;
                x->x_vec[i].a_w.w_float = 0.f;
                x->x_ins[i].x_pd = pack2_inlet_class;
                x->x_ins[i].x_atoms = x->x_vec+i;
                x->x_ins[i].x_max = x->x_n-i;
                x->x_ins[i].x_owner = x;
                x->x_ins[i].x_idx = i;
                inlet_new((t_object *)x, &(x->x_ins[i].x_pd), 0, 0);
            }
            else{
                x->x_vec[i].a_type = A_SYMBOL;
                x->x_vec[i].a_w.w_symbol = av[i].a_w.w_symbol;
                x->x_ins[i].x_pd = pack2_inlet_class;
                x->x_ins[i].x_atoms = x->x_vec+i;
                x->x_ins[i].x_max = x->x_n-i;
                x->x_ins[i].x_owner = x;
                x->x_ins[i].x_idx = i;
                inlet_new((t_object *)x, &(x->x_ins[i].x_pd), 0, 0);
            }
        }
    }
    outlet_new(&x->x_obj, &s_list);
    return(x);
}

static void pack2_free(t_pack2 *x){
    freebytes(x->x_vec, x->x_n * sizeof(*x->x_vec));
    freebytes(x->x_out, x->x_n * sizeof(*x->x_out));
    freebytes(x->x_ins, x->x_n * sizeof(*x->x_ins));
}

extern void pack2_setup(void){
    t_class* c = NULL;
    c = class_new(gensym("pack2-inlet"), 0, 0, sizeof(t_pack2_inlet), CLASS_PD, 0);
    if(c){
        class_addlist(c, (t_method)pack2_inlet_list);
        class_addfloat(c, (t_method)pack2_inlet_float);
        class_addsymbol(c, (t_method)pack2_inlet_symbol);
        class_addmethod(c, (t_method)pack2_inlet_set, gensym("set"), A_GIMME, 0);
    }
    pack2_inlet_class = c;
    c = class_new(gensym("pack2"), (t_newmethod)pack2_new, (t_method)pack2_free,
        sizeof(t_pack2), CLASS_NOINLET, A_GIMME, 0);
    pack2_class = c;
}
