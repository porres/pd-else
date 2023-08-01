#include "m_pd.h"

static t_class *order_class;

typedef struct _order{
    t_object    x_obj;
    t_int       x_offset;
    t_int       x_n;
}t_order;

static void order_n(t_order *x, t_floatarg f){
    x->x_n = f < 1 ? 1 : (int)f;
}

static void order_offset(t_order *x, t_floatarg f){
    x->x_offset = (int)f;
}

static void order_list(t_order *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(!ac)
        return;
    int i = x->x_offset;
    while(ac){
        if(ac >= x->x_n){
            t_atom at[x->x_n+1];
            SETFLOAT(at, i);
            for(int n = 0; n < x->x_n; n++){
                if(av->a_type == A_FLOAT)
                    SETFLOAT(at+1+n, atom_getfloat(av++));
                else if(av->a_type == A_SYMBOL)
                    SETSYMBOL(at+1+n, atom_getsymbol(av++));
                else if(av->a_type == A_POINTER)
                    SETPOINTER(at+1+n, (av++)->a_w.w_gpointer);
                ac--;
            }
            outlet_list(x->x_obj.ob_outlet, &s_list, x->x_n+1, at);
        }
        else{
            int size = ac;
            t_atom at[size+1];
            SETFLOAT(at, i);
            for(int n = 0; n < size; n++){
                if(av->a_type == A_FLOAT)
                    SETFLOAT(at+1+n, atom_getfloat(av++));
                else if(av->a_type == A_SYMBOL)
                    SETSYMBOL(at+1+n, atom_getsymbol(av++));
                else if(av->a_type == A_POINTER)
                    SETPOINTER(at+1+n, (av++)->a_w.w_gpointer);
                ac--;
            }
            outlet_list(x->x_obj.ob_outlet, &s_list, size+1, at);
        }
        i++;
    }
}

static void *order_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_order *x = (t_order *)pd_new(order_class);
    x->x_n = 1;
    x->x_offset = 0;
    if(ac){
        if(av->a_type == A_FLOAT){
            t_float n = atom_getint(av);
            x->x_n = n < 1 ? 1 : n;
            av++, ac--;
        }
        if(av->a_type == A_FLOAT)
            x->x_offset = atom_getint(av);
    }
    outlet_new(&x->x_obj, &s_list);
    return(x);
}

void order_setup(void){
    order_class = class_new(gensym("order"), (t_newmethod)order_new,
        0, sizeof(t_order), 0, A_GIMME, 0);
    class_addlist(order_class, order_list);
    class_addmethod(order_class, (t_method)order_n, gensym("n"), A_FLOAT, 0);
    class_addmethod(order_class, (t_method)order_offset, gensym("offset"), A_FLOAT, 0);
}
