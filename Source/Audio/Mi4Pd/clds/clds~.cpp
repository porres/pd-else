#include "m_pd.h"

//IMPROVE - inlets
//TODO - types on params?

static const char* clds_version = "0.5"; 

#include "clouds/dsp/granular_processor.h"

inline float constrain(float v, float vMin, float vMax) {
  return std::max<float>(vMin,std::min<float>(vMax, v));
}

inline short TO_SHORTFRAME(float v)   { return (short (v * 16384.0f));}
inline float FROM_SHORTFRAME(short v) { return (float(v) / 16384.0f); }

static t_class *clds_tilde_class;

typedef struct _clds_tilde {
  t_object  x_obj;

  t_float  f_dummy;

  t_float f_freeze;
  t_float f_trig;
  t_float f_position;
  t_float f_size;
  t_float f_pitch;
  t_float f_density;
  t_float f_texture;
  t_float f_mix;
  t_float f_spread;
  t_float f_feedback;
  t_float f_reverb;
  t_float f_mode;
  t_float f_mono;
  t_float f_silence;
  t_float f_bypass;
  t_float f_lofi;

  // CLASS_MAINSIGNALIN  = in_left;
  t_inlet*  x_in_right;
  t_outlet* x_out_left;
  t_outlet* x_out_right;

  clouds::GranularProcessor processor;
  bool ltrig;
  clouds::ShortFrame* ibuf;
  clouds::ShortFrame* obuf;
  int iobufsz;

  static const int LARGE_BUF = 524288;
  static const int SMALL_BUF = 262144;
  uint8_t* large_buf;
  int      large_buf_size;
  uint8_t* small_buf;
  int      small_buf_size;

} t_clds_tilde;


//define pure data methods
extern "C"  {
  t_int* clds_tilde_render(t_int *w);
  void clds_tilde_dsp(t_clds_tilde *x, t_signal **sp);
  void clds_tilde_free(t_clds_tilde *x);
  void* clds_tilde_new(t_floatarg lsize, t_floatarg ssize);
  void clds_tilde_setup(void);

  void clds_tilde_freeze(t_clds_tilde *x, t_floatarg);
  void clds_tilde_trig(t_clds_tilde *x, t_floatarg);
  void clds_tilde_position(t_clds_tilde *x, t_floatarg);
  void clds_tilde_size(t_clds_tilde *x, t_floatarg);
  void clds_tilde_pitch(t_clds_tilde *x, t_floatarg);
  void clds_tilde_density(t_clds_tilde *x, t_floatarg);
  void clds_tilde_texture(t_clds_tilde *x, t_floatarg);
  void clds_tilde_mix(t_clds_tilde *x, t_floatarg);
  void clds_tilde_spread(t_clds_tilde *x, t_floatarg);
  void clds_tilde_feedback(t_clds_tilde *x, t_floatarg);
  void clds_tilde_reverb(t_clds_tilde *x, t_floatarg);
  void clds_tilde_mode(t_clds_tilde *x, t_floatarg);
  void clds_tilde_mono(t_clds_tilde *x, t_floatarg);
  void clds_tilde_silence(t_clds_tilde *x, t_floatarg);
  void clds_tilde_bypass(t_clds_tilde *x, t_floatarg);
  void clds_tilde_lofi(t_clds_tilde *x, t_floatarg);
}

