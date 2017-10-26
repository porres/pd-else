// porres 2017

#include "m_pd.h"

static t_class *break_class;

typedef struct _break{
  t_object     	x_obj;
  int 			x_break;
  char			x_separator;
}t_break;

static void break_anything(t_break *x, t_symbol *s, int argc, t_atom *argv){
    if (x->x_break){
        int i = 0, first = 1, ac_break;
        while(i < argc){
            int j;
            for (j = i + 1; j < argc; j++)
                if ((argv+j)->a_type == A_SYMBOL && x->x_separator == (atom_getsymbol(argv+j))->s_name[0])
                    break;
            ac_break = j - i;
            if(first){
                outlet_anything(x->x_obj.ob_outlet, s, ac_break, argv + i);
                first = 0;
            }
            else
                outlet_anything(x->x_obj.ob_outlet, atom_getsymbol(argv + i), ac_break - 1, argv + i + 1);
            i = j;
        }
    }
    else
        outlet_anything(x->x_obj.ob_outlet, s, argc, argv);
}

static void *break_new(t_symbol *selector, int argc, t_atom* argv){
  t_break *x = (t_break *)pd_new(break_class);
  if(argc && ((argv)->a_type == A_SYMBOL)){
        x->x_separator = atom_getsymbol(argv)->s_name[0];
        x->x_break = 1;
        }
  outlet_new(&x->x_obj, &s_anything);
  return (x);
}

void break_setup(void){
  break_class = class_new(gensym("break"), (t_newmethod)break_new, 0, sizeof(t_break), 0, A_GIMME, 0);
  class_addanything(break_class, break_anything);
}
