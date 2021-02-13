// porres

#include "m_pd.h"

static t_class *receiver_class, *receiver_proxy_class;

typedef struct _receiver{
    t_object                x_obj;
    struct _receiver_proxy *x_proxy_receiver;
    t_symbol               *x_bound_sym_1;
    t_symbol               *x_bound_sym_2;
}t_receiver;

typedef struct _receiver_proxy{
    t_object    p_pd;
    t_receiver *p_owner;
}t_receiver_proxy;

static void receiver_clear(t_receiver *x){
    t_receiver_proxy *p=x->x_proxy_receiver;
    if(x->x_bound_sym_1)
        pd_unbind(&p->p_pd.ob_pd, x->x_bound_sym_1);
    x->x_bound_sym_1 = 0;
    if(x->x_bound_sym_2)
        pd_unbind(&p->p_pd.ob_pd, x->x_bound_sym_2);
    x->x_bound_sym_2 = 0;
}

static void receiver_symbol(t_receiver *x, t_symbol *sym){
	t_receiver_proxy *p=x->x_proxy_receiver;
    if(x->x_bound_sym_1)
        pd_unbind(&p->p_pd.ob_pd, x->x_bound_sym_1);
    pd_bind(&p->p_pd.ob_pd, x->x_bound_sym_1 = sym);
}

static void receiver_list(t_receiver *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_receiver_proxy *p=x->x_proxy_receiver;
    if(ac > 0){
        if(ac > 2)
            pd_error(x, "[receiver]: list message has too many arguments");
        else if((av)->a_type == A_FLOAT)
            pd_error(x, "[receiver]: list can't take float as an argument");
        else if((av)->a_type == A_SYMBOL){
            if(x->x_bound_sym_1)
                pd_unbind(&p->p_pd.ob_pd, x->x_bound_sym_1);
            pd_bind(&p->p_pd.ob_pd, x->x_bound_sym_1 = atom_getsymbol(av));
        }
        if(ac == 2){
            if((av+1)->a_type == A_FLOAT)
                pd_error(x, "[receiver]: can't take float as an argument");
            else if((av+1)->a_type == A_SYMBOL){
                if(x->x_bound_sym_2)
                    pd_unbind(&p->p_pd.ob_pd, x->x_bound_sym_2);
                pd_bind(&p->p_pd.ob_pd, x->x_bound_sym_2 = atom_getsymbol(av+1));
            }
        }
    }
}

static void receiver_proxy_bang(t_receiver_proxy *p){
	t_receiver *x = p->p_owner;
    outlet_bang(x->x_obj.ob_outlet);
}

static void receiver_proxy_float(t_receiver_proxy *p, t_floatarg f){
	t_receiver *x = p->p_owner;
    outlet_float(x->x_obj.ob_outlet, f);
}

static void receiver_proxy_symbol(t_receiver_proxy *p, t_symbol *s){
	t_receiver *x = p->p_owner;
    outlet_symbol(x->x_obj.ob_outlet, s);
}

static void receiver_proxy_pointer(t_receiver_proxy *p, t_gpointer *gp){
	t_receiver *x = p->p_owner;
    outlet_pointer(x->x_obj.ob_outlet, gp);
}

static void receiver_proxy_list(t_receiver_proxy *p, t_symbol *s, int ac, t_atom *av){
    s = NULL;
	t_receiver *x = p->p_owner;
    outlet_list(x->x_obj.ob_outlet, &s_list, ac, av);
}

static void receiver_proxy_anything(t_receiver_proxy *p, t_symbol *s, int ac, t_atom *av){
	t_receiver *x = p->p_owner;
    outlet_anything(x->x_obj.ob_outlet, s, ac, av);
}

static void receiver_free(t_receiver *x){
	t_receiver_proxy *p=x->x_proxy_receiver;
    if(x->x_bound_sym_1)
        pd_unbind(&p->p_pd.ob_pd, x->x_bound_sym_1);
    if(x->x_bound_sym_2)
        pd_unbind(&p->p_pd.ob_pd, x->x_bound_sym_2);
	if(x->x_proxy_receiver)
        pd_free((t_pd *)x->x_proxy_receiver);
}

static void *receiver_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_receiver *x = (t_receiver *)pd_new(receiver_class);
	t_receiver_proxy *p = (t_receiver_proxy *)pd_new(receiver_proxy_class);
    x->x_proxy_receiver = p;
    p->p_owner = x;
    x->x_bound_sym_1 = x->x_bound_sym_2 = 0;
    if(ac > 0){
        if(ac > 2){
            pd_error(x, "[receiver]: too many arguments");
            return(NULL);
        }
        if((av)->a_type == A_FLOAT){
            pd_error(x, "[receiver]: can't take float as an argument");
            return(NULL);
        }
        if((av)->a_type == A_SYMBOL){
            x->x_bound_sym_1 = atom_getsymbol(av);
            pd_bind(&p->p_pd.ob_pd, x->x_bound_sym_1);
        }
        if(ac == 2){
            if((av+1)->a_type == A_FLOAT){
                pd_error(x, "[receiver]: can't take float as an argument");
                return(NULL);
            }
            if((av+1)->a_type == A_SYMBOL){
                x->x_bound_sym_2 = atom_getsymbol(av+1);
                pd_bind(&p->p_pd.ob_pd, x->x_bound_sym_2);
            }
        }
    }
    outlet_new(&x->x_obj, &s_list);
    return(x);
}

void receiver_setup(void){
    receiver_class = class_new(gensym("receiver"), (t_newmethod)receiver_new,
        (t_method)receiver_free, sizeof(t_receiver), 0, A_GIMME, 0);
    class_addmethod(receiver_class, (t_method)receiver_clear, gensym("clear"), 0);
    class_addsymbol(receiver_class, receiver_symbol);
    class_addlist(receiver_class, receiver_list);
    receiver_proxy_class = class_new(gensym("_receiver_proxy"), 0, 0,
        sizeof(t_receiver_proxy), CLASS_PD | CLASS_NOINLET, 0);
    class_addbang(receiver_proxy_class, receiver_proxy_bang);
    class_addfloat(receiver_proxy_class, receiver_proxy_float);
	class_addsymbol(receiver_proxy_class, receiver_proxy_symbol);
	class_addpointer(receiver_proxy_class, receiver_proxy_pointer);
    class_addlist(receiver_proxy_class, receiver_proxy_list);
    class_addanything(receiver_proxy_class, receiver_proxy_anything);
}
