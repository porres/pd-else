// Porres 2016
 
#include "m_pd.h"
#include <math.h>

static t_class *cents2ratio_class;

typedef struct _cents2ratio
{
  t_object x_obj;
  t_outlet *float_outlet;
  t_int bytes;
  t_atom *output_list;
  t_float f;
} t_cents2ratio;


void *cents2ratio_new(t_floatarg f1, t_floatarg f2);
void cents2ratio_free(t_cents2ratio *x);
void cents2ratio_float(t_cents2ratio *x, t_floatarg f);
void cents2ratio_bang(t_cents2ratio *x);
void cents2ratio_set(t_cents2ratio *x, t_floatarg f);

static t_float convert(t_float f);

void cents2ratio_float(t_cents2ratio *x, t_floatarg f)
{
  x->f = f;
  outlet_float(x->float_outlet, convert(f));
}

t_float convert(t_float f)
{
  return pow(2, (f/1200));
    
}

void cents2ratio_list(t_cents2ratio *x, t_symbol *s, int argc, t_atom *argv)
{
  int old_bytes = x->bytes, i = 0;
  x->bytes = argc*sizeof(t_atom);
  x->output_list = (t_atom *)t_resizebytes(x->output_list,old_bytes,x->bytes);
  for(i=0;i<argc;i++)
    SETFLOAT(x->output_list+i,convert(atom_getfloatarg(i,argc,argv)));
  outlet_list(x->float_outlet,0,argc,x->output_list);
}

void cents2ratio_set(t_cents2ratio *x, t_float f)
{
  x->f = f;
}

void cents2ratio_bang(t_cents2ratio *x)
{
  outlet_float(x->float_outlet,convert(x->f));
}


void *cents2ratio_new(t_floatarg f1, t_floatarg f2)
{
  t_cents2ratio *x = (t_cents2ratio *) pd_new(cents2ratio_class);
  x->f = f1;
  int init = (int)f2;
  x->float_outlet = outlet_new(&x->x_obj, 0);
  x->bytes = sizeof(t_atom);
  x->output_list = (t_atom *)getbytes(x->bytes);
  if(x->output_list==NULL) {
    pd_error(x,"cents2ratio: memory allocation failure");
    return NULL;
  }
  return (x);
    
    if(init != 0)
        outlet_float(x->float_outlet,convert(x->f));
}

void cents2ratio_free(t_cents2ratio *x)
{
  t_freebytes(x->output_list,x->bytes);
}

void cents2ratio_setup(void)
{
  cents2ratio_class = class_new(gensym("cents2ratio"), (t_newmethod)cents2ratio_new,
			  (t_method)cents2ratio_free,sizeof(t_cents2ratio),0, A_DEFFLOAT, A_DEFFLOAT, 0);

  class_addfloat(cents2ratio_class,(t_method)cents2ratio_float);
  class_addlist(cents2ratio_class,(t_method)cents2ratio_list);
  class_addmethod(cents2ratio_class,(t_method)cents2ratio_set,gensym("set"),A_DEFFLOAT,0);
  class_addbang(cents2ratio_class,(t_method)cents2ratio_bang);
}
