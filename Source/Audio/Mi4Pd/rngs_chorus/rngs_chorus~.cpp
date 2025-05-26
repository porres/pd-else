#include "m_pd.h"

//IMPROVE - inlets
//IMPROVE - variable  buffer size


#include "rings/dsp/fx/chorus.h"

static t_class *rngs_chorus_tilde_class;

typedef struct _rngs_chorus_tilde {
  t_object  x_obj;

  rings::Chorus fx;
  uint16_t fx_buffer[32768];  

  t_float f_dummy;
  t_float f_amount;
  t_float f_depth;

  // CLASS_MAINSIGNALIN  = in_left;
  t_inlet*  x_in_right;
  t_inlet*  x_in_amount;
  t_inlet*  x_in_depth;
  t_outlet* x_out_left;
  t_outlet* x_out_right;
} t_rngs_chorus_tilde;


//define pure data methods
extern "C"  {
  t_int*  rngs_chorus_tilde_render(t_int *w);
  void    rngs_chorus_tilde_dsp(t_rngs_chorus_tilde *x, t_signal **sp);
  void    rngs_chorus_tilde_free(t_rngs_chorus_tilde *x);
  void*   rngs_chorus_tilde_new(t_floatarg f);
  void    rngs_chorus_tilde_setup(void);

  void rngs_chorus_tilde_amount(t_rngs_chorus_tilde *x, t_floatarg f);
  void rngs_chorus_tilde_depth(t_rngs_chorus_tilde *x, t_floatarg f);
}

// puredata methods implementation -start
t_int* rngs_chorus_tilde_render(t_int *w)
{
  t_rngs_chorus_tilde *x = (t_rngs_chorus_tilde *)(w[1]);
  t_sample  *in_left  =    (t_sample *)(w[2]);
  t_sample  *in_right =    (t_sample *)(w[3]);
  t_sample  *out_left =    (t_sample *)(w[4]);
  t_sample  *out_right =   (t_sample *)(w[5]);
  int n =  (int)(w[6]);

  x->fx.set_amount(x->f_amount);
  x->fx.set_depth(x->f_depth);

  for (int i = 0; i < n; i++) {
    out_left[i] = in_left[i];
    out_right[i] = in_right[i];
  }

  x->fx.Process(out_left,out_right, n);

  return (w + 7); // # args + 1
}

void rngs_chorus_tilde_dsp(t_rngs_chorus_tilde *x, t_signal **sp)
{
  // add the perform method, with all signal i/o
  dsp_add(rngs_chorus_tilde_render, 6,
          x,
          sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, // signal i/o (clockwise)
          sp[0]->s_n);
}

void rngs_chorus_tilde_free(t_rngs_chorus_tilde *x)
{
  inlet_free(x->x_in_right);
  inlet_free(x->x_in_amount);
  inlet_free(x->x_in_depth);
  outlet_free(x->x_out_left);
  outlet_free(x->x_out_right);
}

void *rngs_chorus_tilde_new(t_floatarg f)
{
  t_rngs_chorus_tilde *x = (t_rngs_chorus_tilde *) pd_new(rngs_chorus_tilde_class);

  x->f_amount = f;
  x->f_depth= 0.5;

  x->x_in_right   = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
  x->x_in_amount  = floatinlet_new (&x->x_obj, &x->f_amount);
  x->x_in_depth  = floatinlet_new (&x->x_obj, &x->f_depth);
  x->x_out_left   = outlet_new(&x->x_obj, &s_signal);
  x->x_out_right  = outlet_new(&x->x_obj, &s_signal);

  x->fx.Init(x->fx_buffer);
  return (void *)x;
}

void rngs_chorus_tilde_amount(t_rngs_chorus_tilde *x, t_floatarg f)
{
  x->f_amount = f;
}
void rngs_chorus_tilde_depth(t_rngs_chorus_tilde *x, t_floatarg f)
{
  x->f_depth = f;
}

void rngs_chorus_tilde_setup(void) {
  rngs_chorus_tilde_class = class_new(gensym("rngs_chorus~"),
                                         (t_newmethod)rngs_chorus_tilde_new,
                                         (t_method) rngs_chorus_tilde_free, 
                                         sizeof(t_rngs_chorus_tilde),
                                         CLASS_DEFAULT,
                                         A_DEFFLOAT, A_NULL);

  class_addmethod(  rngs_chorus_tilde_class,
                    (t_method)rngs_chorus_tilde_dsp,
                    gensym("dsp"), A_NULL);
  CLASS_MAINSIGNALIN(rngs_chorus_tilde_class, t_rngs_chorus_tilde, f_dummy);


  class_addmethod(rngs_chorus_tilde_class,
                  (t_method) rngs_chorus_tilde_amount, gensym("amount"),
                  A_DEFFLOAT, A_NULL);
  class_addmethod(rngs_chorus_tilde_class,
                  (t_method) rngs_chorus_tilde_depth, gensym("depth"),
                  A_DEFFLOAT, A_NULL);
}
// puredata methods implementation - end

