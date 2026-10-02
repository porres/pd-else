// Based on Plaits' voice.cc (Copyright 2016 Emilie Gillet, MIT).
// Bass drum only, trigger mode only. See voice.h for what was removed.

#include "voice.h"

namespace plaits {

using namespace std;
using namespace stmlib;

void Voice::Init() {
  // Same as EngineRegistry::RegisterInstance(bass_drum_engine_, true, 0.8f, 0.8f)
  PostProcessingSettings* s = &bass_drum_engine_.post_processing_settings;
  s->already_enveloped = true;
  s->out_gain = 0.8f;
  s->aux_gain = 0.8f;

  bass_drum_engine_.Init(NULL);  // The bass drum does not use the allocator.
  bass_drum_engine_.Reset();

  decay_envelope_.Init();

  trigger_state_ = false;

//  trigger_delay_.Init();
}

void Voice::Render(
    const Patch& patch,
    const Modulations& modulations,
    Frame* frames,
    size_t size) {
  // Delay trigger by 1ms to deal with sequencers or MIDI interfaces whose
  // CV out lags behind the GATE out.
//  trigger_delay_.Write(modulations.trigger);
//  float trigger_value = trigger_delay_.Read(static_cast<size_t>(kTriggerDelay));
    
  float trigger_value = modulations.trigger;
    
  bool previous_trigger_state = trigger_state_;
  if (!previous_trigger_state) {
    if (trigger_value > 0.3f) {
      trigger_state_ = true;
      decay_envelope_.Trigger();
    }
  } else {
    if (trigger_value < 0.1f) {
      trigger_state_ = false;
    }
  }

  BassDrumEngine* e = &bass_drum_engine_;

  EngineParameters p;

  bool rising_edge = trigger_state_ && !previous_trigger_state;
  const PostProcessingSettings& pp_s = e->post_processing_settings;

  p.trigger = (rising_edge ? TRIGGER_RISING_EDGE : TRIGGER_LOW) | \
              (trigger_state_ ? TRIGGER_HIGH : TRIGGER_LOW);

  const float short_decay = (200.0f * kBlockSize) / kSampleRate *
      SemitonesToRatio(-96.0f * patch.decay);

  decay_envelope_.Process(short_decay * 2.0f);

  float compressed_level = 1.3f * modulations.level / (0.3f + fabsf(modulations.level));
  CONSTRAIN(compressed_level, 0.0f, 1.0f);
  p.accent = modulations.level_patched ? compressed_level : 0.8f;

  const bool use_internal_envelope = true;  // trigger is always patched.

  p.harmonics = patch.harmonics;
  CONSTRAIN(p.harmonics, 0.0f, 1.0f);

  float internal_envelope_amplitude = 1.0f;
  float internal_envelope_amplitude_timbre = 1.0f;

  p.note = ApplyModulations(
      patch.note,
      patch.frequency_modulation_amount,
      false,
      0.0f,
      use_internal_envelope,
      internal_envelope_amplitude * \
          decay_envelope_.value() * decay_envelope_.value() * 48.0f,
      1.0f,
      -119.0f,
      120.0f);

  p.timbre = ApplyModulations(
      patch.timbre,
      patch.timbre_modulation_amount,
      false,
      0.0f,
      use_internal_envelope,
      internal_envelope_amplitude_timbre * decay_envelope_.value(),
      0.0f,
      0.0f,
      1.0f);

  p.morph = ApplyModulations(
      patch.morph,
      patch.morph_modulation_amount,
      false,
      0.0f,
      use_internal_envelope,
      internal_envelope_amplitude * decay_envelope_.value(),
      0.0f,
      0.0f,
      1.0f);

  bool already_enveloped = pp_s.already_enveloped;
  e->Render(p, out_buffer_, aux_buffer_, size, &already_enveloped);

  // lpg_bypass is always true for the bass drum (already_enveloped).
  out_post_processor_.Process(pp_s.out_gain, out_buffer_, &frames->out, size, 2);
  aux_post_processor_.Process(pp_s.aux_gain, aux_buffer_, &frames->aux, size, 2);
}

}  // namespace plaits
