// porres 2017

#include "m_pd.h"

#define IS_A_FLOAT(atom,index) ((atom+index)->a_type == A_FLOAT)
#define IS_A_SYMBOL(atom,index) ((atom+index)->a_type == A_SYMBOL)

static t_class *changed_class;

typedef struct _changed
{
    t_object x_obj;
    t_atom   x_a[4096];
	int      x_c;
    t_symbol *x_sym;
} t_changed;

static void changed_bang(t_changed *x){
    outlet_anything(x->x_obj.ob_outlet, x->x_sym, x->x_c, x->x_a);
}

static void changed_anything(t_changed *x, t_symbol *s, int argc, t_atom *argv){
    int i, change = 0;
    int c = argc;
    x->x_sym = s;
    if(c == x->x_c)	// same number of elements
        for (i = 0; i < c; i++){
            if (x->x_a[i].a_type == A_FLOAT){
                if (argv[i].a_type != A_FLOAT || x->x_a[i].a_w.w_float != argv[i].a_w.w_float){
                    change = 1;
                    break;
                }
            }
            else if (x->x_a[i].a_type == A_SYMBOL){
                if (argv[i].a_type != A_SYMBOL || x->x_a[i].a_w.w_symbol != argv[i].a_w.w_symbol){
                    change = 1;
                    break;
                }
            }
        }
    else
        change = 1;	// different number of elements
    if (change){
        x->x_c = c;
        for (i = 0; i < c; i++){	// same new list
            x->x_a[i] = argv[i];
        }
        outlet_anything(x->x_obj.ob_outlet, s, argc, argv);
    }
}

static void changed_set(t_changed *x, t_symbol *s, int argc, t_atom *argv){
    int i;
//    x->x_sym = s;
    x->x_c = argc;
    for (i = 0; i < argc; i++)
        x->x_a[i] = argv[i];
}

static void *changed_new(t_symbol *s, int argc, t_atom *argv){
    t_changed *x = (t_changed *)pd_new(changed_class);
    int i;
//    x->x_sym = s;
    //////////////////
    if(argc == 0){
        x->x_sym = &s_bang;
//        x->x_a[0] = (t_atom *)getbytes(x->x_n * sizeof(t_atom));
    }
    else if(argc == 1){
        if(IS_A_SYMBOL(argv, 0)){
            x->x_sym = atom_getsymbol(argv);
//            x->x_at = (t_atom *)getbytes(x->x_n * sizeof(t_atom));
        }
        else{
            if(IS_A_FLOAT(argv, 0)){
            x->x_sym = &s_float;
            }
        }
    }
    else{
        if(IS_A_SYMBOL(argv, 0)){
            x->x_sym = atom_getsymbol(argv++);
            argc--;
        }
        else{
            x->x_sym = &s_list;
        }
    }
/////////////////////////////////////////////
    x->x_c = argc;
    for (i = 0; i < argc; i++)
        x->x_a[i] = argv[i];
    outlet_new(&x->x_obj, &s_anything);
    return (x);
}

void changed_setup(void){
    changed_class = class_new(gensym("changed"), (t_newmethod)changed_new, 0,
    	sizeof(t_changed), 0, A_GIMME, 0);
    class_addbang(changed_class, changed_bang);
//    class_addlist(changed_class, changed_list);
    class_addanything(changed_class, changed_anything);
    class_addmethod(changed_class, (t_method)changed_set, gensym("set"), A_GIMME, 0);
}
