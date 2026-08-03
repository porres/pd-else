// Copyright 2016 Emilie Gillet.
//
// 808 and synthetic snare drum generators.

#include "plaits/dsp/engine/snare_drum_engine.h"

#include <algorithm>

namespace plaits {

using namespace std;
using namespace stmlib;

void SnareDrumEngine::Init(BufferAllocator* allocator) {
  analog_snare_drum_.Init();
  synthetic_snare_drum_.Init();
}

void SnareDrumEngine::Reset() {
  
}

void SnareDrumEngine::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  const float f0 = NoteToFrequency(parameters.note);
  
  analog_snare_drum_.Render(
      parameters.trigger & TRIGGER_UNPATCHED,
      parameters.trigger & TRIGGER_RISING_EDGE,
      parameters.accent,
      f0,
      parameters.timbre,
      parameters.morph,
      parameters.harmonics,
      out,
      size);
  
  synthetic_snare_drum_.Render(
      parameters.trigger & TRIGGER_UNPATCHED,
      parameters.trigger & TRIGGER_RISING_EDGE,
      parameters.accent,
      f0,
      parameters.timbre,
      parameters.morph,
      parameters.harmonics,
      aux,
      size);
}

}  // namespace plaits
