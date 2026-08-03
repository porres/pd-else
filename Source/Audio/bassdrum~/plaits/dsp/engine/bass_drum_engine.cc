// Copyright 2016 Emilie Gillet.
//
// 808 and synthetic bass drum generators.

#include "plaits/dsp/engine/bass_drum_engine.h"

namespace plaits {

using namespace std;
using namespace stmlib;

void BassDrumEngine::Init(BufferAllocator* allocator) {
  analog_bass_drum_.Init();
  synthetic_bass_drum_.Init();
  overdrive_.Init();
}

void BassDrumEngine::Reset() {
  
}

void BassDrumEngine::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  const float f0 = NoteToFrequency(parameters.note);
  
  const float attack_fm_amount = min(parameters.harmonics * 4.0f, 1.0f);
  const float self_fm_amount = max(min(parameters.harmonics * 4.0f - 1.0f, 1.0f), 0.0f);
  const float drive = max(parameters.harmonics * 2.0f - 1.0f, 0.0f) * \
      max(1.0f - 16.0f * f0, 0.0f);
  
  const bool sustain = parameters.trigger & TRIGGER_UNPATCHED;
  
  analog_bass_drum_.Render(
      sustain,
      parameters.trigger & TRIGGER_RISING_EDGE,
      parameters.accent,
      f0,
      parameters.timbre,
      parameters.morph,
      attack_fm_amount,
      self_fm_amount,
      out,
      size);

  overdrive_.Process(
      0.5f + 0.5f * drive,
      out,
      size);

  synthetic_bass_drum_.Render(
      sustain,
      parameters.trigger & TRIGGER_RISING_EDGE,
      parameters.accent,
      f0,
      parameters.timbre,
      parameters.morph,
      sustain
          ? parameters.harmonics
          : 0.4f - 0.25f * parameters.morph * parameters.morph,
      min(parameters.harmonics * 2.0f, 1.0f),
      max(parameters.harmonics * 2.0f - 1.0f, 0.0f),
      aux,
      size);
}

}  // namespace plaits
