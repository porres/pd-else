#include "m_pd.h"

//IMPROVE - inlets
//IMPROVE - variable  buffer size

#include <algorithm> 
#include "braids/macro_oscillator.h"
#include "braids/envelope.h"
#include "braids/vco_jitter_source.h"


// TOOD
// add parameters
// trigger handling
// sync handling


// for signature_waveshaper, need abs
inline int16_t abs(int16_t x) { return x <0.0f ? -x : x;}
#include "braids/signature_waveshaper.h"

static t_class *brds_tilde_class;

inline float constrain(float v, float vMin, float vMax) {
  return std::max<float>(vMin,std::min<float>(vMax, v));
}

inline int constrain(int v, int vMin, int vMax) {
  return std::max<int>(vMin,std::min<int>(vMax, v));
}

typedef struct _brds_tilde {
  t_object  x_obj;

  t_float f_dummy;

  t_float f_shape;
  t_float f_pitch;
  t_float f_trig;
  t_float f_fm;
  t_float f_modulation;
  t_float f_colour;
  t_float f_timbre;

  t_float f_ad_attack;
  t_float f_ad_decay;
  t_float f_ad_mod_vca;
  t_float f_ad_mod_timbre;
  t_float f_ad_mod_colour;
  t_float f_ad_mod_pitch;
  t_float f_ad_mod_fm;

  t_float f_paques;
  t_float f_transposition;
  t_float f_auto_trig;
  t_float f_meta_modulation;
  t_float f_vco_drift;
  t_float f_vco_flatten;
  t_float f_fm_cv_offset;


  // CLASS_MAINSIGNALIN  = in_sync;
  t_inlet*  x_in_pitch;
  t_inlet*  x_in_shape;
  t_outlet* x_out;
  t_outlet* x_out_shape;
  float pitch_offset=0.0f;


  braids::MacroOscillator osc;
  braids::Envelope envelope;
  braids::SignatureWaveshaper ws;
  braids::VcoJitterSource jitter_source;

  int16_t   previous_pitch;
  int16_t   previous_shape;
  uint16_t  gain_lp;
  bool      trigger_detected_flag;
  bool      trigger_flag;
  uint16_t  trigger_delay;

  t_int block_size;
  t_int block_count;
  t_int last_n;

  t_int buf_size;
  uint8_t* sync_buf; 
  int16_t* outint_buf;
} t_brds_tilde;


//define pure data methods
extern "C"  {
  t_int*  brds_tilde_render(t_int *w);
  void    brds_tilde_dsp(t_brds_tilde *x, t_signal **sp);
  void    brds_tilde_free(t_brds_tilde *x);
  void*   brds_tilde_new(t_floatarg f);
  void    brds_tilde_setup(void);

  void brds_tilde_pitch(t_brds_tilde *x, t_floatarg f);
  void brds_tilde_shape(t_brds_tilde *x, t_floatarg f);
  void brds_tilde_colour(t_brds_tilde *x, t_floatarg f);
  void brds_tilde_timbre(t_brds_tilde *x, t_floatarg f);
  void brds_tilde_trigger(t_brds_tilde *x, t_floatarg f);
}

static const char *algo_values[] = {
    "CSAW",
    "/\\-_",
    "//-_",
    "FOLD",
    "uuuu",
    "SUB-",
    "SUB/",
    "SYN-",
    "SYN/",
    "//x3",
    "-_x3",
    "/\\x3",
    "SIx3",
    "RING",
    "////",
    "//uu",
    "TOY*",
    "ZLPF",
    "ZPKF",
    "ZBPF",
    "ZHPF",
    "VOSM",
    "VOWL",
    "VFOF",
    "HARM",
    "FM  ",
    "FBFM",
    "WTFM",
    "PLUK",
    "BOWD",
    "BLOW",
    "FLUT",
    "BELL",
    "DRUM",
    "KICK",
    "CYMB",
    "SNAR",
    "WTBL",
    "WMAP",
    "WLIN",
    "WTx4",
    "NOIS",
    "TWNQ",
    "CLKN",
    "CLOU",
    "PRTC",
    "QPSK",
    "    ",
};


int getShape( float v) {
  float value = constrain(v, 0.0f, 1.0f);
  braids::MacroOscillatorShape ishape = static_cast<braids::MacroOscillatorShape>(value* (braids::MACRO_OSC_SHAPE_LAST - 1));
  return ishape;
}


