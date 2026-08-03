// Copyright 2016 Emilie Gillet.
//
// -----------------------------------------------------------------------------
//
// 808-style HH with two noise sources - one faithful to the original, the other
// more metallic.

#include "plaits/dsp/engine/hi_hat_engine.h"

namespace plaits {

using namespace stmlib;

void HiHatEngine::Init(BufferAllocator* allocator) {
  hi_hat_1_.Init();
  hi_hat_2_.Init();
  temp_buffer_ = allocator->Allocate<float>(kMaxBlockSize * 2);
}

void HiHatEngine::Reset() {
  
}

void HiHatEngine::Render(
    const EngineParameters& parameters,
    float* out,
    float* aux,
    size_t size,
    bool* already_enveloped) {
  const float f0 = NoteToFrequency(parameters.note);
  
  hi_hat_1_.Render(
      parameters.trigger & TRIGGER_UNPATCHED,
      parameters.trigger & TRIGGER_RISING_EDGE,
      parameters.accent,
      f0,
      parameters.timbre,
      parameters.morph,
      parameters.harmonics,
      temp_buffer_,
      temp_buffer_ + size,
      out,
      size);
  
  hi_hat_2_.Render(
      parameters.trigger & TRIGGER_UNPATCHED,
      parameters.trigger & TRIGGER_RISING_EDGE,
      parameters.accent,
      f0,
      parameters.timbre,
      parameters.morph,
      parameters.harmonics,
      temp_buffer_,
      temp_buffer_ + size,
      aux,
      size);
}

}  // namespace plaits
