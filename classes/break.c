
#include "m_pd.h"

static t_class *break_class;

typedef struct _break{
  t_object     	x_obj;
  int 			x_break;
  char			x_separator;
} t_break;

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
        int ac_a, iter = 0;
        t_atom* av_a;
        while (next_separator(x->x_separator, argc, argv, &ac_a, &av_a, &iter)){
            if (ac_a == 0)
                outlet_bang(x->x_obj.ob_outlet);
            else if ((av_a)->a_type == A_SYMBOL)
                outlet_anything(x->x_obj.ob_outlet, atom_getsymbol(av_a), ac_a - 1, av_a + 1);
            else if (((av_a)->a_type == A_FLOAT) && ac_a == 1)
                outlet_float(x->x_obj.ob_outlet, atom_getfloat(av_a));
            else
                outlet_anything(x->x_obj.ob_outlet, s, ac_a, av_a);
        }
    }
    else
        outlet_anything(x->x_obj.ob_outlet, s, argc, argv);
}

static void *break_new(t_symbol *selector, int argc, t_atom* argv) {
  t_break *x = (t_break *)pd_new(break_class);
  x->x_break = 0;
  if(argc && ((argv)->a_type == A_SYMBOL)){
        t_symbol* s = atom_getsymbol(argv);
        x->x_separator = s->s_name[0];
        x->x_break = 1;
        }
   outlet_new(&x->x_obj, &s_anything);
  return (x);
}

void break_setup(void) {
  break_class = class_new(gensym("break"), (t_newmethod)break_new, 0,
                          sizeof(t_break), 0, A_GIMME, 0);
  class_addanything(break_class, break_anything);
}
