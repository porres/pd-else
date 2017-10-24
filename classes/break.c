
#include "m_pd.h"

static t_class *break_class;

typedef struct _break{
  t_object                  	x_obj;
  t_outlet*			       	 	outlet;
  int 							x_break;
  char							x_separator;
} t_break;

static void break_output(t_outlet* o, int argc, t_atom* argv) { // Prepends the proper selector
		if (!argc)
            outlet_bang(o);
		else if ((argv)->a_type == A_SYMBOL)
            outlet_anything(o, atom_getsymbol(argv), argc-1, argv+1);
		else if (((argv)->a_type == A_FLOAT) && argc == 1)
            outlet_float(o, atom_getfloat(argv));
		else
            outlet_anything(o, &s_list, argc, argv);
}

static int next_separator(char tag, int ac, t_atom *av, int* ac_a, t_atom ** av_a, int* iter){
	int i;
    if (ac == 0 || *iter >= ac){
		*ac_a = 0;
		*av_a = NULL;
		return 0;
	}
    for (i = *iter + 1; i < ac; i++){
        if ((av+i)->a_type == A_SYMBOL && (atom_getsymbol(av+i))->s_name[0] == tag)
            break;
     }
	 *ac_a = i - *iter;
	 *av_a = av + *iter;
	 *iter = i;
     return (*ac_a);     
}

static void break_anything(t_break *x, t_symbol *s, int argc, t_atom *argv){
    if (x->x_break){
        int ac_a;
        t_atom* av_a;
        t_symbol* selector_a;
        int iter = 0;
        while (next_separator(x->x_separator, argc, argv, &ac_a, &av_a, &iter))
            break_output(x->outlet, ac_a, av_a);
    }
    else //x->x_break = ALL
        break_output(x->outlet, argc, argv);
}

static void *break_new(t_symbol *selector, int argc, t_atom* argv) {
  t_break *x = (t_break *)pd_new(break_class);
  x->x_break = 0;
  if(argc && ((argv)->a_type == A_SYMBOL)){
        t_symbol* s = atom_getsymbol(argv);
        x->x_separator = s->s_name[0];
        x->x_break = 1;
        }
   x->outlet = outlet_new(&x->x_obj, &s_anything);
  return (x);
}

void break_setup(void) {
  break_class = class_new(gensym("break"), (t_newmethod)break_new, 0,
                          sizeof(t_break), 0, A_GIMME, 0);
  class_addanything(break_class, break_anything);
}
