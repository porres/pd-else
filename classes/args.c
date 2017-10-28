// porres 2017

#include "m_pd.h"

static t_class *args_class;

typedef struct _args{
  t_object 	x_obj;
  int       x_ac;
  t_atom*   x_av;
} t_args;

static void args_bang(t_args *x){
    outlet_anything(x->x_obj.ob_outlet, &s_list, x->x_ac, x->x_av);
}

static void copy_atoms(t_atom *av_src, t_atom *av_dst, int ac_n){
  while(ac_n--)
    *av_dst++ = *av_src++;
}

static void *args_new(t_symbol *selector, int argc, t_atom* argv){
  t_args *x = (t_args *)pd_new(args_class);
  int ac;
  t_atom* av;
  canvas_getargs(&ac, &av);
   x->x_ac = ac;
   x->x_av = getbytes(x->x_ac * sizeof(*(x->x_av)));
   copy_atoms(av, x->x_av, x->x_ac); // source - destination - n
  outlet_new(&x->x_obj, &s_anything);
  return (x);
}

void args_setup(void) {
  args_class = class_new(gensym("args"), (t_newmethod)args_new, 0,
    sizeof(t_args), 0, A_GIMME, 0);
  class_addbang(args_class, args_bang);
}
