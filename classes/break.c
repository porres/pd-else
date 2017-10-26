// porres 2017

#include "m_pd.h"

static t_class *break_class;

typedef struct _break{
  t_object     	x_obj;
  int 			x_break;
  char			x_separator;
}t_break;

static void break_anything(t_break *x, t_symbol *s, int ac, t_atom *av){
    if (x->x_break){
        int i = 0, first = 1, ac_break;
        while(i < ac){
            int j = i + 1; // j = i
            for (j; j < ac; j++) // i++; i < ac; i++
                if ((av+j)->a_type == A_SYMBOL && x->x_separator == (atom_getsymbol(av+j))->s_name[0])
                    break;
            ac_break = j - i; // i - j
            if(first){
                outlet_anything(x->x_obj.ob_outlet, s, ac_break, av + i);
                first = 0;
            }
            else
                outlet_anything(x->x_obj.ob_outlet, atom_getsymbol(av + i), ac_break - 1, av + i + 1);
            i = j;
        }
    }
    else
        outlet_anything(x->x_obj.ob_outlet, s, ac, av);
}

static void *break_new(t_symbol *selector, int ac, t_atom* av){
  t_break *x = (t_break *)pd_new(break_class);
  if(ac && ((av)->a_type == A_SYMBOL)){
        x->x_separator = atom_getsymbol(av)->s_name[0];
        x->x_break = 1;
        }
  outlet_new(&x->x_obj, &s_anything);
  return (x);
}

void break_setup(void){
  break_class = class_new(gensym("break"), (t_newmethod)break_new, 0, sizeof(t_break), 0, A_GIMME, 0);
  class_addanything(break_class, break_anything);
}
