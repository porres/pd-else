#include "m_pd.h"

//IMPROVE - 
//IMPROVE - 
//TODO - hep file



#include "elements/dsp/part.h"

inline float constrain(float v, float vMin, float vMax) {
  return std::max<float>(vMin,std::min<float>(vMax, v));
}

static t_class *lmnts_tilde_class;

typedef struct _lmnts_tilde {
  t_object  x_obj;

  t_float  f_dummy;

  t_float f_gate;
  t_float f_pitch;
  t_float f_contour;
  t_float f_bow_level;
  t_float f_bow_timbre;
  t_float f_blow_level;
  t_float f_blow_flow;
  t_float f_blow_timbre;
  t_float f_strike_level;
  t_float f_strike_mallet;
  t_float f_strike_timbre;
  t_float f_resonator;
  t_float f_geometry;
  t_float f_brightness;
  t_float f_damping;
  t_float f_position;
  t_float f_space;
  t_float f_mod_pitch;
  t_float f_mod_depth;
  t_float f_seed;
  t_float f_bypass;
  t_float f_easter_egg;

  // CLASS_MAINSIGNALIN  = in_strke
  t_inlet*  x_in_blow;
  t_outlet* x_out_left;
  t_outlet* x_out_right;

  elements::Part part;
  elements::PerformanceState state;

  uint32_t seed=0;
  uint32_t resonator = 0;
  bool panic = false;

  const float kNoiseGateThreshold = 0.0001f;
  float strike_in_level = 0.0f;
  float blow_in_level = 0.0f;

  static const int ELEMENTS_SZ= 32768;
  uint16_t buffer[ELEMENTS_SZ];

  float* blow;
  float* strike;
  int iobufsz;

} t_lmnts_tilde;


//define pure data methods
extern "C"  {
  t_int*  lmnts_tilde_render(t_int *w);
  void    lmnts_tilde_dsp(t_lmnts_tilde *x, t_signal **sp);
  void    lmnts_tilde_free(t_lmnts_tilde *x);
  void*   lmnts_tilde_new(t_floatarg f);
  void    lmnts_tilde_setup(void);

  void lmnts_tilde_gate(t_lmnts_tilde *x, t_floatarg f);

  void lmnts_tilde_pitch(t_lmnts_tilde *x, t_floatarg f);
  void lmnts_tilde_contour(t_lmnts_tilde *x, t_floatarg f);
  void lmnts_tilde_bow_level(t_lmnts_tilde *x, t_floatarg f);
  void lmnts_tilde_bow_timbre(t_lmnts_tilde *x, t_floatarg f);
  void lmnts_tilde_blow_level(t_lmnts_tilde *x, t_floatarg f);
  void lmnts_tilde_blow_flow(t_lmnts_tilde *x, t_floatarg f);
  void lmnts_tilde_blow_timbre(t_lmnts_tilde *x, t_floatarg f);
  void lmnts_tilde_strike_level(t_lmnts_tilde *x, t_floatarg f);
  void lmnts_tilde_strike_mallet(t_lmnts_tilde *x, t_floatarg f);
  void lmnts_tilde_strike_timbre(t_lmnts_tilde *x, t_floatarg f);
  void lmnts_tilde_resonator(t_lmnts_tilde *x, t_floatarg f);
  void lmnts_tilde_geometry(t_lmnts_tilde *x, t_floatarg f);
  void lmnts_tilde_brightness(t_lmnts_tilde *x, t_floatarg f);
  void lmnts_tilde_damping(t_lmnts_tilde *x, t_floatarg f);
  void lmnts_tilde_position(t_lmnts_tilde *x, t_floatarg f);
  void lmnts_tilde_space(t_lmnts_tilde *x, t_floatarg f);
  void lmnts_tilde_mod_pitch(t_lmnts_tilde *x, t_floatarg f);
  void lmnts_tilde_mod_depth(t_lmnts_tilde *x, t_floatarg f);
  void lmnts_tilde_seed(t_lmnts_tilde *x, t_floatarg f);
  void lmnts_tilde_bypass(t_lmnts_tilde *x, t_floatarg f);
  void lmnts_tilde_easter_egg(t_lmnts_tilde *x, t_floatarg f);
}

