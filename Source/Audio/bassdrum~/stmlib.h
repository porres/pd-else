// Copyright 2012-2019 Emilie Gillet.
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// Shared DSP primitives, ported from stmlib and plaits.
// Requires C++17 (inline variables).

#ifndef STMLIB_MERGED_H_
#define STMLIB_MERGED_H_

#include <inttypes.h>
#include <stddef.h>
#include <cstdint>
#include <algorithm>
#include <math.h>

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)

#define CONSTRAIN(var, min, max) \
  if (var < (min)) { \
    var = (min); \
  } else if (var > (max)) { \
    var = (max); \
  }

#define MAKE_INTEGRAL_FRACTIONAL(x) \
  int32_t x ## _integral = static_cast<int32_t>(x); \
  float x ## _fractional = x - static_cast<float>(x ## _integral);

#define ONE_POLE(out, in, coefficient) out += (coefficient) * ((in) - out);
#define SLOPE(out, in, positive, negative) { \
  float error = (in) - out; \
  out += (error > 0 ? positive : negative) * error; \
}

#define _USE_MATH_DEFINES
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define M_PI_F float(M_PI)
#define M_PI_POW_2 M_PI * M_PI
#define M_PI_POW_3 M_PI_POW_2 * M_PI
#define M_PI_POW_5 M_PI_POW_3 * M_PI_POW_2
#define M_PI_POW_7 M_PI_POW_5 * M_PI_POW_2
#define M_PI_POW_9 M_PI_POW_7 * M_PI_POW_2
#define M_PI_POW_11 M_PI_POW_9 * M_PI_POW_2

namespace stmlib {

// Table lookup with linear interpolation; phase is wrapped into [0, 1).
inline float InterpolateWrap(const float* table, float index, float size) {
  index -= static_cast<float>(static_cast<int32_t>(index));
  index *= size;
  MAKE_INTEGRAL_FRACTIONAL(index)
  float a = table[index_integral];
  float b = table[index_integral + 1];
  return a + (b - a) * index_fractional;
}

inline float SoftClip(float x) {
  if (x < -3.0f) {
    return -1.0f;
  } else if (x > 3.0f) {
    return 1.0f;
  } else {
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
  }
}

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

// tan(pi * f) approximations, used to compute the SVF coefficient.
// (Formerly OnePole::tan; the OnePole filter itself was never used.)
template<FrequencyApproximation approximation>
inline float Tan(float f) {
  if (approximation == FREQUENCY_EXACT) {
    f = f < 0.497f ? f : 0.497f;
    return tanf(M_PI_F * f);
  } else if (approximation == FREQUENCY_DIRTY) {
    const float a = 3.736e-01f * M_PI_POW_3;
    return f * (M_PI_F + a * f * f);
  } else if (approximation == FREQUENCY_FAST) {
    const float a = 3.260e-01f * M_PI_POW_3;
    const float b = 1.823e-01f * M_PI_POW_5;
    float f2 = f * f;
    return f * (M_PI_F + f2 * (a + b * f2));
  } else if (approximation == FREQUENCY_ACCURATE) {
    const float a = 3.333314036e-01f * M_PI_POW_3;
    const float b = 1.333923995e-01f * M_PI_POW_5;
    const float c = 5.33740603e-02f * M_PI_POW_7;
    const float d = 2.900525e-03f * M_PI_POW_9;
    const float e = 9.5168091e-03f * M_PI_POW_11;
    float f2 = f * f;
    return f * (M_PI_F + f2 * (a + f2 * (b + f2 * (c + f2 * (d + f2 * e)))));
  }
}

// Trapezoidal state-variable filter (only the entry points this engine uses).
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

  template<FrequencyApproximation approximation>
  inline void set_f_q(float f, float resonance) {
    g_ = Tan<approximation>(f);
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

 private:
  float g_;
  float r_;
  float h_;

  float state_1_;
  float state_2_;

  DISALLOW_COPY_AND_ASSIGN(Svf);
};

// Linearly ramps a parameter across a block; writes the final value back to
// *state when it goes out of scope.
class ParameterInterpolator {
 public:
  ParameterInterpolator(float* state, float new_value, size_t size) {
    state_ = state;
    value_ = *state;
    increment_ = (new_value - *state) / static_cast<float>(size);
  }

