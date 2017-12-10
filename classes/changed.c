// porres 2017

#include "m_pd.h"

static t_class *changed_class;

typedef struct _changed{
    t_object   x_obj;
    t_atom     x_a[4096];
    int        x_c;
    int        x_change;
    t_symbol  *x_sym;
    t_inlet   *x_set_inlet;
    t_outlet  *x_bang_out;
} t_changed;

static void changed_bang(t_changed *x){
    outlet_anything(x->x_obj.ob_outlet, x->x_sym, x->x_c, x->x_a);
}

static void changed_anything(t_changed *x, t_symbol *s, int argc, t_atom *argv){
    int i;
    int c = argc;
    x->x_sym = s;
    if(c == x->x_c)
        for (i = 0; i < c; i++){
            if (x->x_a[i].a_type == A_FLOAT){
                if (argv[i].a_type != A_FLOAT || x->x_a[i].a_w.w_float != argv[i].a_w.w_float)
                    x->x_change = 1;
                break;
            }
            else if (x->x_a[i].a_type == A_SYMBOL){
                if (argv[i].a_type != A_SYMBOL || x->x_a[i].a_w.w_symbol != argv[i].a_w.w_symbol)
                    x->x_change = 1;
                break;
            }
        }
    else
        x->x_change = 1;
    if (x->x_change){
        x->x_c = c;
        for (i = 0; i < c; i++)
            x->x_a[i] = argv[i];
        outlet_anything(x->x_obj.ob_outlet, s, argc, argv);
        x->x_change = 0;
    }
    else
        outlet_bang(x->x_bang_out);
}

static void changed_set(t_changed *x, t_symbol *s, int argc, t_atom *argv){
    int i;
    if(argc == 0)
        x->x_sym = &s_;
    else if(argc == 1){
        if ((argv)->a_type == A_SYMBOL){
            x->x_sym = atom_getsymbol(argv);
            argc--;
        }
        else if ((argv)->a_type == A_FLOAT)
            x->x_sym = &s_float;
    }
    else{
        if((argv)->a_type == A_SYMBOL){
            x->x_sym = atom_getsymbol(argv++);
            argc--;
        }
        else
            x->x_sym = &s_list;
    }
    x->x_c = argc;
    for (i = 0; i < argc; i++)
        x->x_a[i] = argv[i];
}

static void *changed_new(t_symbol *s, int argc, t_atom *argv){
    t_changed *x = (t_changed *)pd_new(changed_class);
    int i;
    if(argc == 0)
        x->x_sym = &s_;
    else if(argc == 1){
        if ((argv)->a_type == A_SYMBOL){
            x->x_sym = atom_getsymbol(argv);
            argc--; 
        }
        else if ((argv)->a_type == A_FLOAT)
                x->x_sym = &s_float;
    }
    else{
        if((argv)->a_type == A_SYMBOL){
            x->x_sym = atom_getsymbol(argv++);
            argc--;
        }
        else
            x->x_sym = &s_list;
    }
    x->x_c = argc;
    for (i = 0; i < argc; i++)
        x->x_a[i] = argv[i];
    x->x_change = 0;
    inlet_new((t_object *)x, (t_pd *)x, &s_anything, gensym("set"));
    outlet_new(&x->x_obj, &s_anything);
    x->x_bang_out = outlet_new(&x->x_obj, &s_bang);
    return (x);
}

void changed_setup(void){
    changed_class = class_new(gensym("changed"), (t_newmethod)changed_new, 0,
    	sizeof(t_changed), 0, A_GIMME, 0);
    class_addbang(changed_class, changed_bang);
    class_addanything(changed_class, changed_anything);
    class_addmethod(changed_class, (t_method)changed_set, gensym("set"), A_GIMME, 0);
}
