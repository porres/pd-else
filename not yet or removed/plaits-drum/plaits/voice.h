// Copyright 2016 Emilie Gillet.
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// Main synthesis voice.
//
// This file merges (in order):
//   - envelope.h (DecayEnvelope)
//   - voice.h    (ChannelPostProcessor, Patch, Modulations, Voice declaration)
//   - voice.cc   (Voice::FreeEngines / Init / Render implementations, folded
//                 inline into the Voice class body)

#ifndef PLAITS_DSP_VOICE_H_
#define PLAITS_DSP_VOICE_H_

#include "plaits/stmlib.h"
#include "plaits/engine/bass_drum.h"
#include "plaits/engine/engine.h"
#include "plaits/engine/hi_hat.h"
#include "plaits/engine/snare_drum.h"

namespace plaits {

// ---------------------------------------------------------------------------
// From plaits/envelope.h
// Internal envelope.
// ---------------------------------------------------------------------------

class DecayEnvelope {
 public:
  DecayEnvelope() { }
  ~DecayEnvelope() { }

  inline void Init() {
    value_ = 0.0f;
  }

  inline void Trigger() {
    value_ = 1.0f;
  }

  inline void Process(float decay) {
    value_ *= (1.0f - decay);
  }

  inline float value() const { return value_; }

 private:
  float value_;

  DISALLOW_COPY_AND_ASSIGN(DecayEnvelope);
};

const int kMaxEngines = 4;

class ChannelPostProcessor {
 public:
  ChannelPostProcessor() { }
  ~ChannelPostProcessor() { }

  void Init() {
    Reset();
  }

  void Reset() {
    limiter_.Init();
  }

  void Process(
      float gain,
      bool bypass_lpg,
      float low_pass_gate_gain,
      float low_pass_gate_frequency,
      float low_pass_gate_hf_bleed,
      float* in,
      short* out,
      size_t size,
      size_t stride) {
    if (gain < 0.0f) {
      limiter_.Process(-gain, in, size);
    }
    const float post_gain = (gain < 0.0f ? 1.0f : gain) * -32767.0f;

    while (size--) {
      *out = stmlib::Clip16(1 + static_cast<int32_t>(*in++ * post_gain));
      out += stride;
    }
  }

 private:
  stmlib::Limiter limiter_;

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
  int engine;
  float decay;
  float lpg_colour;
};

struct Modulations {
  float engine;
  float note;
  float frequency;
  float harmonics;
  float timbre;
  float morph;
  float trigger;
  float level;
  bool frequency_patched;
  bool timbre_patched;
  bool morph_patched;
  bool trigger_patched;
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

  void Init(stmlib::BufferAllocator* allocator) {
    bass_drum_engine_ = new BassDrumEngine();
    snare_drum_engine_ = new SnareDrumEngine();
    hi_hat_engine_ = new HiHatEngine();

    engines_.Init();

    engines_.RegisterInstance(bass_drum_engine_, true, 0.8f, 0.8f);
    engines_.RegisterInstance(snare_drum_engine_, true, 0.8f, 0.8f);
    engines_.RegisterInstance(hi_hat_engine_, true, 0.8f, 0.8f);

    for (int i = 0; i < engines_.size(); ++i) {
      // All engines will share the same RAM space.
      allocator->Free();
      engines_.get(i)->Init(allocator);
    }

    engine_quantizer_.Init(engines_.size(), 0.05f, true);
    previous_engine_index_ = -1;
    engine_cv_ = 0.0f;

    out_post_processor_.Init();
    aux_post_processor_.Init();

    decay_envelope_.Init();

    trigger_state_ = false;
    previous_note_ = 0.0f;
  }

  void FreeEngines() {
    delete bass_drum_engine_;
    delete snare_drum_engine_;
    delete hi_hat_engine_;
  }