// puredata methods implementation -start
t_int *lmnts_tilde_render(t_int *w)
{
  t_lmnts_tilde *x   = (t_lmnts_tilde *)(w[1]);
  t_sample  *in_strike   = (t_sample *)(w[2]);
  t_sample  *in_blow  = (t_sample *)(w[3]);
  t_sample  *out_left  = (t_sample *)(w[4]);
  t_sample  *out_right = (t_sample *)(w[5]);
  int n =  (int)(w[6]);

  if (n > x->iobufsz) {
    delete [] x->strike;
    delete [] x->blow;
    x->iobufsz = n;
    x->strike = new float[ x->iobufsz];
    x->blow = new float[ x->iobufsz];
  }


  x->part.mutable_patch()->exciter_envelope_shape  = constrain(x->f_contour   ,0.0f, 1.0f);
  x->part.mutable_patch()->exciter_bow_level    = constrain(x->f_bow_level    ,0.0f, 1.0f);
  x->part.mutable_patch()->exciter_bow_timbre   = constrain(x->f_bow_timbre   ,0.0f, 0.9995f);

  x->part.mutable_patch()->exciter_blow_level   = constrain(x->f_blow_level   ,0.0f, 1.0f);
  x->part.mutable_patch()->exciter_blow_meta    = constrain(x->f_blow_flow    ,0.0f, 0.9995f);
  x->part.mutable_patch()->exciter_blow_timbre  = constrain(x->f_blow_timbre  ,0.0f, 0.9995f);

  x->part.mutable_patch()->exciter_strike_level = constrain(x->f_strike_level   ,0.0f, 1.0f);
  x->part.mutable_patch()->exciter_strike_meta  = constrain(x->f_strike_mallet  ,0.0f, 0.9995f);
  x->part.mutable_patch()->exciter_strike_timbre = constrain(x->f_strike_timbre ,0.0f, 0.9995f);

  x->part.mutable_patch()->resonator_geometry   = constrain(x->f_geometry   ,0.0f, 0.9995f);
  x->part.mutable_patch()->resonator_brightness = constrain(x->f_brightness ,0.0f, 0.9995f);
  x->part.mutable_patch()->resonator_damping    = constrain(x->f_damping    ,0.0f, 0.9995f);
  x->part.mutable_patch()->resonator_position   = constrain( x->f_position  ,0.0f, 0.9995f);

  x->part.mutable_patch()->space                = constrain(x->f_space * 2.0  ,0.0f, 2.0f);


  uint32_t nresonator = (int (x->f_resonator) % 3);
  if(x->resonator != nresonator) {
    x->resonator = nresonator;
    x->part.set_resonator_model(elements::ResonatorModel(x->resonator));
  }

  uint32_t nseed = (uint32_t) x->f_seed;
  if(x->seed != nseed) {
    x->seed = nseed;
    x->part.Seed(&x->seed, 1);
  }

  x->part.set_bypass(x->f_bypass > 0.5);
  x->part.set_easter_egg(x->f_easter_egg > 0.5);

  for(int i=0;i<n;i++){
      float blow_in_sample = in_blow[i];
      float strike_in_sample = in_strike[i];

     float error =0.0f, gain=1.0f;
  //    error = strike_in_sample * strike_in_sample - strike_in_level;
  //    strike_in_level += error * (error > 0.0f ? 0.1f : 0.0001f);
  //    gain = strike_in_level <= kNoiseGateThreshold 
  //          ? (1.0f / kNoiseGateThreshold) * strike_in_level : 1.0f;
      x->strike[i] = gain * strike_in_sample;
      
  //    error = blow_in_sample * blow_in_sample - blow_in_level;
  //    blow_in_level += error * (error > 0.0f ? 0.1f : 0.0001f);
  //    gain = blow_in_level <= kNoiseGateThreshold 
  //          ? (1.0f / kNoiseGateThreshold) * blow_in_level : 1.0f;
      x->blow[i] = gain * blow_in_sample;

  }

  x->state.gate       = ( x->f_gate > 0.5);
  x->state.note       = constrain(x->f_pitch * 64.0f, -64.0f, 64.0f);
  x->state.strength   = constrain(x->f_mod_depth  ,0.0f, 1.0f);
  x->state.modulation = constrain(x->f_mod_pitch * 60.0f, -60.0f, 60.0f);

  x->part.Process(x->state, x->blow, x->strike, out_left, out_right, n);


  return (w + 7); // # args + 1
}


void lmnts_tilde_dsp(t_lmnts_tilde *x, t_signal **sp)
{
  // add the perform method, with all signal i/o
  dsp_add(lmnts_tilde_render, 6,
          x,
          sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, // signal i/o (clockwise)
          sp[0]->s_n);
}

void lmnts_tilde_free(t_lmnts_tilde *x)
{
  delete [] x->strike;
  delete [] x->blow;

  inlet_free(x->x_in_blow);
  outlet_free(x->x_out_left);
  outlet_free(x->x_out_right);
}

void *lmnts_tilde_new(t_floatarg)
{
  t_lmnts_tilde *x = (t_lmnts_tilde *) pd_new(lmnts_tilde_class);

  x->f_gate = 0.0;
  x->f_pitch = 0.0;
  x->f_contour = 0.0;
  x->f_bow_level = 0.0f;
  x->f_bow_timbre= 0.0f;
  x->f_blow_level= 0.0f;
  x->f_blow_flow= 0.0f;
  x->f_blow_timbre= 0.0f;
  x->f_strike_level= 0.0f;
  x->f_strike_mallet= 0.0f;
  x->f_strike_timbre= 0.0f;
  x->f_resonator= 0.0f;
  x->f_geometry= 0.0f;
  x->f_brightness= 0.0f;
  x->f_damping= 0.0f;
  x->f_position= 0.0f;
  x->f_space= 0.0f;
  x->f_mod_pitch= 0.0f;
  x->f_mod_depth= 0.0f;
  x->f_seed= 1.0f;
  x->f_bypass= 0.0f;
  x->f_easter_egg= 0.0f;

  x->part.Init(x->buffer);

  // x->seed = 0;
  // x->part.Seed(&x->seed, 1);

  // x->resonator = 0;
  // x->part.set_resonator_model(elements::ResonatorModel(x->resonator));
  // x->state.gate = false;
  // x->state.note = 0.0f;
  // x->state.modulation = 0.0f;
  // x->state.strength = 0.0f;

  x->iobufsz = 64;
  x->strike = new float[ x->iobufsz];
  x->blow = new float[ x->iobufsz];

  //x_in_strike = main input 
  x->x_in_blow    = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
  x->x_out_left   = outlet_new(&x->x_obj, &s_signal);
  x->x_out_right  = outlet_new(&x->x_obj, &s_signal);

  return (void *)x;
}