  ~ParameterInterpolator() {
    *state_ = value_;
  }

  inline float Next() {
    value_ += increment_;
    return value_;
  }

 private:
  float* state_;
  float value_;
  float increment_;
};

class Random {
 public:
  static inline uint32_t state() { return rng_state_; }

  static inline uint32_t GetWord() {
    rng_state_ = rng_state_ * 1664525L + 1013904223L;
    return state();
  }

  static inline float GetFloat() {
    return static_cast<float>(GetWord()) / 4294967296.0f;
  }

 private:
  static inline uint32_t rng_state_ = 0x21;  // C++17 inline variable.

  DISALLOW_COPY_AND_ASSIGN(Random);
};

// ---------------------------------------------------------------------------
// Sine wavetable (513 entries: one full cycle plus the wrap-around sample).
// ---------------------------------------------------------------------------

inline const float lut_sine[] = {
  0.000000000e+00, 1.227153829e-02, 2.454122852e-02, 3.680722294e-02,
  4.906767433e-02, 6.132073630e-02, 7.356456360e-02, 8.579731234e-02,
  9.801714033e-02, 1.102222073e-01, 1.224106752e-01, 1.345807085e-01,
  1.467304745e-01, 1.588581433e-01, 1.709618888e-01, 1.830398880e-01,
  1.950903220e-01, 2.071113762e-01, 2.191012402e-01, 2.310581083e-01,
  2.429801799e-01, 2.548656596e-01, 2.667127575e-01, 2.785196894e-01,
  2.902846773e-01, 3.020059493e-01, 3.136817404e-01, 3.253102922e-01,
  3.368898534e-01, 3.484186802e-01, 3.598950365e-01, 3.713171940e-01,
  3.826834324e-01, 3.939920401e-01, 4.052413140e-01, 4.164295601e-01,
  4.275550934e-01, 4.386162385e-01, 4.496113297e-01, 4.605387110e-01,
  4.713967368e-01, 4.821837721e-01, 4.928981922e-01, 5.035383837e-01,
  5.141027442e-01, 5.245896827e-01, 5.349976199e-01, 5.453249884e-01,
  5.555702330e-01, 5.657318108e-01, 5.758081914e-01, 5.857978575e-01,
  5.956993045e-01, 6.055110414e-01, 6.152315906e-01, 6.248594881e-01,
  6.343932842e-01, 6.438315429e-01, 6.531728430e-01, 6.624157776e-01,
  6.715589548e-01, 6.806009978e-01, 6.895405447e-01, 6.983762494e-01,
  7.071067812e-01, 7.157308253e-01, 7.242470830e-01, 7.326542717e-01,
  7.409511254e-01, 7.491363945e-01, 7.572088465e-01, 7.651672656e-01,
  7.730104534e-01, 7.807372286e-01, 7.883464276e-01, 7.958369046e-01,
  8.032075315e-01, 8.104571983e-01, 8.175848132e-01, 8.245893028e-01,
  8.314696123e-01, 8.382247056e-01, 8.448535652e-01, 8.513551931e-01,
  8.577286100e-01, 8.639728561e-01, 8.700869911e-01, 8.760700942e-01,
  8.819212643e-01, 8.876396204e-01, 8.932243012e-01, 8.986744657e-01,
  9.039892931e-01, 9.091679831e-01, 9.142097557e-01, 9.191138517e-01,
  9.238795325e-01, 9.285060805e-01, 9.329927988e-01, 9.373390119e-01,
  9.415440652e-01, 9.456073254e-01, 9.495281806e-01, 9.533060404e-01,
  9.569403357e-01, 9.604305194e-01, 9.637760658e-01, 9.669764710e-01,
  9.700312532e-01, 9.729399522e-01, 9.757021300e-01, 9.783173707e-01,
  9.807852804e-01, 9.831054874e-01, 9.852776424e-01, 9.873014182e-01,
  9.891765100e-01, 9.909026354e-01, 9.924795346e-01, 9.939069700e-01,
  9.951847267e-01, 9.963126122e-01, 9.972904567e-01, 9.981181129e-01,
  9.987954562e-01, 9.993223846e-01, 9.996988187e-01, 9.999247018e-01,
  1.000000000e+00, 9.999247018e-01, 9.996988187e-01, 9.993223846e-01,
  9.987954562e-01, 9.981181129e-01, 9.972904567e-01, 9.963126122e-01,
  9.951847267e-01, 9.939069700e-01, 9.924795346e-01, 9.909026354e-01,
  9.891765100e-01, 9.873014182e-01, 9.852776424e-01, 9.831054874e-01,
  9.807852804e-01, 9.783173707e-01, 9.757021300e-01, 9.729399522e-01,
  9.700312532e-01, 9.669764710e-01, 9.637760658e-01, 9.604305194e-01,
  9.569403357e-01, 9.533060404e-01, 9.495281806e-01, 9.456073254e-01,
  9.415440652e-01, 9.373390119e-01, 9.329927988e-01, 9.285060805e-01,
  9.238795325e-01, 9.191138517e-01, 9.142097557e-01, 9.091679831e-01,
  9.039892931e-01, 8.986744657e-01, 8.932243012e-01, 8.876396204e-01,
  8.819212643e-01, 8.760700942e-01, 8.700869911e-01, 8.639728561e-01,
  8.577286100e-01, 8.513551931e-01, 8.448535652e-01, 8.382247056e-01,
  8.314696123e-01, 8.245893028e-01, 8.175848132e-01, 8.104571983e-01,
  8.032075315e-01, 7.958369046e-01, 7.883464276e-01, 7.807372286e-01,
  7.730104534e-01, 7.651672656e-01, 7.572088465e-01, 7.491363945e-01,
  7.409511254e-01, 7.326542717e-01, 7.242470830e-01, 7.157308253e-01,
  7.071067812e-01, 6.983762494e-01, 6.895405447e-01, 6.806009978e-01,
  6.715589548e-01, 6.624157776e-01, 6.531728430e-01, 6.438315429e-01,
  6.343932842e-01, 6.248594881e-01, 6.152315906e-01, 6.055110414e-01,
  5.956993045e-01, 5.857978575e-01, 5.758081914e-01, 5.657318108e-01,
  5.555702330e-01, 5.453249884e-01, 5.349976199e-01, 5.245896827e-01,
  5.141027442e-01, 5.035383837e-01, 4.928981922e-01, 4.821837721e-01,
  4.713967368e-01, 4.605387110e-01, 4.496113297e-01, 4.386162385e-01,
  4.275550934e-01, 4.164295601e-01, 4.052413140e-01, 3.939920401e-01,
  3.826834324e-01, 3.713171940e-01, 3.598950365e-01, 3.484186802e-01,
  3.368898534e-01, 3.253102922e-01, 3.136817404e-01, 3.020059493e-01,
  2.902846773e-01, 2.785196894e-01, 2.667127575e-01, 2.548656596e-01,
  2.429801799e-01, 2.310581083e-01, 2.191012402e-01, 2.071113762e-01,
  1.950903220e-01, 1.830398880e-01, 1.709618888e-01, 1.588581433e-01,
  1.467304745e-01, 1.345807085e-01, 1.224106752e-01, 1.102222073e-01,
  9.801714033e-02, 8.579731234e-02, 7.356456360e-02, 6.132073630e-02,
  4.906767433e-02, 3.680722294e-02, 2.454122852e-02, 1.227153829e-02,
  1.224646799e-16, -1.227153829e-02, -2.454122852e-02, -3.680722294e-02,
  -4.906767433e-02, -6.132073630e-02, -7.356456360e-02, -8.579731234e-02,
  -9.801714033e-02, -1.102222073e-01, -1.224106752e-01, -1.345807085e-01,
  -1.467304745e-01, -1.588581433e-01, -1.709618888e-01, -1.830398880e-01,
  -1.950903220e-01, -2.071113762e-01, -2.191012402e-01, -2.310581083e-01,
  -2.429801799e-01, -2.548656596e-01, -2.667127575e-01, -2.785196894e-01,
  -2.902846773e-01, -3.020059493e-01, -3.136817404e-01, -3.253102922e-01,
  -3.368898534e-01, -3.484186802e-01, -3.598950365e-01, -3.713171940e-01,
  -3.826834324e-01, -3.939920401e-01, -4.052413140e-01, -4.164295601e-01,
  -4.275550934e-01, -4.386162385e-01, -4.496113297e-01, -4.605387110e-01,
  -4.713967368e-01, -4.821837721e-01, -4.928981922e-01, -5.035383837e-01,
  -5.141027442e-01, -5.245896827e-01, -5.349976199e-01, -5.453249884e-01,
  -5.555702330e-01, -5.657318108e-01, -5.758081914e-01, -5.857978575e-01,
  -5.956993045e-01, -6.055110414e-01, -6.152315906e-01, -6.248594881e-01,
  -6.343932842e-01, -6.438315429e-01, -6.531728430e-01, -6.624157776e-01,
  -6.715589548e-01, -6.806009978e-01, -6.895405447e-01, -6.983762494e-01,
  -7.071067812e-01, -7.157308253e-01, -7.242470830e-01, -7.326542717e-01,
  -7.409511254e-01, -7.491363945e-01, -7.572088465e-01, -7.651672656e-01,
  -7.730104534e-01, -7.807372286e-01, -7.883464276e-01, -7.958369046e-01,
  -8.032075315e-01, -8.104571983e-01, -8.175848132e-01, -8.245893028e-01,
  -8.314696123e-01, -8.382247056e-01, -8.448535652e-01, -8.513551931e-01,
  -8.577286100e-01, -8.639728561e-01, -8.700869911e-01, -8.760700942e-01,
  -8.819212643e-01, -8.876396204e-01, -8.932243012e-01, -8.986744657e-01,
  -9.039892931e-01, -9.091679831e-01, -9.142097557e-01, -9.191138517e-01,
  -9.238795325e-01, -9.285060805e-01, -9.329927988e-01, -9.373390119e-01,
  -9.415440652e-01, -9.456073254e-01, -9.495281806e-01, -9.533060404e-01,
  -9.569403357e-01, -9.604305194e-01, -9.637760658e-01, -9.669764710e-01,
  -9.700312532e-01, -9.729399522e-01, -9.757021300e-01, -9.783173707e-01,
  -9.807852804e-01, -9.831054874e-01, -9.852776424e-01, -9.873014182e-01,
  -9.891765100e-01, -9.909026354e-01, -9.924795346e-01, -9.939069700e-01,
  -9.951847267e-01, -9.963126122e-01, -9.972904567e-01, -9.981181129e-01,
  -9.987954562e-01, -9.993223846e-01, -9.996988187e-01, -9.999247018e-01,
  -1.000000000e+00, -9.999247018e-01, -9.996988187e-01, -9.993223846e-01,
  -9.987954562e-01, -9.981181129e-01, -9.972904567e-01, -9.963126122e-01,
  -9.951847267e-01, -9.939069700e-01, -9.924795346e-01, -9.909026354e-01,
  -9.891765100e-01, -9.873014182e-01, -9.852776424e-01, -9.831054874e-01,
  -9.807852804e-01, -9.783173707e-01, -9.757021300e-01, -9.729399522e-01,
  -9.700312532e-01, -9.669764710e-01, -9.637760658e-01, -9.604305194e-01,
  -9.569403357e-01, -9.533060404e-01, -9.495281806e-01, -9.456073254e-01,
  -9.415440652e-01, -9.373390119e-01, -9.329927988e-01, -9.285060805e-01,
  -9.238795325e-01, -9.191138517e-01, -9.142097557e-01, -9.091679831e-01,
  -9.039892931e-01, -8.986744657e-01, -8.932243012e-01, -8.876396204e-01,
  -8.819212643e-01, -8.760700942e-01, -8.700869911e-01, -8.639728561e-01,
  -8.577286100e-01, -8.513551931e-01, -8.448535652e-01, -8.382247056e-01,
  -8.314696123e-01, -8.245893028e-01, -8.175848132e-01, -8.104571983e-01,
  -8.032075315e-01, -7.958369046e-01, -7.883464276e-01, -7.807372286e-01,
  -7.730104534e-01, -7.651672656e-01, -7.572088465e-01, -7.491363945e-01,
  -7.409511254e-01, -7.326542717e-01, -7.242470830e-01, -7.157308253e-01,
  -7.071067812e-01, -6.983762494e-01, -6.895405447e-01, -6.806009978e-01,
  -6.715589548e-01, -6.624157776e-01, -6.531728430e-01, -6.438315429e-01,
  -6.343932842e-01, -6.248594881e-01, -6.152315906e-01, -6.055110414e-01,
  -5.956993045e-01, -5.857978575e-01, -5.758081914e-01, -5.657318108e-01,
  -5.555702330e-01, -5.453249884e-01, -5.349976199e-01, -5.245896827e-01,
  -5.141027442e-01, -5.035383837e-01, -4.928981922e-01, -4.821837721e-01,
  -4.713967368e-01, -4.605387110e-01, -4.496113297e-01, -4.386162385e-01,
  -4.275550934e-01, -4.164295601e-01, -4.052413140e-01, -3.939920401e-01,
  -3.826834324e-01, -3.713171940e-01, -3.598950365e-01, -3.484186802e-01,
  -3.368898534e-01, -3.253102922e-01, -3.136817404e-01, -3.020059493e-01,
  -2.902846773e-01, -2.785196894e-01, -2.667127575e-01, -2.548656596e-01,
  -2.429801799e-01, -2.310581083e-01, -2.191012402e-01, -2.071113762e-01,
  -1.950903220e-01, -1.830398880e-01, -1.709618888e-01, -1.588581433e-01,
  -1.467304745e-01, -1.345807085e-01, -1.224106752e-01, -1.102222073e-01,
  -9.801714033e-02, -8.579731234e-02, -7.356456360e-02, -6.132073630e-02,
  -4.906767433e-02, -3.680722294e-02, -2.454122852e-02, -1.227153829e-02,
  -2.449293598e-16, 1.227153829e-02, 2.454122852e-02, 3.680722294e-02,
};

const float kSineLUTSize = 512.0f;

// Safe for phase >= 0.0f, will wrap.
inline float Sine(float phase) {
  return stmlib::InterpolateWrap(lut_sine, phase, kSineLUTSize);
}

// ---------------------------------------------------------------------------
// Overdrive
// ---------------------------------------------------------------------------

class Overdrive {
 public:
  Overdrive() { }
  ~Overdrive() { }
  void Init() {
    pre_gain_ = 0.0f;
    post_gain_ = 0.0f;
  }
  void Process(float drive, float* in_out, size_t size) {
    const float drive_2 = drive * drive;
    const float pre_gain_a = drive * 0.5f;
    const float pre_gain_b = drive_2 * drive_2 * drive * 24.0f;
    const float pre_gain = pre_gain_a + (pre_gain_b - pre_gain_a) * drive_2;
    const float drive_squashed = drive * (2.0f - drive);
    const float post_gain = 1.0f / stmlib::SoftClip(
        0.33f + drive_squashed * (pre_gain - 0.33f));
    stmlib::ParameterInterpolator pre_gain_modulation(
        &pre_gain_, pre_gain, size);
    stmlib::ParameterInterpolator post_gain_modulation(
        &post_gain_, post_gain, size);
    while (size--) {
      float pre = pre_gain_modulation.Next() * *in_out;
      *in_out++ = stmlib::SoftClip(pre) * post_gain_modulation.Next();
    }
  }
 private:
  float pre_gain_;
  float post_gain_;
  DISALLOW_COPY_AND_ASSIGN(Overdrive);
};

}  // namespace stmlib

#endif  // STMLIB_MERGED_H_
