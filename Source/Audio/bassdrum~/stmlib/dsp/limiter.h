// Copyright 2015 Emilie Gillet.
//
// Limiter.

#ifndef STMLIB_DSP_LIMITER_H_
#define STMLIB_DSP_LIMITER_H_

#include "stmlib/stmlib.h"

#include <algorithm>

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/filter.h"

namespace stmlib {

class Limiter {
 public:
  Limiter() { }
  ~Limiter() { }

  void Init() {
    peak_ = 0.5f;
  }

  void Process(float pre_gain, float* in_out, size_t size) {
    while (size--) {
      float s = *in_out * pre_gain;
      SLOPE(peak_, fabsf(s), 0.05f, 0.00002f);
      float gain = (peak_ <= 1.0f ? 1.0f : 1.0f / peak_);
      *in_out++ = s * gain * 0.8f;
    }
  }

 private:
  float peak_;

  DISALLOW_COPY_AND_ASSIGN(Limiter);
};

}  // namespace stmlib

#endif  // STMLIB_DSP_LIMITER_H_
