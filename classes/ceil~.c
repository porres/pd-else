// Porres 2017

#include "m_pd.h"
#include <math.h>

static t_class *ceil_class;

typedef struct _ceil {
  t_object x_obj;
  t_inlet *x_inlet;
  t_outlet *x_outlet;
} t_ceil;

void *ceil_new(void);
static t_int * ceil_perform(t_int *w);
static void ceil_dsp(t_ceil *x, t_signal **sp);

static t_int * ceil_perform(t_int *w)
{
  t_ceil *x = (t_ceil *)(w[1]);
  int n = (int)(w[2]);
  t_float *in = (t_float *)(w[3]);
  t_float *out = (t_float *)(w[4]);
  while(n--)
      *out++ = ceil(*in++);
  return (w + 5);
}

static void ceil_dsp(t_ceil *x, t_signal **sp)
{
  dsp_add(ceil_perform, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
}

void *ceil_new(void)
{
  t_ceil *x = (t_ceil *)pd_new(ceil_class); 
  x->x_inlet = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
  x->x_outlet = outlet_new(&x->x_obj, &s_signal);
  return (void *)x;
}

void ceil_tilde_setup(void) {
  ceil_class = class_new(gensym("ceil~"),
    (t_newmethod) ceil_new, 0, sizeof (t_ceil), CLASS_NOINLET, 0);
  class_addmethod(ceil_class, (t_method) ceil_dsp, gensym("dsp"), 0);
}
