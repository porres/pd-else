#include "m_pd.h"

//IMPROVE - inlets
//IMPROVE - variable reverb buffer size


#include "clouds/dsp/frame.h"
#include "clouds/dsp/fx/reverb.h"

static t_class *clds_reverb_tilde_class;

typedef struct _clds_reverb_tilde {
  t_object  x_obj;

  t_float  f_dummy;

  t_float  f_amount;
  t_float  f_input_gain;
  t_float  f_time;
  t_float  f_diffusion;
  t_float  f_lp;

  // CLASS_MAINSIGNALIN  = in_left;
  t_inlet*  x_in_right;
  t_inlet*  x_in_amount;
  t_outlet* x_out_left;
  t_outlet* x_out_right;

  // clouds:reverb
  clouds::Reverb fx;
  uint16_t fx_buffer[32768];
  clouds::FloatFrame* iobuf;
  int iobufsz;
} t_clds_reverb_tilde;


//define pure data methods
extern "C"  {
  t_int* clds_reverb_tilde_render(t_int *w);
  void clds_reverb_tilde_dsp(t_clds_reverb_tilde *x, t_signal **sp);
  void clds_reverb_tilde_free(t_clds_reverb_tilde *x);
  void* clds_reverb_tilde_new(t_floatarg f);
  void clds_reverb_tilde_setup(void);
  void clds_reverb_tilde_amount(t_clds_reverb_tilde *x, t_floatarg f);
  void clds_reverb_tilde_gain(t_clds_reverb_tilde *x, t_floatarg f);
  void clds_reverb_tilde_diffusion(t_clds_reverb_tilde *x, t_floatarg f);
  void clds_reverb_tilde_time(t_clds_reverb_tilde *x, t_floatarg f);
  void clds_reverb_tilde_lp(t_clds_reverb_tilde *x, t_floatarg f);
}

// puredata methods implementation -start
t_int* clds_reverb_tilde_render(t_int *w)
{
  t_clds_reverb_tilde *x = (t_clds_reverb_tilde *)(w[1]);
  t_sample  *in_left  =    (t_sample *)(w[2]);
  t_sample  *in_right =    (t_sample *)(w[3]);
  t_sample  *out_left =    (t_sample *)(w[4]);
  t_sample  *out_right =   (t_sample *)(w[5]);
  int n =  (int)(w[6]);

  if (n > x->iobufsz) {
    delete [] x->iobuf;
    x->iobuf = new clouds::FloatFrame[n];
    x->iobufsz = n;
  }

  x->fx.set_amount(x->f_amount);
  x->fx.set_input_gain(x->f_input_gain);
  x->fx.set_time(x->f_time);
  x->fx.set_diffusion(x->f_diffusion);
  x->fx.set_lp(x->f_lp);

  for (int i = 0; i < n; i++) {
    x->iobuf[i].l = in_left[i];
    x->iobuf[i].r = in_right[i];
  }

  x->fx.Process(x->iobuf, n);

  for (int i = 0; i < n; i++) {
    out_left[i] = x->iobuf[i].l;
    out_right[i] = x->iobuf[i].r;
  }

  return (w + 7); // # args + 1
}

void clds_reverb_tilde_dsp(t_clds_reverb_tilde *x, t_signal **sp)
{
  // add the perform method, with all signal i/o
  dsp_add(clds_reverb_tilde_render, 6,
          x,
          sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, // signal i/o (clockwise)
          sp[0]->s_n);
}

void clds_reverb_tilde_free(t_clds_reverb_tilde *x)
{
  delete [] x->iobuf;

  inlet_free(x->x_in_right);
  inlet_free(x->x_in_amount);
  outlet_free(x->x_out_left);
  outlet_free(x->x_out_right);
}

void *clds_reverb_tilde_new(t_floatarg f)
{
  t_clds_reverb_tilde *x = (t_clds_reverb_tilde *) pd_new(clds_reverb_tilde_class);
  x->iobuf = new clouds::FloatFrame[64];
  x->iobufsz = 64;

  x->f_amount = f;
  x->f_input_gain = 0.75;
  x->f_time = 0.25;
  x->f_diffusion = 0.25;
  x->f_lp = 0.75;

  x->x_in_right   = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
  x->x_in_amount  = floatinlet_new (&x->x_obj, &x->f_amount);
  x->x_out_left   = outlet_new(&x->x_obj, &s_signal);
  x->x_out_right  = outlet_new(&x->x_obj, &s_signal);

  x->fx.Init(x->fx_buffer,44100.0f);
  return (void *)x;
}

void clds_reverb_tilde_amount(t_clds_reverb_tilde *x, t_floatarg f)
{
  x->f_amount = f;
}
void clds_reverb_tilde_gain(t_clds_reverb_tilde *x, t_floatarg f)
{
  x->f_input_gain = f;
}
void clds_reverb_tilde_diffusion(t_clds_reverb_tilde *x, t_floatarg f)
{
  x->f_diffusion = f;
}
void clds_reverb_tilde_time(t_clds_reverb_tilde *x, t_floatarg f)
{
  x->f_time = f;
}
void clds_reverb_tilde_lp(t_clds_reverb_tilde *x, t_floatarg f)
{
  x->f_lp = f;
}

void clds_reverb_tilde_setup(void) {
  clds_reverb_tilde_class = class_new(gensym("clds_reverb~"),
                                         (t_newmethod)clds_reverb_tilde_new,
                                         0, sizeof(t_clds_reverb_tilde),
                                         CLASS_DEFAULT,
                                         A_DEFFLOAT, A_NULL);

  class_addmethod(  clds_reverb_tilde_class,
                    (t_method)clds_reverb_tilde_dsp,
                    gensym("dsp"), A_NULL);
  CLASS_MAINSIGNALIN(clds_reverb_tilde_class, t_clds_reverb_tilde, f_dummy);


  class_addmethod(clds_reverb_tilde_class,
                  (t_method) clds_reverb_tilde_amount, gensym("amount"),
                  A_DEFFLOAT, A_NULL);
  class_addmethod(clds_reverb_tilde_class,
                  (t_method) clds_reverb_tilde_gain, gensym("gain"),
                  A_DEFFLOAT, A_NULL);
  class_addmethod(clds_reverb_tilde_class,
                  (t_method) clds_reverb_tilde_time, gensym("time"),
                  A_DEFFLOAT, A_NULL);
  class_addmethod(clds_reverb_tilde_class,
                  (t_method) clds_reverb_tilde_diffusion, gensym("diffusion"),
                  A_DEFFLOAT, A_NULL);
  class_addmethod(clds_reverb_tilde_class,
                  (t_method) clds_reverb_tilde_lp, gensym("lp"),
                  A_DEFFLOAT, A_NULL);
}
// puredata methods implementation - end

