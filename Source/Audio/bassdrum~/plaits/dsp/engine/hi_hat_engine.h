// Copyright 2016 Emilie Gillet.
// -----------------------------------------------------------------------------
//
// 808-style HH with two noise sources - one faithful to the original, the other
// more metallic.

#ifndef PLAITS_DSP_ENGINE_HI_HAT_ENGINE_H_
#define PLAITS_DSP_ENGINE_HI_HAT_ENGINE_H_

#include "plaits/dsp/engine/hi_hat.h"
#include "plaits/dsp/engine/engine.h"

namespace plaits {
  
class HiHatEngine final : public Engine {
 public:
  HiHatEngine() { }
  ~HiHatEngine() { }
  
  virtual void Init(stmlib::BufferAllocator* allocator);
  virtual void Reset();
  virtual void LoadUserData(const uint8_t* user_data) { }
  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped);

 private:
  HiHat<SquareNoise, SwingVCA, true, false> hi_hat_1_;
  HiHat<RingModNoise, LinearVCA, false, true> hi_hat_2_;
  
  float* temp_buffer_;
  
  DISALLOW_COPY_AND_ASSIGN(HiHatEngine);
};

}  // namespace plaits

#endif  // PLAITS_DSP_ENGINE_HI_HAT_ENGINE_H_
