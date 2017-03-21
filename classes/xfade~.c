#include "m_pd.h"

static t_class *xfade_tilde_class;

typedef struct _xfade_tilde {
  t_object  x_obj;
  //t_float mix;   //The mix value (0: left input, 1: right input)
  int n_in;
  int channels;
  t_sample **in;
  int n_out;
  t_sample **out;
  t_sample f;
  t_sample* buffer;   //Input frame buffer: one sample of every input
} t_xfade_tilde;


static t_int *xfade_tilde_perform(t_int *w)
{
  t_xfade_tilde *x = (t_xfade_tilde *)(w[1]);
  int n = (int)(w[2]);
  int i;
  int j;
  t_sample* out;
  t_sample* mix = (t_sample *) (w[3]);
  t_sample mix_f;
  t_sample inv_mix_f;
  for (j =0; j < n; j++)
  {
	  mix_f = *mix++; // RIGHT
	  if (mix_f > 1) mix_f = 1;
	  if (mix_f < 0) mix_f = 0;
	  inv_mix_f = (1 - mix_f); // LEFT
	  // Copy one sample of all the inputs
	  for ( i=0; i < x->n_in;i++ )
      {
       x->buffer[i] = (t_sample) x->in[i][j];
	  }
    for ( i=0; i < x->channels;i++ )
      {
	  out = (t_sample *)(x->out[i]);
      out[j] = (x->buffer[i] * inv_mix_f) + (x->buffer[i+x->channels] * mix_f) ; 
      }
  }
  return (w+4);
}

// Annouce signal inputs and outputs to the DSP chain
static void xfade_tilde_dsp(t_xfade_tilde *x, t_signal **sp)
{
  int n;
  t_sample **dummy=x->in;
  for(n=0;n<x->n_in;n++)*dummy++=sp[n]->s_vec;
  //Add +1 because of the mix inlet
  dummy=x->out;
  for(n=0;n<x->n_out;n++)*dummy++=sp[n+x->n_in+1]->s_vec;
  dsp_add(xfade_tilde_perform, 3, x, sp[0]->s_n, sp[x->n_in]->s_vec);
}

static void xfade_tilde_free( t_xfade_tilde *x)
{
	 freebytes(x->in, x->n_in * sizeof(t_sample *));
	 freebytes(x->out, x->n_out * sizeof(t_sample *));
	 freebytes(x->buffer,x->n_in * sizeof( * x->buffer));
}

static void *xfade_tilde_new(t_floatarg f)
{
  t_xfade_tilde *x = (t_xfade_tilde *)pd_new(xfade_tilde_class);

  x->channels = (int) f;
  if ( x->channels  < 1 ) x->channels = 2;
  x->n_in =  x->channels * 2;
  x->in = (t_sample **)getbytes(x->n_in * sizeof(t_sample *));
  int i=x->n_in;
  while(i--)x->in[i]=0;
  x->n_out = x->channels;
  x->out = (t_sample **)getbytes(x->n_in * sizeof(t_sample *));
  i=x->n_out;
  while(i--)x->out[i]=0;
  x->buffer = getbytes(x->n_in * sizeof( * x->buffer));
  for (i=0; i < x->n_in - 1; i++)
    {
	inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    }
  inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
  for (i=0; i < x->n_out; i++)
    {
        outlet_new(&x->x_obj, &s_signal);
    }
  return (void *)x;
}

 void xfade_tilde_setup(void) {
	   
  xfade_tilde_class = class_new(gensym("xfade~"),
        (t_newmethod)xfade_tilde_new,
        (t_method)xfade_tilde_free, sizeof(t_xfade_tilde),
        0, A_DEFFLOAT, 0);
  CLASS_MAINSIGNALIN(xfade_tilde_class, t_xfade_tilde, f);
  class_addmethod(xfade_tilde_class,
        (t_method)xfade_tilde_dsp, gensym("dsp"), A_CANT, 0);
}
