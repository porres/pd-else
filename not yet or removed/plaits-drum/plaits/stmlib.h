// Copyright 2012-2019 Emilie Gillet.
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// Minimal stmlib bundle from plaits used by bass drum, snare and hi-hat engines.
//
// This file merges:
//   - stmlib.h                (base macros, Word/LongWord)
//   - dsp.h                   (ONE_POLE, SLOPE, SoftClip, Sqrt, ...)
//   - filter.h                (OnePole, Svf)
//   - parameter_interpolator.h (ParameterInterpolator)
//   - random.h + random.cc    (Random)
//   - units.h                 (SemitonesToRatio)
//   - buffer_allocator.h      (BufferAllocator)
//   - limiter.h               (Limiter - used by voice.h's ChannelPostProcessor)
//   - hysteresis_quantizer.h  (HysteresisQuantizer2 only - used by voice.h
//                              for engine selection)
//   - plaits/dsp.h            (kSampleRate, kCorrectedSampleRate, a0,
//                              kMaxBlockSize, kBlockSize)

#ifndef STMLIB_MERGED_H_
#define STMLIB_MERGED_H_

#include <inttypes.h>
#include <stddef.h>
#include <cstdint>
#include <algorithm>
#include <math.h>

#ifndef NULL
#define NULL 0
#endif

#ifdef _MSC_VER
  #define forcedinline __forceinline
#else
  #define forcedinline __attribute__((always_inline))
#endif

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)

#define CLIP(x) if (x < -32767) x = -32767; if (x > 32767) x = 32767;

#define CONSTRAIN(var, min, max) \
  if (var < (min)) { \
    var = (min); \
  } else if (var > (max)) { \
    var = (max); \
  }

// TEST=1 selects the portable (non-ARM-intrinsic) code paths below.
#define TEST 1

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

// ---------------------------------------------------------------------------
// From plaits/dsp.h
// Plaits DSP constants (block size, sample rate, note-to-frequency scale).
//
// NOTE: these live in namespace plaits (not stmlib), matching the original
// plaits/dsp.h, so existing unqualified uses keep working.
// ---------------------------------------------------------------------------

namespace plaits {

static const float kSampleRate = 48000.0f;

// There is no proper PLL for I2S, only a divider on the system clock to derive
// the bit clock.
// The division ratio is set to 47 (23 EVEN, 1 ODD) by the ST libraries.
//
// Bit clock = 72000000 / 47 = 1531.91 kHz
// Frame clock = Bit clock / 32 = 47872.34 Hz
//
// That's only 4.6 cts of error, but we care!

static const float kCorrectedSampleRate = 47872.34f;
const float a0 = (440.0f / 8.0f) / kCorrectedSampleRate;

const size_t kMaxBlockSize = 24;
const size_t kBlockSize = 12;

}  // namespace plaits

