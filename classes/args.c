#include "m_pd.h"

#define IS_A_SYMBOL(atom,index) ((atom+index)->a_type == A_SYMBOL)
#define IS_A_FLOAT(atom,index) ((atom+index)->a_type == A_FLOAT)

static t_class *args_class;

typedef struct _args{
  t_object                  	x_obj;
  t_outlet*			       	 	outlet;
  t_canvas*						canvas;
  int                           size;
  int                           ac;
  t_atom*                       av;
} t_args;

// Prepends the proper selector
static void args_output(t_outlet* o, int argc, t_atom* argv) {
		if (!argc) outlet_bang(o);
		else if (IS_A_SYMBOL(argv,0)) outlet_anything(o,atom_getsymbol(argv),argc-1,argv+1);
		else if (IS_A_FLOAT(argv,0) && argc==1) outlet_float(o,atom_getfloat(argv));
		else outlet_anything(o,&s_list,argc,argv);
}

// Dump args
static void args_bang(t_args *x){
    args_output(x->outlet, x->ac, x->av);
}

static void args_free(t_args *x){
    freebytes(x->av,x->ac*sizeof(*(x->av)));
}

static void copy_atoms(t_atom *src, t_atom *dst, int n){
  while(n--)
    *dst++ = *src++;
}

static void *args_new(t_symbol *selector, int argc, t_atom* argv) {
  t_args *x = (t_args *)pd_new(args_class);
  int ac;
  t_atom* av;
  canvas_getargs(&ac, &av);
   x->ac = ac;
   x->av = getbytes(x->ac * sizeof(*(x->av)));
   copy_atoms(av, x->av, x->ac);
   x->outlet = outlet_new(&x->x_obj, &s_anything);
  return (x);
}

void args_setup(void) {
  args_class = class_new(gensym("args"), (t_newmethod)args_new, (t_method)args_free,
    sizeof(t_args), 0, A_GIMME, 0);
  class_addbang(args_class, args_bang);
}
