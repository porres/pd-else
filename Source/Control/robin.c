
#include "m_pd.h"

#define ROBIN_MAXOUTS  4096

typedef struct _robin{
    t_object    x_obj;
    int         x_index;
    int         x_nouts;
    t_outlet  **x_outs;
}t_robin;

static t_class *robin_class;

static void robin_list(t_robin *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(x->x_index >= x->x_nouts)
        x->x_index = 0;
    if(!ac)
        outlet_bang(x->x_outs[x->x_index++]);
    else if(ac == 1){
        if(av->a_type == A_FLOAT)
            outlet_float(x->x_outs[x->x_index++], av->a_w.w_float);
        else if(av->a_type == A_SYMBOL)
            outlet_symbol(x->x_outs[x->x_index++], av->a_w.w_symbol);
    }
    else
        outlet_list(x->x_outs[x->x_index++], &s_list, ac, av);
}

static void robin_anything(t_robin *x, t_symbol *s, int ac, t_atom *av){
    if(s == &s_list)
        robin_list(x, NULL, ac, av);
    else{
        if(x->x_index >= x->x_nouts)
            x->x_index = 0;
        outlet_anything(x->x_outs[x->x_index++], s, ac, av);
    }
}

static void robin_set(t_robin *x, t_floatarg f){
    int i = (int)f - 1;
    x->x_index = i < 0 ? 0 : i >= x->x_nouts ? x->x_nouts-1: i;
}

static void robin_free(t_robin *x){
    if(x->x_outs)
        freebytes(x->x_outs, x->x_nouts * sizeof(*x->x_outs));
}

static void *robin_new(t_floatarg f1){
    t_robin *x = (t_robin *)pd_new(robin_class);
    int i, nouts = (int)f1;
    t_outlet **outs;
    if(nouts < 1)
        nouts = 1;
    if(nouts > ROBIN_MAXOUTS){
        post("[robin]: maximum outputs is %d", ROBIN_MAXOUTS);
        nouts = ROBIN_MAXOUTS;
    }
    if(!(outs = (t_outlet **)getbytes(nouts * sizeof(*outs))))
        return (0);
    x->x_nouts = nouts;
    x->x_outs = outs;
    x->x_index = 0;
    for(i = 0; i < nouts; i++)
        x->x_outs[i] = outlet_new((t_object *)x, &s_anything);
    return(x);
}

void robin_setup(void){
    robin_class = class_new(gensym("robin"), (t_newmethod)(void*)robin_new,
        (t_method)robin_free, sizeof(t_robin), 0, A_DEFFLOAT, 0);
    class_addanything(robin_class, robin_anything);
    class_addmethod(robin_class, (t_method)robin_set, gensym("set"), A_FLOAT, 0);
}
