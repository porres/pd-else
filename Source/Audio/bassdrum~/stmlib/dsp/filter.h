// Copyright 2014 Emilie Gillet.
//
// Zero-delay-feedback filters (one pole and SVF).
// Naive SVF.

#ifndef STMLIB_DSP_FILTER_H_
#define STMLIB_DSP_FILTER_H_

#include "stmlib/stmlib.h"

#define _USE_MATH_DEFINES
#include <math.h>
#include <algorithm>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace stmlib {

enum FilterMode {
  FILTER_MODE_LOW_PASS,
  FILTER_MODE_BAND_PASS,
  FILTER_MODE_BAND_PASS_NORMALIZED,
  FILTER_MODE_HIGH_PASS
};

enum FrequencyApproximation {
  FREQUENCY_EXACT,
  FREQUENCY_ACCURATE,
  FREQUENCY_FAST,
  FREQUENCY_DIRTY
};

#define M_PI_F float(M_PI)
#define M_PI_POW_2 M_PI * M_PI
#define M_PI_POW_3 M_PI_POW_2 * M_PI
#define M_PI_POW_5 M_PI_POW_3 * M_PI_POW_2
#define M_PI_POW_7 M_PI_POW_5 * M_PI_POW_2
#define M_PI_POW_9 M_PI_POW_7 * M_PI_POW_2
#define M_PI_POW_11 M_PI_POW_9 * M_PI_POW_2

class OnePole {
 public:
  OnePole() { }
  ~OnePole() { }
  
  void Init() {
    set_f<FREQUENCY_DIRTY>(0.01f);
    Reset();
  }
  
  void Reset() {
    state_ = 0.0f;
  }
  
  template<FrequencyApproximation approximation>
  static inline float tan(float f) {
    if (approximation == FREQUENCY_EXACT) {
      // Clip coefficient to about 100.
      f = f < 0.497f ? f : 0.497f;
      return tanf(M_PI_F * f);
    } else if (approximation == FREQUENCY_DIRTY) {
      // Optimized for frequencies below 8kHz.
      const float a = 3.736e-01f * M_PI_POW_3;
      return f * (M_PI_F + a * f * f);
    } else if (approximation == FREQUENCY_FAST) {
      // The usual tangent approximation uses 3.1755e-01 and 2.033e-01, but
      // the coefficients used here are optimized to minimize error for the
      // 16Hz to 16kHz range, with a sample rate of 48kHz.
      const float a = 3.260e-01f * M_PI_POW_3;
      const float b = 1.823e-01f * M_PI_POW_5;
      float f2 = f * f;
      return f * (M_PI_F + f2 * (a + b * f2));
    } else if (approximation == FREQUENCY_ACCURATE) {
      // These coefficients don't need to be tweaked for the audio range.
      const float a = 3.333314036e-01f * M_PI_POW_3;
      const float b = 1.333923995e-01f * M_PI_POW_5;
      const float c = 5.33740603e-02f * M_PI_POW_7;
      const float d = 2.900525e-03f * M_PI_POW_9;
      const float e = 9.5168091e-03f * M_PI_POW_11;
      float f2 = f * f;
      return f * (M_PI_F + f2 * (a + f2 * (b + f2 * (c + f2 * (d + f2 * e)))));
    }
  }
  
  // Set frequency and resonance from true units. Various approximations
  // are available to avoid the cost of tanf.
  template<FrequencyApproximation approximation>
  inline void set_f(float f) {
    g_ = tan<approximation>(f);
    gi_ = 1.0f / (1.0f + g_);
  }
  
  template<FilterMode mode>
  inline float Process(float in) {
    float lp;
    lp = (g_ * in + state_) * gi_;
    state_ = g_ * (in - lp) + lp;

    if (mode == FILTER_MODE_LOW_PASS) {
      return lp;
    } else if (mode == FILTER_MODE_HIGH_PASS) {
      return in - lp;
    } else {
      return 0.0f;
    }
  }
  
  template<FilterMode mode>
  inline void Process(float* in_out, size_t size) {
    while (size--) {
      *in_out = Process<mode>(*in_out);
      ++in_out;
    }
  }
  
 private:
  float g_;
  float gi_;
  float state_;
  
  DISALLOW_COPY_AND_ASSIGN(OnePole);
};

class Svf {
 public:
  Svf() { }
  ~Svf() { }
  
  void Init() {
    set_f_q<FREQUENCY_DIRTY>(0.01f, 100.0f);
    Reset();
  }
  
  void Reset() {
    state_1_ = state_2_ = 0.0f;
  }
  
  // Copy settings from another filter.
  inline void set(const Svf& f) {
    g_ = f.g();
    r_ = f.r();
    h_ = f.h();
  }

