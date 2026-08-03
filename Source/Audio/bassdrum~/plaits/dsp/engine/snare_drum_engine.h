// Copyright 2016 Emilie Gillet.
//
// -----------------------------------------------------------------------------
//
// 808 and synthetic snare drum generators.

#ifndef PLAITS_DSP_ENGINE_SNARE_DRUM_ENGINE_H_
#define PLAITS_DSP_ENGINE_SNARE_DRUM_ENGINE_H_

#include "plaits/dsp/engine/analog_snare_drum.h"
#include "plaits/dsp/engine/synthetic_snare_drum.h"
#include "plaits/dsp/engine/engine.h"

namespace plaits {
  
class SnareDrumEngine final : public Engine {
 public:
  SnareDrumEngine() { }
  ~SnareDrumEngine() { }
  
  virtual void Init(stmlib::BufferAllocator* allocator);
  virtual void Reset();
  virtual void LoadUserData(const uint8_t* user_data) { }
  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  AnalogSnareDrum analog_snare_drum_;
  SyntheticSnareDrum synthetic_snare_drum_;
  
  DISALLOW_COPY_AND_ASSIGN(SnareDrumEngine);
};

}  // namespace plaits

#endif  // PLAITS_DSP_ENGINE_SNARE_DRUM_ENGINE_H_