// puredata methods implementation -start
t_int* brds_tilde_render(t_int *w)
{
  t_brds_tilde *x = (t_brds_tilde *)(w[1]);
  t_sample  *in_sync  =    (t_sample *)(w[2]);
  t_sample  *out =    (t_sample *)(w[3]);
  int n =  (int)(w[4]);

  // Determine block size
  if (n != x->last_n) {
    // Plaits uses a block size of 24 max
    if (n > 24) {
      int block_size = 24;
      while (n > 24 && n % block_size > 0) {
        block_size--;
      }
      x->block_size = block_size;
      x->block_count = n / block_size;
    } else {
      x->block_size = n;
      x->block_count = 1;
    }
    x->last_n = n;
  }

  if(x->block_size> x->buf_size) {
    x->buf_size = x->block_size;
    delete [] x->sync_buf;
    delete [] x->outint_buf;
    x->sync_buf = new uint8_t[x->buf_size];
    x->outint_buf= new int16_t[x->buf_size]; 
  }
    
  x->envelope.Update(int(x->f_ad_attack * 8.0f ) , int(x->f_ad_decay * 8.0f) );
  uint32_t ad_value = x->envelope.Render();

  int ishape = getShape(x->f_shape);

  if (x->f_paques) {
    x->osc.set_shape(braids::MACRO_OSC_SHAPE_QUESTION_MARK);
  } else if (x->f_meta_modulation) {
    int16_t shape = getShape(x->f_fm);
    shape -= x->f_fm_cv_offset;
    if (shape > x->previous_shape + 2 || shape < x->previous_shape - 2) {
      x->previous_shape = shape;
    } else {
      shape = x->previous_shape;
    }
    shape = braids::MACRO_OSC_SHAPE_LAST * shape >> 11;
    shape += ishape;
    if (shape >= braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META) {
      shape = braids::MACRO_OSC_SHAPE_LAST_ACCESSIBLE_FROM_META;
    } else if (shape <= 0) {
      shape = 0;
    }
    braids::MacroOscillatorShape osc_shape = static_cast<braids::MacroOscillatorShape>(shape);
    x->osc.set_shape(osc_shape);
  } else {
    x->osc.set_shape((braids::MacroOscillatorShape) (ishape));
  }
  
  int32_t timbre = int(x->f_timbre * 32767.0f);
  timbre += ad_value * x->f_ad_mod_timbre;
  int32_t colour = int(x->f_colour * 32767.0f);
  colour += ad_value * x->f_ad_mod_colour;
  x->osc.set_parameters(constrain(timbre,0,32767), constrain(colour,0,32767));


  int32_t pitch = x->f_pitch + x->pitch_offset;
  if (! x->f_meta_modulation) {
    pitch += x->f_fm;
  }

  // Check if the pitch has changed to cause an auto-retrigger
  int32_t pitch_delta = pitch - x->previous_pitch;
  if (x->f_auto_trig &&
      (pitch_delta >= 0x40 || -pitch_delta >= 0x40)) {
    x->trigger_detected_flag = true;
  }
  x->previous_pitch = pitch; 
  
  pitch += x->jitter_source.Render(x->f_vco_drift);
  pitch += ad_value * x->f_ad_mod_pitch;
  
  if (pitch > 16383) {
    pitch = 16383;
  } else if (pitch < 0) {
    pitch = 0;
  }
  
  if (x->f_vco_flatten) {
    pitch = braids::Interpolate88(braids::lut_vco_detune, pitch << 2);
  }
  x->osc.set_pitch(pitch + x->f_transposition);

  if (x->trigger_flag) {
    x->osc.Strike();
    x->envelope.Trigger(braids::ENV_SEGMENT_ATTACK);
    x->trigger_flag = false;
  }

  bool sync_zero = x->f_ad_mod_vca!=0  || x->f_ad_mod_timbre !=0 || x->f_ad_mod_colour !=0 || x->f_ad_mod_fm !=0; 

  for (int j = 0; j < x->block_count; j++) {
    for (int i = 0; i < x->block_size; i++) {
      if(sync_zero) x->sync_buf[i] = 0;
      else x->sync_buf[i] = in_sync[i + (x->block_size * j)] * (1<<8) ;
    }

    x->osc.Render(x->sync_buf, x->outint_buf, x->block_size);

    for (int i = 0; i < x->block_size; i++) {
      out[i + (x->block_size * j)] = x->outint_buf[i] / 65536.0f ;
    }
  }

   // Copy to DAC buffer with sample rate and bit reduction applied.
//   int16_t sample = 0;
//   size_t decimation_factor = decimation_factors[settings.data().sample_rate];
//   uint16_t bit_mask = bit_reduction_masks[settings.data().resolution];
//   int32_t gain = settings.GetValue(SETTING_AD_VCA) ? ad_value : 65535;
//   uint16_t signature = settings.signature() * settings.signature() * 4095;
//   for (size_t i = 0; i < kBlockSize; ++i) {
//     if ((i % decimation_factor) == 0) {
//       sample = render_buffer[i] & bit_mask;
//     }
//     sample = sample * gain_lp >> 16;
//     gain_lp += (gain - gain_lp) >> 4;
//     int16_t warped = ws.Transform(sample);
//     render_buffer[i] = Mix(sample, warped, signature);
//   }

  // bool trigger_detected = gate_input.raised();
  // sync_samples[playback_block][current_sample] = trigger_detected;
  // trigger_detected_flag = trigger_detected_flag | trigger_detected;
  
  // if (trigger_detected_flag) {
  //   trigger_delay = settings.trig_delay()
  //       ? (1 << settings.trig_delay()) : 0;
  //   ++trigger_delay;
  //   trigger_detected_flag = false;
  // }
  // if (trigger_delay) {
  //   --trigger_delay;
  //   if (trigger_delay == 0) {
  //     trigger_flag = true;
  //   }
  // }

  return (w + 5); // # args + 1
}