namespace stmlib {

typedef union {
  uint16_t value;
  uint8_t bytes[2];
} Word;

typedef union {
  uint32_t value;
  uint16_t words[2];
  uint8_t bytes[4];
} LongWord;

// ---------------------------------------------------------------------------
// From dsp.h
// ---------------------------------------------------------------------------

inline float Interpolate(const float* table, float index, float size) {
  index *= size;
  if (index == size) { index--; }
  MAKE_INTEGRAL_FRACTIONAL(index)
  if (!table || index_integral < 0) { return 0; }
  float a = table[index_integral];
  float b = table[index_integral + 1];
  return a + (b - a) * index_fractional;
}

inline float InterpolateWrap(const float* table, float index, float size) {
  index -= static_cast<float>(static_cast<int32_t>(index));
  index *= size;
  MAKE_INTEGRAL_FRACTIONAL(index)
  float a = table[index_integral];
  float b = table[index_integral + 1];
  return a + (b - a) * index_fractional;
}

inline float SoftLimit(float x) {
  return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

inline float SoftClip(float x) {
  if (x < -3.0f) {
    return -1.0f;
  } else if (x > 3.0f) {
    return 1.0f;
  } else {
    return SoftLimit(x);
  }
}

inline int32_t Clip16(int32_t x) {
  if (x < -32768) {
    return -32768;
  } else if (x > 32767) {
    return 32767;
  } else {
    return x;
  }
}

inline uint16_t ClipU16(int32_t x) {
  if (x < 0) {
    return 0;
  } else if (x > 65535) {
    return 65535;
  } else {
    return x;
  }
}

inline float Sqrt(float x) {
  return sqrtf(x);
}

inline int16_t SoftConvert(float x) {
  return Clip16(static_cast<int32_t>(SoftLimit(x * 0.5f) * 32768.0f));
}

// ---------------------------------------------------------------------------
// From filter.h
// ---------------------------------------------------------------------------

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

  inline void set(const Svf& f) {
    g_ = f.g();
    r_ = f.r();
    h_ = f.h();
  }

  inline void set_g_r_h(float g, float r, float h) {
    g_ = g;
    r_ = r;
    h_ = h;
  }

  inline void set_g_r(float g, float r) {
    g_ = g;
    r_ = r;
    h_ = 1.0f / (1.0f + r_ * g_ + g_ * g_);
  }

  inline void set_g_q(float g, float resonance) {
    g_ = g;
    r_ = 1.0f / resonance;
    h_ = 1.0f / (1.0f + r_ * g_ + g_ * g_);
  }

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

// ---------------------------------------------------------------------------
// From parameter_interpolator.h
// ---------------------------------------------------------------------------

class ParameterInterpolator {
 public:
  ParameterInterpolator() { }
  ParameterInterpolator(float* state, float new_value, size_t size) {
    Init(state, new_value, size);
  }

  ParameterInterpolator(float* state, float new_value, float step) {
    state_ = state;
    value_ = *state;
    increment_ = (new_value - *state) * step;
  }

  ~ParameterInterpolator() {
    *state_ = value_;
  }

  inline void Init(float* state, float new_value, size_t size) {
    state_ = state;
    value_ = *state;
    increment_ = (new_value - *state) / static_cast<float>(size);
  }

  inline float Next() {
    value_ += increment_;
    return value_;
  }

  inline float subsample(float t) {
    return value_ + increment_ * t;
  }

 private:
  float* state_;
  float value_;
  float increment_;
};

// ---------------------------------------------------------------------------
// From random.h + random.cc
// ---------------------------------------------------------------------------

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
// From units.h
// ---------------------------------------------------------------------------

inline float SemitonesToRatio(float semitones) {
  return pow(2, semitones / 12.f);
}

// ---------------------------------------------------------------------------
// From buffer_allocator.h
// ---------------------------------------------------------------------------

class BufferAllocator {
 public:
  BufferAllocator() { }
  ~BufferAllocator() { }

  BufferAllocator(void* buffer, size_t size) {
    Init(buffer, size);
  }

  inline void Init(void* buffer, size_t size) {
    buffer_ = static_cast<uint8_t*>(buffer);
    size_ = size;
    Free();
  }

  template<typename T>
  inline T* Allocate() {
    return Allocate<T>(1);
  }

  template<typename T>
  inline T* Allocate(size_t size) {
    size_t size_bytes = sizeof(T) * size;
    if (size_bytes <= free_) {
      T* start = static_cast<T*>(static_cast<void*>(next_));
      next_ += size_bytes;
      free_ -= size_bytes;
      return start;
    } else {
      return NULL;
    }
  }

  inline void Free() {
    next_ = buffer_;
    free_ = size_;
  }

  inline size_t free() const { return free_; }

 private:
  uint8_t* next_;
  uint8_t* buffer_;
  size_t free_;
  size_t size_;

  DISALLOW_COPY_AND_ASSIGN(BufferAllocator);
};

// ---------------------------------------------------------------------------
// From limiter.h
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// From hysteresis_quantizer.h (v2 only - the only version actually used)
// ---------------------------------------------------------------------------

class HysteresisQuantizer2 {
 public:
  HysteresisQuantizer2() { }
  ~HysteresisQuantizer2() { }

  void Init(int num_steps, float hysteresis, bool symmetric) {
    num_steps_ = num_steps;
    hysteresis_ = hysteresis;

    scale_ = static_cast<float>(symmetric ? num_steps - 1 : num_steps);
    offset_ = symmetric ? 0.0f : -0.5f;

    quantized_value_ = 0;
  }

  inline int Process(float value) {
    return Process(0, value);
  }

  inline int Process(int base, float value) {
    value *= scale_;
    value += offset_;
    value += static_cast<float>(base);

    float hysteresis_sign = value > static_cast<float>(quantized_value_)
        ? -1.0f
        : +1.0f;
    int q = static_cast<int>(value + hysteresis_sign * hysteresis_ + 0.5f);
    CONSTRAIN(q, 0, num_steps_ - 1);
    quantized_value_ = q;
    return q;
  }

  template<typename T>
  const T& Lookup(const T* array, float value) {
    return array[Process(value)];
  }

  inline int num_steps() const {
    return num_steps_;
  }

  inline int quantized_value() const {
    return quantized_value_;
  }

 private:
  int num_steps_;
  float hysteresis_;

  float scale_;
  float offset_;

  int quantized_value_;

  DISALLOW_COPY_AND_ASSIGN(HysteresisQuantizer2);
};

}  // namespace stmlib

#endif  // STMLIB_MERGED_H_
