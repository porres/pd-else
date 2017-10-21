// porres 2017

#define A_LIST 4096;
#define A_ANYTHING 4096;

#include "m_pd.h"

static t_class *changed_class;

typedef struct _changed
{
    t_object x_obj;
    t_atom   x_a[256];
	int      x_c;
	int	x_type;
} t_changed;

static void changed_list(t_changed *x, t_symbol *s, int argc, t_atom *argv){
    int i, change = 0;
    int c = argc;
    if(c == x->x_c)	// same number of elements
        for (i = 0; i < c; i++){
            //	if(x->x_a[i].a_type != argv[i].a_type)
            //	{
            //		change = 1;
            //		break;
            //	}
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
        change = 1;	// different number of elems.
    if (change){
        x->x_c = c;
        for (i = 0; i < c; i++){	// same new list
            x->x_a[i] = argv[i];
        }
        outlet_list(x->x_obj.ob_outlet, s, argc, argv);
    }
}

/*
 
 static void changed_bang(t_changed *x){
 if (x->x_type == A_FLOAT)
 outlet_float(x->x_obj.ob_outlet, x->x_a->a_w.w_float);
 else if (x->x_type == A_SYMBOL)
 outlet_symbol(x->x_obj.ob_outlet, x->x_a->a_w.w_symbol);
 else
 outlet_list(x->x_obj.ob_outlet, NULL, x->x_c, x->x_a);
 }
 
 static void changed_float(t_changed *x, t_float f){
 if (f != x->x_a->a_w.w_float){
 SETFLOAT(x->x_a, f);
 outlet_float(x->x_obj.ob_outlet, x->x_a->a_w.w_float);
 }
 }

 
static void changed_symbol(t_changed *x, t_symbol *s)
{
	if (x->x_type == A_SYMBOL)
	{
		if (s != x->x_a->a_w.w_symbol)
		{
    		// x->x_s = s;
			SETSYMBOL(x->x_a, s);
			outlet_symbol(x->x_obj.ob_outlet, x->x_a->a_w.w_symbol);
		}
	}
}

static void changed_set(t_changed *x, t_symbol *s, int argc, t_atom *argv)
{
	int i;

	if (x->x_type == A_SYMBOL)
	{
		SETSYMBOL(x->x_a, argv->a_w.w_symbol);
	}
 	else if (x->x_type == A_FLOAT)
	{
		SETFLOAT(x->x_a, argv->a_w.w_float);
	}
	else	// list or anything
    {
		x->x_c = argc;
		for (i = 0; i < argc; i++)
		{
			x->x_a[i] = argv[i];
		}
    }
}
*/

static void *changed_new(t_symbol *s){
    int i;
    t_changed *x = (t_changed *)pd_new(changed_class);
    outlet_new(&x->x_obj, &s_anything);
    return (x);
}

void changed_setup(void){
    changed_class = class_new(gensym("changed"), (t_newmethod)changed_new, 0,
    	sizeof(t_changed), 0, 0);
//    class_addbang(changed_class, changed_bang);
//    class_addfloat(changed_class, changed_float);
//    class_addsymbol(changed_class, changed_symbol);
//    class_addlist(changed_class, changed_list);
    class_addanything(changed_class, changed_list);
//    class_addmethod(changed_class, (t_method)changed_set, gensym("set"), A_GIMME, 0);
}
