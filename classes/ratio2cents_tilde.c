// Porres 2016

#include "m_pd.h"
#include <math.h>

static t_class *ratio2cents_class;

typedef struct _ratio2cents {
  t_object x_obj;
  t_inlet *x_inlet;
  t_outlet *x_outlet;
} t_ratio2cents;

void *ratio2cents_new(void);
static t_int * ratio2cents_perform(t_int *w);
static void ratio2cents_dsp(t_ratio2cents *x, t_signal **sp);

static t_int * ratio2cents_perform(t_int *w)
{
  t_ratio2cents *x = (t_ratio2cents *)(w[1]); // ???
  int n = (int)(w[2]);
  t_float *in = (t_float *)(w[3]);
  t_float *out = (t_float *)(w[4]);
    while(n--){
        float f = *in++;
        if(f <= 0.f)
            f = 1.f;
        *out++ = log2(f) * 1200;
    }
    
  return (w + 5);
}

static void ratio2cents_dsp(t_ratio2cents *x, t_signal **sp)
{
  dsp_add(ratio2cents_perform, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
}

void *ratio2cents_new(void)
{
  t_ratio2cents *x = (t_ratio2cents *)pd_new(ratio2cents_class); 
  x->x_inlet = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
  x->x_outlet = outlet_new(&x->x_obj, &s_signal);
  return (void *)x;
}

void ratio2cents_tilde_setup(void) {
  ratio2cents_class = class_new(gensym("ratio2cents~"),
    (t_newmethod) ratio2cents_new, 0, sizeof (t_ratio2cents), CLASS_NOINLET, 0);
  class_addmethod(ratio2cents_class, (t_method) ratio2cents_dsp, gensym("dsp"), 0);
}