/**
 *
 * a puredata wrapper for aubio tempo detection functions
 *
 * Thanks to Johannes M Zmolnig for writing the excellent HOWTO:
 *       http://iem.kug.ac.at/pd/externals-HOWTO/
 *
 * */

#include <m_pd.h>
#include <aubio/src/aubio.h>

static t_class *tempo_tilde_class;

void tempodetect_tilde_setup (void);

typedef struct _tempo_tilde
{
  t_object x_obj;
  t_float threshold;
  t_float silence;
  t_int pos; /*frames%dspblocksize*/
  t_int bufsize;
  t_int hopsize;
  aubio_tempo_t * t;
  fvec_t *vec;
  fvec_t *output;
  t_outlet *tempobang;
  t_outlet *onsetbang;
} t_tempo_tilde;

static t_int *tempo_tilde_perform(t_int *w)
{
  t_tempo_tilde *x = (t_tempo_tilde *)(w[1]);
  t_sample *in          = (t_sample *)(w[2]);
  int n                 = (int)(w[3]);
  int j;
  for (j=0;j<n;j++) {
    /* write input to datanew */
    fvec_set_sample(x->vec, in[j], x->pos);
    /*time for fft*/
    if (x->pos == x->hopsize-1) {
      /* block loop */
      aubio_tempo_do (x->t, x->vec, x->output);
      if (x->output->data[0]) {
        outlet_bang(x->tempobang);
      }
      if (x->output->data[1]) {
        outlet_bang(x->onsetbang);
      }
      /* end of block loop */
      x->pos = -1; /* so it will be zero next j loop */
    }
    x->pos++;
  }
  return (w+4);
}

static void tempo_tilde_dsp(t_tempo_tilde *x, t_signal **sp)
{
  dsp_add(tempo_tilde_perform, 3, x, sp[0]->s_vec, sp[0]->s_n);
}

static void *tempo_tilde_new (t_floatarg f)
{
  t_tempo_tilde *x =
    (t_tempo_tilde *)pd_new(tempo_tilde_class);

  x->threshold = (f < 1e-5) ? 0.1 : (f > 10.) ? 10. : f;
  x->silence = -70.;
  /* should get from block~ size */
  x->bufsize   = 1024;
  x->hopsize   = x->bufsize / 2;

  x->t = new_aubio_tempo ("specdiff", x->bufsize, x->hopsize,
          (uint_t) sys_getsr ());
  aubio_tempo_set_silence(x->t,x->silence);
  aubio_tempo_set_threshold(x->t,x->threshold);
  x->output = (fvec_t *)new_fvec(2);
  x->vec = (fvec_t *)new_fvec(x->hopsize);

  floatinlet_new (&x->x_obj, &x->threshold);
  x->tempobang = outlet_new (&x->x_obj, &s_bang);
  x->onsetbang = outlet_new (&x->x_obj, &s_bang);
  return (void *)x;
}

static void tempo_tilde_del(t_tempo_tilde *x)
{
  del_aubio_tempo(x->t);
  del_fvec(x->output);
  del_fvec(x->vec);
}

void tempodetect_tilde_setup (void)
{
  tempo_tilde_class = class_new (gensym ("tempodetect~"),
      (t_newmethod)tempo_tilde_new,
      (t_method)tempo_tilde_del,
      sizeof (t_tempo_tilde),
      CLASS_DEFAULT, A_DEFFLOAT, 0);
  class_addmethod(tempo_tilde_class,
      (t_method)tempo_tilde_dsp,
      gensym("dsp"), 0);
  CLASS_MAINSIGNALIN(tempo_tilde_class,
      t_tempo_tilde, threshold);
}