// puredata methods implementation -start
t_int *clds_tilde_render(t_int *w)
{
  t_clds_tilde *x   = (t_clds_tilde *)(w[1]);
  t_sample  *in_left   = (t_sample *)(w[2]);
  t_sample  *in_right  = (t_sample *)(w[3]);
  t_sample  *out_right = (t_sample *)(w[4]);
  t_sample  *out_left  = (t_sample *)(w[5]);
  int n =  (int)(w[6]);

  //TODO - blowing up... fidility is probably due to downsampler
  //x->processor.set_num_channels(x->f_mono  < 0.5f ? 1 : 2 );
  //x->processor.set_low_fidelity(x->f_lofi > 0.5f);
  //x->processor.set_num_channels(2);
  x->processor.set_low_fidelity(false);
  // for now restrict playback mode to working modes (granular and looping) 
  clouds::PlaybackMode mode  = (clouds::PlaybackMode) ( int(x->f_mode) % clouds::PLAYBACK_MODE_LAST);
  if(mode!=x->processor.playback_mode()) {
    switch(mode) {
    case clouds::PLAYBACK_MODE_GRANULAR: post("clds:granular"); x->processor.set_num_channels(2);break;
    case clouds::PLAYBACK_MODE_STRETCH: post("clds:stretch"); x->processor.set_num_channels(2);break;
    case clouds::PLAYBACK_MODE_LOOPING_DELAY: post("clds:looping"); x->processor.set_num_channels(2);break;
    case clouds::PLAYBACK_MODE_SPECTRAL: post("clds:spectral"); x->processor.set_num_channels(1);break;
    case clouds::PLAYBACK_MODE_LAST:
    default: post("clds : unknown mode");    
    }  
  }
  // clouds::PlaybackMode mode =  
  //   (x->f_mode < 0.5f) 
  //   ? clouds::PLAYBACK_MODE_GRANULAR 
  //   : clouds::PLAYBACK_MODE_LOOPING_DELAY;
  x->processor.set_playback_mode(mode);
///
  x->processor.mutable_parameters()->position  = constrain(x->f_position,   0.0f,1.0f);
  x->processor.mutable_parameters()->size    = constrain(x->f_size,     0.0f,1.0f);
  x->processor.mutable_parameters()->texture   = constrain(x->f_texture, 0.0f,1.0f);
  x->processor.mutable_parameters()->dry_wet   = constrain(x->f_mix,       0.0f,1.0f);
  x->processor.mutable_parameters()->stereo_spread= constrain(x->f_spread,    0.0f,1.0f);
  x->processor.mutable_parameters()->feedback  = constrain(x->f_feedback,   0.0f,1.0f);
  x->processor.mutable_parameters()->reverb    = constrain(x->f_reverb,     0.0f,1.0f);

  x->processor.mutable_parameters()->pitch   = constrain(x->f_pitch * 64.0f,-64.0f,64.0f);

  // restrict density to .2 to .8 for granular mode, outside this breaks up
  float density = constrain(x->f_density, 0.0f,1.0f);
  density = (mode == clouds::PLAYBACK_MODE_GRANULAR) ? (density*0.6f)+0.2f : density;
  x->processor.mutable_parameters()->density = constrain(density, 0.0f, 1.0f);

  x->processor.mutable_parameters()->freeze = (x->f_freeze > 0.5f);

  //note the trig input is really a gate... which then feeds the trig
  x->processor.mutable_parameters()->gate = (x->f_trig > 0.5f);

  bool trig = false;
  if((x->f_trig > 0.5f)  && !x->ltrig) {
    x->ltrig = true;
    trig  = true;
  } else if (! (x->f_trig > 0.5f))  {
    x->ltrig = false;
  }
  x->processor.mutable_parameters()->trigger = trig;

  x->processor.set_bypass(x->f_bypass > 0.5f);
  x->processor.set_silence(x->f_silence > 0.5f);

  if (n > x->iobufsz) {
    delete [] x->ibuf;
    delete [] x->obuf;
    x->iobufsz = n;
    x->ibuf = new clouds::ShortFrame[x->iobufsz];
    x->obuf = new clouds::ShortFrame[x->iobufsz];
  }

  for (int i = 0; i < n; i++) {
    x->ibuf[i].l = TO_SHORTFRAME(in_left[i]);
    x->ibuf[i].r = TO_SHORTFRAME(in_right[i]);
  }


  x->processor.Prepare();
  x->processor.Process(x->ibuf, x->obuf,n);

  for (int i = 0; i < n; i++) {
    out_left[i]  = FROM_SHORTFRAME(x->obuf[i].l);
    out_right[i] = FROM_SHORTFRAME(x->obuf[i].r);
  }

  return (w + 7); // # args + 1
}

