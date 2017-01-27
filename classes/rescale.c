// porres 2017

#include <string.h>

#include "m_pd.h"
#include <math.h>

static t_class *rescale_class;

typedef struct _rescale
{
  t_object obj; /* object header */
  t_float in; /* stored input value */
  t_outlet *float_outlet;
  t_float minin;
  t_float maxin;
  t_float minout;
  t_float maxout;
  t_float expo;
  t_atom *output_list; /* for list output */
  t_int a_bytes;
  t_int flag;
  t_int ac;
} t_rescale;

void *rescale_new(t_symbol *s, int argc, t_atom *argv);
void rescale_ft(t_rescale *x, t_floatarg f);
void rescale_bang(t_rescale *x);
void rescale_list(t_rescale *x, t_symbol *s, int argc, t_atom *argv);
void rescale_free(t_rescale *x);
void rescale_factor(t_rescale *x, t_floatarg f);
void rescale_classic(t_rescale *x, t_floatarg f);
void rescale_set(t_rescale *x, t_floatarg f);

t_float scaling(t_rescale *x, t_float f);
t_float exp_scaling(t_rescale *x, t_float f);
t_float (*ptrtoscaling)(t_rescale *x,t_float f);
void check(t_rescale *x);

void *rescale_new(t_symbol *s, int argc, t_atom *argv)
{
  t_rescale *x = (t_rescale *)pd_new(rescale_class);
  x->float_outlet = outlet_new(&x->obj, 0);
  floatinlet_new(&x->obj,&x->minout);
  floatinlet_new(&x->obj,&x->maxout);
  x->minin = 0;
  x->maxin = 127;
  x->minout = 0;
  x->maxout = 1;
  x->flag = 0;
  x->expo = 1.f;
  t_int numargs = 0;
  t_float tmp = -1.f;

  while(argc>0) {
    t_symbol *firstarg = atom_getsymbolarg(0,argc,argv);
    if(firstarg==&s_){
      switch(numargs) {
      case 0:
	x->minout = atom_getfloatarg(0,argc,argv);
	numargs++;
	argc--;
	argv++;
	break;
      case 1:
	x->maxout = atom_getfloatarg(0,argc,argv);
	numargs++;
	argc--;
	argv++;
	break;
      case 2:
	tmp = atom_getfloatarg(0,argc,argv);
	numargs++;
	argc--;
	argv++;
	break;
      default:
	argc--;
	argv++;
      }
    }
  }


  if(tmp!=-1) {
    x->expo = ((tmp<0.f) ? 0.f : tmp);
  }
  
  x->ac = 1;
  x->a_bytes = x->ac*sizeof(t_atom);
  x->output_list = (t_atom *)getbytes(x->a_bytes);
  if(x->output_list==NULL) {
    pd_error(x,"rescale: memory allocation failure");
    return NULL;
  }
  
  return (x);
}

void rescale_setup(void)
{
  t_class *c;
  rescale_class = class_new(gensym("rescale"), (t_newmethod)rescale_new,
			  (t_method)rescale_free,sizeof(t_rescale),0,A_GIMME,0);
  c = rescale_class;
  class_addfloat(c,(t_method)rescale_ft);
  class_addbang(c,(t_method)rescale_bang);
  class_addlist(c,(t_method)rescale_list);
  class_addmethod(c,(t_method)rescale_factor, gensym("factor"), A_DEFFLOAT, 0);
  class_addmethod(c,(t_method)rescale_set, gensym("set"), A_DEFFLOAT, 0);
}


void rescale_ft(t_rescale *x, t_floatarg f)
{
  if(f < 0) f = 0;
  if(f > 127) f = 127;
  x->in = f;
  check(x);
  t_float temp = ptrtoscaling(x, f);
  SETFLOAT(x->output_list, temp);
  outlet_list(x->float_outlet, 0, x->ac, x->output_list);
  return;
}

t_float scaling(t_rescale *x, t_float f)
{
  f = (x->maxout - x->minout)*(f-x->minin)/(x->maxin-x->minin) + x->minout;
  f < x->minout ? x->minout : f > x->maxout ? x->maxout : f;
  return f;
}

t_float exp_scaling(t_rescale *x, t_float f)
{
  f = ((f-x->minin)/(x->maxin-x->minin) == 0) 
    ? x->minout : (((f-x->minin)/(x->maxin-x->minin)) > 0) 
    ? (x->minout + (x->maxout-x->minout) * pow((f-x->minin)/(x->maxin-x->minin),x->expo)) 
    : ( x->minout + (x->maxout-x->minout) * -(pow(((-f+x->minin)/(x->maxin-x->minin)),x->expo)));
    f < x->minout ? x->minout : f > x->maxout ? x->maxout : f;
  return f;
}

void rescale_bang(t_rescale *x)
{
  rescale_ft(x,x->in);
  return;
}

void rescale_list(t_rescale *x, t_symbol *s, int argc, t_atom *argv)
{
  int i = 0;
  int old_a = x->a_bytes;
  x->ac = argc;
  x->a_bytes = argc*sizeof(t_atom);
  x->output_list = (t_atom *)t_resizebytes(x->output_list,old_a,x->a_bytes);
  check(x);
    x->in = atom_getfloatarg(0, argc, argv);
    x->in = x->in < 0 ? 0 : x->in > 127 ? 127 : x->in;
  for(i=0;i<argc;i++)
    SETFLOAT(x->output_list+i, ptrtoscaling(x, atom_getfloatarg(i, argc, argv)));
  outlet_list(x->float_outlet,0,argc,x->output_list);
  return;
}

void rescale_set(t_rescale *x, t_float f)
{
    x->in = f;
    if(x->in < 0) x->in = 0;
    if(x->in > 127) x->in = 127;
}

void rescale_factor(t_rescale *x, t_floatarg f)
{
  x->expo = f < 0. ? 0. : f;
  return;
}

void check(t_rescale *x)
{
  if(x->expo==1)
    ptrtoscaling = scaling;
  else ptrtoscaling = exp_scaling;
  return;
}

void rescale_free(t_rescale *x)
{
  t_freebytes(x->output_list,x->a_bytes);
}
