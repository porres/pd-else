// Copyright 2016 Emilie Gillet.
//
// Simple sine oscillator (wavetable) + fast sine oscillator (magic circle).
//
// The fast implementation might glitch a bit under heavy modulations of the
// frequency.

#ifndef PLAITS_DSP_OSCILLATOR_SINE_OSCILLATOR_H_
#define PLAITS_DSP_OSCILLATOR_SINE_OSCILLATOR_H_

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "plaits/resources.h"

namespace plaits {
  
const float kSineLUTSize = 512.0f;
const size_t kSineLUTQuadrature = 128;
const size_t kSineLUTBits = 9;

// Safe for phase >= 0.0f, will wrap.
inline float Sine(float phase) {
  return stmlib::InterpolateWrap(lut_sine, phase, kSineLUTSize);
}

// Potentially unsafe, if phase >= 1.25.
inline float SineNoWrap(float phase) {
  return stmlib::Interpolate(lut_sine, phase, kSineLUTSize);
}

class SineOscillator {
 public:
  SineOscillator() { }
  ~SineOscillator() { }

  void Init() {
    phase_ = 0.0f;
    frequency_ = 0.0f;
    amplitude_ = 0.0f;
  }
  
  inline float Next(float frequency) {
    if (frequency >= 0.5f) {
      frequency = 0.5f;
    }
    
    phase_ += frequency;
    if (phase_ >= 1.0f) {
      phase_ -= 1.0f;
    }
    
    return SineNoWrap(phase_);
  }
  
  inline void Next(float frequency, float amplitude, float* sin, float* cos) {
    if (frequency >= 0.5f) {
      frequency = 0.5f;
    }
    
    phase_ += frequency;
    if (phase_ >= 1.0f) {
      phase_ -= 1.0f;
    }
    
    *sin = amplitude * SineNoWrap(phase_);
    *cos = amplitude * SineNoWrap(phase_ + 0.25f);
  }
  
  void Render(float frequency, float amplitude, float* out, size_t size) {
    RenderInternal<true>(frequency, amplitude, out, size);
  }
  
  void Render(float frequency, float* out, size_t size) {
    RenderInternal<false>(frequency, 1.0f, out, size);
  }

 private:
  template<bool additive>
  void RenderInternal(
      float frequency, float amplitude, float* out, size_t size) {
    if (frequency >= 0.5f) {
      frequency = 0.5f;
    }
    stmlib::ParameterInterpolator fm(&frequency_, frequency, size);
    stmlib::ParameterInterpolator am(&amplitude_, amplitude, size);

    while (size--) {
      phase_ += fm.Next();
      if (phase_ >= 1.0f) {
        phase_ -= 1.0f;
      }
      float s = SineNoWrap(phase_);
      if (additive) {
        *out++ += am.Next() * s;
      } else {
        *out++ = s;
      }
    }
  }
  
  // Oscillator state.
  float phase_;

  // For interpolation of parameters.
  float frequency_;
  float amplitude_;
  
  DISALLOW_COPY_AND_ASSIGN(SineOscillator);
};
  
}  // namespace plaits

#endif  // PLAITS_DSP_OSCILLATOR_SINE_OSCILLATOR_H_