void clds_tilde_dsp(t_clds_tilde *x, t_signal **sp)
{
  // add the perform method, with all signal i/o
  dsp_add(clds_tilde_render, 6,
          x,
          sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, // signal i/o (clockwise)
          sp[0]->s_n);
}

void clds_tilde_free(t_clds_tilde *x)
{
  delete [] x->ibuf;
  delete [] x->obuf;
  delete [] x->small_buf;
  delete [] x->large_buf;

  inlet_free(x->x_in_right);
  outlet_free(x->x_out_left);
  outlet_free(x->x_out_right);
}

void *clds_tilde_new(t_floatarg lsize,t_floatarg ssize)
{
  t_clds_tilde *x = (t_clds_tilde *) pd_new(clds_tilde_class);
  x->ltrig = false;

  x->iobufsz = 64;
  x->ibuf = new clouds::ShortFrame[x->iobufsz];
  x->obuf = new clouds::ShortFrame[x->iobufsz];
  x->large_buf_size = (lsize <t_clds_tilde::LARGE_BUF ? t_clds_tilde::LARGE_BUF : lsize);
  x->large_buf = new uint8_t[x->large_buf_size];
  x->small_buf_size =  (ssize < t_clds_tilde::SMALL_BUF ? t_clds_tilde::SMALL_BUF : ssize);;
  x->small_buf = new uint8_t[x->small_buf_size];


  x->x_in_right   = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
  x->x_out_left   = outlet_new(&x->x_obj, &s_signal);
  x->x_out_right  = outlet_new(&x->x_obj, &s_signal);

  x->f_freeze = 0.0f;
  x->f_trig = 0.0f;
  x->f_position = 0.0f;
  x->f_size = 0.0f;
  x->f_pitch = 0.0f;
  x->f_density = 0.0f;
  x->f_texture = 0.0f;
  x->f_mix = 0.0f;
  x->f_spread = 0.0f;
  x->f_feedback = 0.0f;
  x->f_reverb = 0.0f;
  x->f_mode = 0.0f;
  x->f_mono = 0.0f;
  x->f_silence = 0.0f;
  x->f_bypass = 0.0f;
  x->f_lofi = 0.0f;

  x->processor.Init(
    x->large_buf,x->large_buf_size, 
    x->small_buf,x->small_buf_size);

  x->ltrig = false;

  post("clds~ version:%s, lbufsz:%d sbufsz: %d", clds_version, x->large_buf_size, x->small_buf_size);
  return (void *)x;
}