  void Render(
      const Patch& patch,
      const Modulations& modulations,
      Frame* frames,
      size_t size) {
    // Trigger, internal envelope.

    float trigger_value = modulations.trigger;

    bool previous_trigger_state = trigger_state_;
    if (!previous_trigger_state) {
      if (trigger_value > 0.0f) {
        trigger_state_ = true;
        decay_envelope_.Trigger();
        engine_cv_ = modulations.engine;
      }
    } else {
      if (trigger_value <= 0.0f) {
        trigger_state_ = false;
      }
    }

    // Engine selection.
    int engine_index = engine_quantizer_.Process(patch.engine, engine_cv_);

    Engine* e = engines_.get(engine_index);

    if (engine_index != previous_engine_index_) {
      e->Reset();
      out_post_processor_.Reset();
      previous_engine_index_ = engine_index;
    }
    EngineParameters p;

    bool rising_edge = trigger_state_ && !previous_trigger_state;
    float note = (modulations.note + previous_note_) * 0.5f;
    previous_note_ = modulations.note;
    const PostProcessingSettings& pp_s = e->post_processing_settings;

    p.trigger = (rising_edge ? TRIGGER_RISING_EDGE : TRIGGER_LOW) | \
        (trigger_state_ ? TRIGGER_HIGH : TRIGGER_LOW);

    const float short_decay = (200.0f * kBlockSize) / kSampleRate *
        stmlib::SemitonesToRatio(-96.0f * patch.decay);

    decay_envelope_.Process(short_decay * 2.0f);

    float compressed_level = 1.3f * modulations.level / (0.3f + fabsf(modulations.level));
    CONSTRAIN(compressed_level, 0.0f, 1.0f);
    p.accent = modulations.level_patched ? compressed_level : 0.8f;

    bool use_internal_envelope = 1;

    // Actual synthesis parameters.

    p.harmonics = patch.harmonics + modulations.harmonics;
    CONSTRAIN(p.harmonics, 0.0f, 1.0f);

    float internal_envelope_amplitude = 1.0f;
    float internal_envelope_amplitude_timbre = 1.0f;

    p.note = ApplyModulations(patch.note + note,
        patch.frequency_modulation_amount,
        modulations.frequency_patched,
        modulations.frequency,
        use_internal_envelope,
        internal_envelope_amplitude * \
        decay_envelope_.value() * decay_envelope_.value() * 48.0f,
        1.0f,
        -119.0f,
        120.0f);

    p.timbre = ApplyModulations(patch.timbre,
        patch.timbre_modulation_amount,
        modulations.timbre_patched,
        modulations.timbre,
        use_internal_envelope,
        internal_envelope_amplitude_timbre * decay_envelope_.value(),
        0.0f,
        0.0f,
        1.0f);

    p.morph = ApplyModulations(patch.morph,
        patch.morph_modulation_amount,
        modulations.morph_patched,
        modulations.morph,
        use_internal_envelope,
        internal_envelope_amplitude * decay_envelope_.value(),
        0.0f,
        0.0f,
        1.0f);

    bool already_enveloped = pp_s.already_enveloped;
    e->Render(p, out_buffer_, aux_buffer_, size, &already_enveloped);

    out_post_processor_.Process(pp_s.out_gain,
        1, 0, 0, 0,
        out_buffer_,
        &frames->out,
        size,
        2);

    aux_post_processor_.Process(pp_s.aux_gain,
        1, 0, 0, 0,
        aux_buffer_,
        &frames->aux,
        size,
        2);
  }

  inline int active_engine() const { return previous_engine_index_; }

 private:
  inline float ApplyModulations(float base_value,
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
        ? external_modulation : (use_internal_envelope ? envelope : default_internal_modulation);
    value += modulation_amount * modulation;
    CONSTRAIN(value, minimum_value, maximum_value);
    return value;
  }

  BassDrumEngine* bass_drum_engine_;
  HiHatEngine* hi_hat_engine_;
  SnareDrumEngine* snare_drum_engine_;

  stmlib::HysteresisQuantizer2 engine_quantizer_;

  int previous_engine_index_;
  float engine_cv_;

  float previous_note_;
  bool trigger_state_;

  DecayEnvelope decay_envelope_;

  ChannelPostProcessor out_post_processor_;
  ChannelPostProcessor aux_post_processor_;

  EngineRegistry<kMaxEngines> engines_;

  float out_buffer_[kMaxBlockSize];
  float aux_buffer_[kMaxBlockSize];

  DISALLOW_COPY_AND_ASSIGN(Voice);
};

}  // namespace plaits

#endif  // PLAITS_DSP_VOICE_H_