void brds_tilde_dsp(t_brds_tilde *x, t_signal **sp)
{
  // add the perform method, with all signal i/o
  dsp_add(brds_tilde_render, 4,
          x,
          sp[0]->s_vec, sp[1]->s_vec, // signal i/o (clockwise)
          sp[0]->s_n);
}

void brds_tilde_free(t_brds_tilde *x)
{
  delete [] x->sync_buf;
  delete [] x->outint_buf;
  inlet_free(x->x_in_shape);
  inlet_free(x->x_in_pitch);
  outlet_free(x->x_out);
  outlet_free(x->x_out_shape);
}

void *brds_tilde_new(t_floatarg f)
{
  t_brds_tilde *x = (t_brds_tilde *) pd_new(brds_tilde_class);

  x->previous_pitch = 0;
  x->previous_shape = 0;
  x->gain_lp = 0;
  x->trigger_detected_flag = false;
  x->trigger_flag = false;
  x->trigger_delay = 0;

  x->f_dummy = f;
  x->f_shape = 0.0f;
  x->f_pitch = 0.0f;
  x->f_trig = 0.0f;
  x->f_fm = 0.0f;
  x->f_modulation = 0.0f;
  x->f_colour = 0.0f;
  x->f_timbre = 0.0f;
  x->f_ad_attack = 0.0f;
  x->f_ad_decay = 0.0f;
  x->f_ad_mod_vca = 0.0f;
  x->f_ad_mod_timbre = 0.0f;
  x->f_ad_mod_colour = 0.0f;
  x->f_ad_mod_pitch = 0.0f;
  x->f_ad_mod_fm = 0.0f;
  x->f_paques = 0.0f;
  x->f_transposition = 0.0f;
  x->f_auto_trig = 0.0f;
  x->f_meta_modulation = 0.0f;
  x->f_vco_drift = 0.0f;
  x->f_vco_flatten = 0.0f;
  x->f_fm_cv_offset = 0.0f;

  x->x_in_shape  = floatinlet_new (&x->x_obj, &x->f_shape);
  x->x_in_pitch  = floatinlet_new (&x->x_obj, &x->f_pitch);
  x->x_out   = outlet_new(&x->x_obj, &s_signal);
  x->x_out_shape = outlet_new(&x->x_obj, &s_symbol);

  // plaits is limited to block size < 24, 
  // so this should never be need to increase
  x->buf_size = 48;
  x->sync_buf = new uint8_t[x->buf_size];
  x->outint_buf= new int16_t[x->buf_size];

  if(sys_getsr()!=48000.0f) {
      post("brds~.pd is designed for 96k, not %f, approximating pitch", sys_getsr());
      if(sys_getsr()==44100) {
          x->pitch_offset=193.0f;
      }
  }



  x->osc.Init();
  x->envelope.Init();
  x->jitter_source.Init();
  x->ws.Init(0x0000);

  return (void *)x;
}

void brds_tilde_pitch(t_brds_tilde *x, t_floatarg f)
{
  x->f_pitch = f;
}
void brds_tilde_shape(t_brds_tilde *x, t_floatarg f)
{
  x->f_shape = f;
  int shape = getShape(x->f_shape);
  outlet_symbol(x->x_out_shape, gensym(algo_values[shape]));
}
void brds_tilde_colour(t_brds_tilde *x, t_floatarg f)
{
  x->f_colour = f;
}
void brds_tilde_timbre(t_brds_tilde *x, t_floatarg f)
{
  x->f_timbre = f;
}
void brds_tilde_trigger(t_brds_tilde *x, t_floatarg f)
{
  x->trigger_flag = f >= 1;
}

void brds_tilde_setup(void) {
  brds_tilde_class = class_new(gensym("brds~"),
                                         (t_newmethod) brds_tilde_new,
                                         (t_method) brds_tilde_free,
                                         sizeof(t_brds_tilde),
                                         CLASS_DEFAULT,
                                         A_DEFFLOAT, A_NULL);

  class_addmethod(  brds_tilde_class,
                    (t_method)brds_tilde_dsp,
                    gensym("dsp"), A_NULL);
  CLASS_MAINSIGNALIN(brds_tilde_class, t_brds_tilde, f_dummy);


  class_addmethod(brds_tilde_class,
                  (t_method) brds_tilde_pitch, gensym("pitch"),
                  A_DEFFLOAT, A_NULL);
  class_addmethod(brds_tilde_class,
                  (t_method) brds_tilde_shape, gensym("shape"),
                  A_DEFFLOAT, A_NULL);
  class_addmethod(brds_tilde_class,
                  (t_method) brds_tilde_colour, gensym("colour"),
                  A_DEFFLOAT, A_NULL);
  class_addmethod(brds_tilde_class,
                  (t_method) brds_tilde_timbre, gensym("timbre"),
                  A_DEFFLOAT, A_NULL);
  class_addmethod(brds_tilde_class,
                  (t_method) brds_tilde_trigger, gensym("trigger"),
                  A_DEFFLOAT, A_NULL);
}
// puredata methods implementation - end

