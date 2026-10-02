// Based on Plaits' voice.h / voice.cc (Copyright 2016 Emilie Gillet, MIT).
//
// Trimmed to the bass drum engine only. Everything that the bass drum can
// never reach in the original Voice::Render has been removed:
//  - engine selection / quantizer / other engines
//  - LPG (the bass drum is registered with already_enveloped = true, so the
//    original always takes the lpg_bypass path)
//  - limiter (bass drum out_gain/aux_gain = 0.8 > 0, so the limiter never runs)
//  - frequency/timbre/morph/harmonics CV inputs (unconnected = 0 in plaits~)
// Trigger is always "patched" (no sustain mode).
//
// Every remaining expression is kept as in the original.

#ifndef PLAITS_DSP_VOICE_H_
#define PLAITS_DSP_VOICE_H_

#include <algorithm>
#include <math.h>

#include "stmlib.h"

#include "bass_drum.h"
#include "envelope.h"

namespace plaits {

//const int kMaxTriggerDelay = 8;
//const int kTriggerDelay = 5;

// Original ChannelPostProcessor, lpg_bypass branch only.
class ChannelPostProcessor {
 public:
  ChannelPostProcessor() { }
  ~ChannelPostProcessor() { }

  void Process(float gain, float* in, short* out, size_t size, size_t stride) {
    const float post_gain = (gain < 0.0f ? 1.0f : gain) * -32767.0f;
    while (size--) {
      *out = stmlib::Clip16(1 + static_cast<int32_t>(*in++ * post_gain));
      out += stride;
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChannelPostProcessor);
};

struct Patch {
  float note;
  float harmonics;
  float timbre;
  float morph;
  float frequency_modulation_amount;
  float timbre_modulation_amount;
  float morph_modulation_amount;
  float decay;
};

struct Modulations {
  float trigger;
  float level;
  bool level_patched;
};

class Voice {
 public:
  Voice() {}
  ~Voice() {}

  struct Frame {
    short out;
    short aux;
  };

  void Init();
  void Render(
      const Patch& patch,
      const Modulations& modulations,
      Frame* frames,
      size_t size);

 private:
  inline float ApplyModulations(
      float base_value,
      float modulation_amount,
      bool use_external_modulation,
      float external_modulation,
      bool use_internal_envelope,
      float envelope,
      float default_internal_modulation,
      float minimum_value,
      float maximum_value) {
    float value = base_value;
    modulation_amount *= std::max(fabsf(modulation_amount) - 0.05f, 0.05f);
    modulation_amount *= 1.05f;

    float modulation = use_external_modulation
        ? external_modulation
        : (use_internal_envelope ? envelope : default_internal_modulation);
    value += modulation_amount * modulation;
    CONSTRAIN(value, minimum_value, maximum_value);
    return value;
  }

  BassDrumEngine bass_drum_engine_;

  bool trigger_state_;

  DecayEnvelope decay_envelope_;

//  stmlib::DelayLine<float, kMaxTriggerDelay> trigger_delay_;

  ChannelPostProcessor out_post_processor_;
  ChannelPostProcessor aux_post_processor_;

  float out_buffer_[kMaxBlockSize];
  float aux_buffer_[kMaxBlockSize];

  DISALLOW_COPY_AND_ASSIGN(Voice);
};

}  // namespace plaits

#endif  // PLAITS_DSP_VOICE_H_
