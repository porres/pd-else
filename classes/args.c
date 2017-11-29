// porres 2017

#include "m_pd.h"

static t_class *args_class;

typedef struct _args{
    t_object x_obj;
    int      x_ac;
    t_atom*  x_av;
} t_args;

static void args_bang(t_args *x){
    outlet_anything(x->x_obj.ob_outlet, &s_list, x->x_ac, x->x_av);
}

static void *args_new(void){
  t_args *x = (t_args *)pd_new(args_class);
  canvas_getargs(&x->x_ac, &x->x_av);
  outlet_new(&x->x_obj, &s_anything);
  return (x);
}

void args_setup(void){
  args_class = class_new(gensym("args"), (t_newmethod)args_new, 0, sizeof(t_args), 0, 0);
  class_addbang(args_class, args_bang);
}