void lmnts_tilde_setup(void) {
  lmnts_tilde_class = class_new(  gensym("lmnts~"),
                                  (t_newmethod)lmnts_tilde_new,
                                  (t_method) lmnts_tilde_free,
                                  sizeof(t_lmnts_tilde),
                                  CLASS_DEFAULT,
                                  A_DEFFLOAT, A_NULL);

  class_addmethod(  lmnts_tilde_class,
                    (t_method)lmnts_tilde_dsp,
                    gensym("dsp"), A_NULL);

  // represents strike input
  CLASS_MAINSIGNALIN(lmnts_tilde_class, t_lmnts_tilde, f_dummy);

  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_gate, gensym("gate"),
    A_DEFFLOAT,A_NULL);

  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_pitch, gensym("pitch"),
    A_DEFFLOAT,A_NULL);
  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_contour, gensym("contour"),
    A_DEFFLOAT,A_NULL);
  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_bow_level, gensym("bow_level"),
    A_DEFFLOAT,A_NULL);
  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_bow_timbre, gensym("bow_timbre"),
    A_DEFFLOAT,A_NULL);
  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_blow_level, gensym("blow_level"),
    A_DEFFLOAT,A_NULL);
  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_blow_flow, gensym("blow_flow"),
    A_DEFFLOAT,A_NULL);
  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_blow_timbre, gensym("blow_timbre"),
    A_DEFFLOAT,A_NULL);
  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_strike_level, gensym("strike_level"),
    A_DEFFLOAT,A_NULL);
  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_strike_mallet, gensym("strike_mallet"),
    A_DEFFLOAT,A_NULL);
  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_strike_timbre, gensym("strike_timbre"),
    A_DEFFLOAT,A_NULL);
  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_resonator, gensym("resonator"),
    A_DEFFLOAT,A_NULL);
  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_geometry, gensym("geometry"),
    A_DEFFLOAT,A_NULL);
  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_brightness, gensym("brightness"),
    A_DEFFLOAT,A_NULL);
  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_damping, gensym("damping"),
    A_DEFFLOAT,A_NULL);
  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_position, gensym("position"),
    A_DEFFLOAT,A_NULL);
  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_space, gensym("space"),
    A_DEFFLOAT,A_NULL);
  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_mod_pitch, gensym("mod_pitch"),
    A_DEFFLOAT,A_NULL);
  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_mod_depth, gensym("mod_depth"),
    A_DEFFLOAT,A_NULL);
  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_seed, gensym("seed"),
    A_DEFFLOAT,A_NULL);
  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_bypass, gensym("bypass"),
    A_DEFFLOAT,A_NULL);
  class_addmethod(lmnts_tilde_class,
    (t_method) lmnts_tilde_easter_egg, gensym("easter_egg"),
    A_DEFFLOAT,A_NULL);

}

void lmnts_tilde_gate(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_gate = f;
}

void lmnts_tilde_pitch(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_pitch = f;
}

void lmnts_tilde_contour(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_contour = f;
}

void lmnts_tilde_bow_level(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_bow_level = f;
}

void lmnts_tilde_bow_timbre(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_bow_timbre = f;
}

void lmnts_tilde_blow_level(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_blow_level = f;
}

void lmnts_tilde_blow_flow(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_blow_flow = f;
}

void lmnts_tilde_blow_timbre(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_blow_timbre = f;
}

void lmnts_tilde_strike_level(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_strike_level = f;
}

void lmnts_tilde_strike_mallet(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_strike_mallet = f;
}

void lmnts_tilde_strike_timbre(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_strike_timbre = f;
}

void lmnts_tilde_resonator(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_resonator = f;
}

void lmnts_tilde_geometry(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_geometry = f;
}

void lmnts_tilde_brightness(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_brightness = f;
}

void lmnts_tilde_damping(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_damping = f;
}

void lmnts_tilde_position(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_position = f;
}

void lmnts_tilde_space(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_space = f;
}

void lmnts_tilde_mod_pitch(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_mod_pitch = f;
}

void lmnts_tilde_mod_depth(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_mod_depth = f;
}

void lmnts_tilde_seed(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_seed = f;
}

void lmnts_tilde_bypass(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_bypass = f;
}

void lmnts_tilde_easter_egg(t_lmnts_tilde *x, t_floatarg f) 
{
  x->f_easter_egg = f;

}



// puredata methods implementation - end