  // Set all parameters from LUT.
  inline void set_g_r_h(float g, float r, float h) {
    g_ = g;
    r_ = r;
    h_ = h;
  }
  
  // Set frequency and resonance coefficients from LUT, adjust remaining
  // parameter.
  inline void set_g_r(float g, float r) {
    g_ = g;
    r_ = r;
    h_ = 1.0f / (1.0f + r_ * g_ + g_ * g_);
  }

  // Set frequency from LUT, resonance in true units, adjust the rest.
  inline void set_g_q(float g, float resonance) {
    g_ = g;
    r_ = 1.0f / resonance;
    h_ = 1.0f / (1.0f + r_ * g_ + g_ * g_);
  }

  // Set frequency and resonance from true units. Various approximations
  // are available to avoid the cost of tanf.
  template<FrequencyApproximation approximation>
  inline void set_f_q(float f, float resonance) {
    g_ = OnePole::tan<approximation>(f);
    r_ = 1.0f / resonance;
    h_ = 1.0f / (1.0f + r_ * g_ + g_ * g_);
  }
  
  template<FilterMode mode>
  inline float Process(float in) {
    float hp, bp, lp;
    hp = (in - r_ * state_1_ - g_ * state_1_ - state_2_) * h_;
    bp = g_ * hp + state_1_;
    state_1_ = g_ * hp + bp;
    lp = g_ * bp + state_2_;
    state_2_ = g_ * bp + lp;
    
    if (mode == FILTER_MODE_LOW_PASS) {
      return lp;
    } else if (mode == FILTER_MODE_BAND_PASS) {
      return bp;
    } else if (mode == FILTER_MODE_BAND_PASS_NORMALIZED) {
      return bp * r_;
    } else if (mode == FILTER_MODE_HIGH_PASS) {
      return hp;
    }
  }
  
  template<FilterMode mode_1, FilterMode mode_2>
  inline void Process(float in, float* out_1, float* out_2) {
    float hp, bp, lp;
    hp = (in - r_ * state_1_ - g_ * state_1_ - state_2_) * h_;
    bp = g_ * hp + state_1_;
    state_1_ = g_ * hp + bp;
    lp = g_ * bp + state_2_;
    state_2_ = g_ * bp + lp;
    
    if (mode_1 == FILTER_MODE_LOW_PASS) {
      *out_1 = lp;
    } else if (mode_1 == FILTER_MODE_BAND_PASS) {
      *out_1 = bp;
    } else if (mode_1 == FILTER_MODE_BAND_PASS_NORMALIZED) {
      *out_1 = bp * r_;
    } else if (mode_1 == FILTER_MODE_HIGH_PASS) {
      *out_1 = hp;
    }

    if (mode_2 == FILTER_MODE_LOW_PASS) {
      *out_2 = lp;
    } else if (mode_2 == FILTER_MODE_BAND_PASS) {
      *out_2 = bp;
    } else if (mode_2 == FILTER_MODE_BAND_PASS_NORMALIZED) {
      *out_2 = bp * r_;
    } else if (mode_2 == FILTER_MODE_HIGH_PASS) {
      *out_2 = hp;
    }
  }
  
  template<FilterMode mode>
  inline void Process(const float* in, float* out, size_t size) {
    float hp, bp, lp;
    float state_1 = state_1_;
    float state_2 = state_2_;
    
    while (size--) {
      hp = (*in - r_ * state_1 - g_ * state_1 - state_2) * h_;
      bp = g_ * hp + state_1;
      state_1 = g_ * hp + bp;
      lp = g_ * bp + state_2;
      state_2 = g_ * bp + lp;
    
      float value;
      if (mode == FILTER_MODE_LOW_PASS) {
        value = lp;
      } else if (mode == FILTER_MODE_BAND_PASS) {
        value = bp;
      } else if (mode == FILTER_MODE_BAND_PASS_NORMALIZED) {
        value = bp * r_;
      } else if (mode == FILTER_MODE_HIGH_PASS) {
        value = hp;
      }
      
      *out = value;
      ++out;
      ++in;
    }
    state_1_ = state_1;
    state_2_ = state_2;
  }
  
  template<FilterMode mode>
  inline void ProcessAdd(const float* in, float* out, size_t size, float gain) {
    float hp, bp, lp;
    float state_1 = state_1_;
    float state_2 = state_2_;
    
    while (size--) {
      hp = (*in - r_ * state_1 - g_ * state_1 - state_2) * h_;
      bp = g_ * hp + state_1;
      state_1 = g_ * hp + bp;
      lp = g_ * bp + state_2;
      state_2 = g_ * bp + lp;
    
      float value;
      if (mode == FILTER_MODE_LOW_PASS) {
        value = lp;
      } else if (mode == FILTER_MODE_BAND_PASS) {
        value = bp;
      } else if (mode == FILTER_MODE_BAND_PASS_NORMALIZED) {
        value = bp * r_;
      } else if (mode == FILTER_MODE_HIGH_PASS) {
        value = hp;
      }
      
      *out += gain * value;
      ++out;
      ++in;
    }
    state_1_ = state_1;
    state_2_ = state_2;
  }
  
