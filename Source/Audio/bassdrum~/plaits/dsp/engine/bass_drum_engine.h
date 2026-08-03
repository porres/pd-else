// Copyright 2016 Emilie Gillet.
//
// 808 and synthetic bass drum generators.

#ifndef PLAITS_DSP_ENGINE_BASS_DRUM_ENGINE_H_
#define PLAITS_DSP_ENGINE_BASS_DRUM_ENGINE_H_

#include "plaits/dsp/engine/analog_bass_drum.h"
#include "plaits/dsp/engine/synthetic_bass_drum.h"
#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/fx/overdrive.h"

namespace plaits {
  
class BassDrumEngine final : public Engine {
 public:
  BassDrumEngine() { }
  ~BassDrumEngine() { }
  
  virtual void Init(stmlib::BufferAllocator* allocator);
  virtual void Reset();
  virtual void LoadUserData(const uint8_t* user_data) { }
  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  AnalogBassDrum analog_bass_drum_;
  SyntheticBassDrum synthetic_bass_drum_;
  
  Overdrive overdrive_;
  
  DISALLOW_COPY_AND_ASSIGN(BassDrumEngine);
};

}  // namespace plaits

#endif  // PLAITS_DSP_ENGINE_BASS_DRUM_ENGINE_H_