void clds_tilde_setup(void) {
  clds_tilde_class = class_new(gensym("clds~"),
                                         (t_newmethod) clds_tilde_new,
                                         (t_method) clds_tilde_free, 
                                         sizeof(t_clds_tilde),
                                         CLASS_DEFAULT,
                                         A_DEFFLOAT, 
                                         A_DEFFLOAT,
                                         A_NULL);

  class_addmethod(  clds_tilde_class,
                    (t_method)clds_tilde_dsp,
                    gensym("dsp"), A_NULL);

  CLASS_MAINSIGNALIN(clds_tilde_class, t_clds_tilde, f_dummy);


  class_addmethod(clds_tilde_class,
    (t_method) clds_tilde_freeze, gensym("freeze"),
    A_DEFFLOAT, A_NULL);
  class_addmethod(clds_tilde_class,
    (t_method) clds_tilde_trig, gensym("trig"),
    A_DEFFLOAT, A_NULL);
  class_addmethod(clds_tilde_class,
    (t_method) clds_tilde_position, gensym("position"),
    A_DEFFLOAT, A_NULL);
  class_addmethod(clds_tilde_class,
    (t_method) clds_tilde_size, gensym("size"),
    A_DEFFLOAT, A_NULL);
  class_addmethod(clds_tilde_class,
    (t_method) clds_tilde_pitch, gensym("pitch"),
    A_DEFFLOAT, A_NULL);
  class_addmethod(clds_tilde_class,
    (t_method) clds_tilde_density, gensym("density"),
    A_DEFFLOAT, A_NULL);
  class_addmethod(clds_tilde_class,
    (t_method) clds_tilde_texture, gensym("texture"),
    A_DEFFLOAT, A_NULL);
  class_addmethod(clds_tilde_class,
    (t_method) clds_tilde_mix, gensym("mix"),
    A_DEFFLOAT, A_NULL);
  class_addmethod(clds_tilde_class,
    (t_method) clds_tilde_spread, gensym("spread"),
    A_DEFFLOAT, A_NULL);
  class_addmethod(clds_tilde_class,
    (t_method) clds_tilde_feedback, gensym("feedback"),
    A_DEFFLOAT, A_NULL);
  class_addmethod(clds_tilde_class,
    (t_method) clds_tilde_reverb, gensym("reverb"),
    A_DEFFLOAT, A_NULL);
  class_addmethod(clds_tilde_class,
    (t_method) clds_tilde_mode, gensym("mode"),
    A_DEFFLOAT, A_NULL);
  class_addmethod(clds_tilde_class,
    (t_method) clds_tilde_mono, gensym("mono"),
    A_DEFFLOAT, A_NULL);
  class_addmethod(clds_tilde_class,
    (t_method) clds_tilde_silence, gensym("silence"),
    A_DEFFLOAT, A_NULL);
  class_addmethod(clds_tilde_class,
    (t_method) clds_tilde_bypass, gensym("bypass"),
    A_DEFFLOAT, A_NULL);
  class_addmethod(clds_tilde_class,
    (t_method) clds_tilde_lofi, gensym("lofi"),
    A_DEFFLOAT, A_NULL);
}



void clds_tilde_freeze(t_clds_tilde *x, t_floatarg f)
{
  x->f_freeze = f;
}
void clds_tilde_trig(t_clds_tilde *x, t_floatarg f)
{
  x->f_trig = f;
}
void clds_tilde_position(t_clds_tilde *x, t_floatarg f)
{
  x->f_position = f;
}
void clds_tilde_size(t_clds_tilde *x, t_floatarg f)
{
  x->f_size = f;
}
void clds_tilde_pitch(t_clds_tilde *x, t_floatarg f)
{
  x->f_pitch = f;
}
void clds_tilde_density(t_clds_tilde *x, t_floatarg f)
{
  x->f_density = f;
}

void clds_tilde_texture(t_clds_tilde *x, t_floatarg f)
{
  x->f_texture = f;
}

void clds_tilde_mix(t_clds_tilde *x, t_floatarg f)
{
  x->f_mix = f;
}

void clds_tilde_spread(t_clds_tilde *x, t_floatarg f)
{
  x->f_spread = f;
}

void clds_tilde_feedback(t_clds_tilde *x, t_floatarg f)
{
  x->f_feedback = f;
}

void clds_tilde_reverb(t_clds_tilde *x, t_floatarg f)
{
  x->f_reverb = f;
}

void clds_tilde_mode(t_clds_tilde *x, t_floatarg f)
{
  x->f_mode = f;
}

void clds_tilde_mono(t_clds_tilde *x, t_floatarg f)
{
  x->f_mono = f;
}

void clds_tilde_silence(t_clds_tilde *x, t_floatarg f)
{
  x->f_silence = f;
}

void clds_tilde_bypass(t_clds_tilde *x, t_floatarg f)
{
  x->f_bypass = f;
}

void clds_tilde_lofi(t_clds_tilde *x, t_floatarg f)
{
  x->f_lofi = f;
}

// puredata methods implementation - end