  template<FilterMode mode>
  inline void Process(const float* in, float* out, size_t size, size_t stride) {
    float hp, bp, lp;
    float state_1 = state_1_;
    float state_2 = state_2_;
    
    while (size--) {
      hp = (*in - r_ * state_1 - g_ * state_1 - state_2) * h_;
      bp = g_ * hp + state_1;
      state_1 = g_ * hp + bp;
      lp = g_ * bp + state_2;
      state_2 = g_ * bp + lp;
    
      float value;
      if (mode == FILTER_MODE_LOW_PASS) {
        value = lp;
      } else if (mode == FILTER_MODE_BAND_PASS) {
        value = bp;
      } else if (mode == FILTER_MODE_BAND_PASS_NORMALIZED) {
        value = bp * r_;
      } else if (mode == FILTER_MODE_HIGH_PASS) {
        value = hp;
      }
      
      *out = value;
      out += stride;
      in += stride;
    }
    state_1_ = state_1;
    state_2_ = state_2;
  }
  
  inline void ProcessMultimode(
      const float* in,
      float* out,
      size_t size,
      float mode) {
    float hp, bp, lp;
    float state_1 = state_1_;
    float state_2 = state_2_;
    float hp_gain = mode < 0.5f ? -mode * 2.0f : -2.0f + mode * 2.0f;
    float lp_gain = mode < 0.5f ? 1.0f - mode * 2.0f : 0.0f;
    float bp_gain = mode < 0.5f ? 0.0f : mode * 2.0f - 1.0f;
    while (size--) {
      hp = (*in - r_ * state_1 - g_ * state_1 - state_2) * h_;
      bp = g_ * hp + state_1;
      state_1 = g_ * hp + bp;
      lp = g_ * bp + state_2;
      state_2 = g_ * bp + lp;
      *out = hp_gain * hp + bp_gain * bp + lp_gain * lp;
      ++in;
      ++out;
    }
    state_1_ = state_1;
    state_2_ = state_2;
  }
  
  inline void ProcessMultimodeLPtoHP(
      const float* in,
      float* out,
      size_t size,
      float mode) {
    float hp, bp, lp;
    float state_1 = state_1_;
    float state_2 = state_2_;
    float hp_gain = std::min(-mode * 2.0f + 1.0f, 0.0f);
    float bp_gain = 1.0f - 2.0f * fabsf(mode - 0.5f);
    float lp_gain = std::max(1.0f - mode * 2.0f, 0.0f);
    while (size--) {
      hp = (*in - r_ * state_1 - g_ * state_1 - state_2) * h_;
      bp = g_ * hp + state_1;
      state_1 = g_ * hp + bp;
      lp = g_ * bp + state_2;
      state_2 = g_ * bp + lp;
      *out = hp_gain * hp + bp_gain * bp + lp_gain * lp;
      ++in;
      ++out;
    }
    state_1_ = state_1;
    state_2_ = state_2;
  }
  
  template<FilterMode mode>
  inline void Process(
      const float* in, float* out_1, float* out_2, size_t size,
      float gain_1, float gain_2) {
    float hp, bp, lp;
    float state_1 = state_1_;
    float state_2 = state_2_;
    
    while (size--) {
      hp = (*in - r_ * state_1 - g_ * state_1 - state_2) * h_;
      bp = g_ * hp + state_1;
      state_1 = g_ * hp + bp;
      lp = g_ * bp + state_2;
      state_2 = g_ * bp + lp;
    
      float value;
      if (mode == FILTER_MODE_LOW_PASS) {
        value = lp;
      } else if (mode == FILTER_MODE_BAND_PASS) {
        value = bp;
      } else if (mode == FILTER_MODE_BAND_PASS_NORMALIZED) {
        value = bp * r_;
      } else if (mode == FILTER_MODE_HIGH_PASS) {
        value = hp;
      }
      
      *out_1 += value * gain_1;
      *out_2 += value * gain_2;
      ++out_1;
      ++out_2;
      ++in;
    }
    state_1_ = state_1;
    state_2_ = state_2;
  }
  
  inline float g() const { return g_; }
  inline float r() const { return r_; }
  inline float h() const { return h_; }
  
 private:
  float g_;
  float r_;
  float h_;

  float state_1_;
  float state_2_;
  
  DISALLOW_COPY_AND_ASSIGN(Svf);
};

}  // namespace stmlib

#endif  // STMLIB_DSP_FILTER_H_
