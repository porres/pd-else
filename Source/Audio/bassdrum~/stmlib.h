// Original copyright Emilie Gillet, MIT license.

#pragma once

#include <inttypes.h>
#include <stddef.h>
#include <math.h>
#include <cstdint>
#include <algorithm>

#ifndef NULL
#define NULL 0
#endif

#define DISALLOW_COPY_AND_ASSIGN(TypeName) \
  TypeName(const TypeName&);               \
  void operator=(const TypeName&)

#define CONSTRAIN(var, min, max) \
  if (var < (min)) { \
    var = (min); \
  } else if (var > (max)) { \
    var = (max); \
  }

namespace stmlib {

// =============================================================================
// dsp.h (trimmed)
// =============================================================================

#define MAKE_INTEGRAL_FRACTIONAL(x) \
  int32_t x ## _integral = static_cast<int32_t>(x); \
  float x ## _fractional = x - static_cast<float>(x ## _integral);

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

#define ONE_POLE(out, in, coefficient) out += (coefficient) * ((in) - out);
#define SLOPE(out, in, positive, negative) { \
  float error = (in) - out; \
  out += (error > 0 ? positive : negative) * error; \
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

// =============================================================================
// random.h + random.cc (trimmed)
// =============================================================================

class Random {
 public:
  static inline uint32_t state() { return rng_state_; }

  static inline void Seed(uint32_t seed) {
    rng_state_ = seed;
  }

  static inline uint32_t GetWord() {
    rng_state_ = rng_state_ * 1664525L + 1013904223L;
    return state();
  }

  static inline int16_t GetSample() {
    return static_cast<int16_t>(GetWord() >> 16);
  }

  static inline float GetFloat() {
    return static_cast<float>(GetWord()) / 4294967296.0f;
  }

 private:
  static uint32_t rng_state_;

  DISALLOW_COPY_AND_ASSIGN(Random);
};

inline uint32_t Random::rng_state_ = 0x21;

// =============================================================================
// parameter_interpolator.h (trimmed)
// =============================================================================

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

 private:
  float* state_;
  float value_;
  float increment_;
};

// =============================================================================
// units.h + units.cc (trimmed)
// =============================================================================

static const float lut_pitch_ratio_high[] = {
   6.151958251e-04,  6.517772725e-04,  6.905339660e-04,  7.315952524e-04,
   7.750981699e-04,  8.211879055e-04,  8.700182794e-04,  9.217522585e-04,
   9.765625000e-04,  1.034631928e-03,  1.096154344e-03,  1.161335073e-03,
   1.230391650e-03,  1.303554545e-03,  1.381067932e-03,  1.463190505e-03,
   1.550196340e-03,  1.642375811e-03,  1.740036559e-03,  1.843504517e-03,
   1.953125000e-03,  2.069263856e-03,  2.192308688e-03,  2.322670146e-03,
   2.460783301e-03,  2.607109090e-03,  2.762135864e-03,  2.926381010e-03,
   3.100392680e-03,  3.284751622e-03,  3.480073118e-03,  3.687009034e-03,
   3.906250000e-03,  4.138527712e-03,  4.384617376e-03,  4.645340293e-03,
   4.921566601e-03,  5.214218180e-03,  5.524271728e-03,  5.852762019e-03,
   6.200785359e-03,  6.569503244e-03,  6.960146235e-03,  7.374018068e-03,
   7.812500000e-03,  8.277055425e-03,  8.769234752e-03,  9.290680586e-03,
   9.843133202e-03,  1.042843636e-02,  1.104854346e-02,  1.170552404e-02,
   1.240157072e-02,  1.313900649e-02,  1.392029247e-02,  1.474803614e-02,
   1.562500000e-02,  1.655411085e-02,  1.753846950e-02,  1.858136117e-02,
   1.968626640e-02,  2.085687272e-02,  2.209708691e-02,  2.341104808e-02,
   2.480314144e-02,  2.627801298e-02,  2.784058494e-02,  2.949607227e-02,
   3.125000000e-02,  3.310822170e-02,  3.507693901e-02,  3.716272234e-02,
   3.937253281e-02,  4.171374544e-02,  4.419417382e-02,  4.682209615e-02,
   4.960628287e-02,  5.255602595e-02,  5.568116988e-02,  5.899214454e-02,
   6.250000000e-02,  6.621644340e-02,  7.015387802e-02,  7.432544469e-02,
   7.874506562e-02,  8.342749089e-02,  8.838834765e-02,  9.364419230e-02,
   9.921256575e-02,  1.051120519e-01,  1.113623398e-01,  1.179842891e-01,
   1.250000000e-01,  1.324328868e-01,  1.403077560e-01,  1.486508894e-01,
   1.574901312e-01,  1.668549818e-01,  1.767766953e-01,  1.872883846e-01,
   1.984251315e-01,  2.102241038e-01,  2.227246795e-01,  2.359685782e-01,
   2.500000000e-01,  2.648657736e-01,  2.806155121e-01,  2.973017788e-01,
   3.149802625e-01,  3.337099635e-01,  3.535533906e-01,  3.745767692e-01,
   3.968502630e-01,  4.204482076e-01,  4.454493591e-01,  4.719371563e-01,
   5.000000000e-01,  5.297315472e-01,  5.612310242e-01,  5.946035575e-01,
   6.299605249e-01,  6.674199271e-01,  7.071067812e-01,  7.491535384e-01,
   7.937005260e-01,  8.408964153e-01,  8.908987181e-01,  9.438743127e-01,
   1.000000000e+00,  1.059463094e+00,  1.122462048e+00,  1.189207115e+00,
   1.259921050e+00,  1.334839854e+00,  1.414213562e+00,  1.498307077e+00,
   1.587401052e+00,  1.681792831e+00,  1.781797436e+00,  1.887748625e+00,
   2.000000000e+00,  2.118926189e+00,  2.244924097e+00,  2.378414230e+00,
   2.519842100e+00,  2.669679708e+00,  2.828427125e+00,  2.996614154e+00,
   3.174802104e+00,  3.363585661e+00,  3.563594873e+00,  3.775497251e+00,
   4.000000000e+00,  4.237852377e+00,  4.489848193e+00,  4.756828460e+00,
   5.039684200e+00,  5.339359417e+00,  5.656854249e+00,  5.993228308e+00,
   6.349604208e+00,  6.727171322e+00,  7.127189745e+00,  7.550994501e+00,
   8.000000000e+00,  8.475704755e+00,  8.979696386e+00,  9.513656920e+00,
   1.007936840e+01,  1.067871883e+01,  1.131370850e+01,  1.198645662e+01,
   1.269920842e+01,  1.345434264e+01,  1.425437949e+01,  1.510198900e+01,
   1.600000000e+01,  1.695140951e+01,  1.795939277e+01,  1.902731384e+01,
   2.015873680e+01,  2.135743767e+01,  2.262741700e+01,  2.397291323e+01,
   2.539841683e+01,  2.690868529e+01,  2.850875898e+01,  3.020397801e+01,
   3.200000000e+01,  3.390281902e+01,  3.591878555e+01,  3.805462768e+01,
   4.031747360e+01,  4.271487533e+01,  4.525483400e+01,  4.794582646e+01,
   5.079683366e+01,  5.381737058e+01,  5.701751796e+01,  6.040795601e+01,
   6.400000000e+01,  6.780563804e+01,  7.183757109e+01,  7.610925536e+01,
   8.063494719e+01,  8.542975067e+01,  9.050966799e+01,  9.589165292e+01,
   1.015936673e+02,  1.076347412e+02,  1.140350359e+02,  1.208159120e+02,
   1.280000000e+02,  1.356112761e+02,  1.436751422e+02,  1.522185107e+02,
   1.612698944e+02,  1.708595013e+02,  1.810193360e+02,  1.917833058e+02,
   2.031873347e+02,  2.152694823e+02,  2.280700718e+02,  2.416318240e+02,
   2.560000000e+02,  2.712225522e+02,  2.873502844e+02,  3.044370214e+02,
   3.225397888e+02,  3.417190027e+02,  3.620386720e+02,  3.835666117e+02,
   4.063746693e+02,  4.305389646e+02,  4.561401437e+02,  4.832636481e+02,
   5.120000000e+02,  5.424451043e+02,  5.747005687e+02,  6.088740429e+02,
   6.450795775e+02,  6.834380053e+02,  7.240773439e+02,  7.671332234e+02,
   8.127493386e+02,  8.610779292e+02,  9.122802874e+02,  9.665272962e+02,
   1.024000000e+03,  1.084890209e+03,  1.149401137e+03,  1.217748086e+03,
   1.290159155e+03,  1.366876011e+03,  1.448154688e+03,  1.534266447e+03,
};

static const float lut_pitch_ratio_low[] = {
   1.000000000e+00,  1.000225659e+00,  1.000451370e+00,  1.000677131e+00,
   1.000902943e+00,  1.001128806e+00,  1.001354720e+00,  1.001580685e+00,
   1.001806701e+00,  1.002032768e+00,  1.002258886e+00,  1.002485055e+00,
   1.002711275e+00,  1.002937546e+00,  1.003163868e+00,  1.003390242e+00,
   1.003616666e+00,  1.003843141e+00,  1.004069668e+00,  1.004296246e+00,
   1.004522874e+00,  1.004749554e+00,  1.004976285e+00,  1.005203068e+00,
   1.005429901e+00,  1.005656786e+00,  1.005883722e+00,  1.006110709e+00,
   1.006337747e+00,  1.006564836e+00,  1.006791977e+00,  1.007019169e+00,
   1.007246412e+00,  1.007473707e+00,  1.007701053e+00,  1.007928450e+00,
   1.008155898e+00,  1.008383398e+00,  1.008610949e+00,  1.008838551e+00,
   1.009066205e+00,  1.009293910e+00,  1.009521667e+00,  1.009749475e+00,
   1.009977334e+00,  1.010205245e+00,  1.010433207e+00,  1.010661221e+00,
   1.010889286e+00,  1.011117403e+00,  1.011345571e+00,  1.011573790e+00,
   1.011802061e+00,  1.012030384e+00,  1.012258758e+00,  1.012487183e+00,
   1.012715661e+00,  1.012944189e+00,  1.013172770e+00,  1.013401401e+00,
   1.013630085e+00,  1.013858820e+00,  1.014087607e+00,  1.014316445e+00,
   1.014545335e+00,  1.014774277e+00,  1.015003270e+00,  1.015232315e+00,
   1.015461411e+00,  1.015690560e+00,  1.015919760e+00,  1.016149011e+00,
   1.016378315e+00,  1.016607670e+00,  1.016837077e+00,  1.017066536e+00,
   1.017296046e+00,  1.017525609e+00,  1.017755223e+00,  1.017984889e+00,
   1.018214607e+00,  1.018444376e+00,  1.018674198e+00,  1.018904071e+00,
   1.019133996e+00,  1.019363973e+00,  1.019594002e+00,  1.019824083e+00,
   1.020054216e+00,  1.020284401e+00,  1.020514637e+00,  1.020744926e+00,
   1.020975266e+00,  1.021205659e+00,  1.021436104e+00,  1.021666600e+00,
   1.021897149e+00,  1.022127749e+00,  1.022358402e+00,  1.022589107e+00,
   1.022819863e+00,  1.023050672e+00,  1.023281533e+00,  1.023512446e+00,
   1.023743411e+00,  1.023974428e+00,  1.024205498e+00,  1.024436619e+00,
   1.024667793e+00,  1.024899019e+00,  1.025130297e+00,  1.025361627e+00,
   1.025593009e+00,  1.025824444e+00,  1.026055931e+00,  1.026287470e+00,
   1.026519061e+00,  1.026750705e+00,  1.026982401e+00,  1.027214149e+00,
   1.027445949e+00,  1.027677802e+00,  1.027909707e+00,  1.028141664e+00,
   1.028373674e+00,  1.028605736e+00,  1.028837851e+00,  1.029070017e+00,
   1.029302237e+00,  1.029534508e+00,  1.029766832e+00,  1.029999209e+00,
   1.030231638e+00,  1.030464119e+00,  1.030696653e+00,  1.030929239e+00,
   1.031161878e+00,  1.031394569e+00,  1.031627313e+00,  1.031860109e+00,
   1.032092958e+00,  1.032325859e+00,  1.032558813e+00,  1.032791820e+00,
   1.033024879e+00,  1.033257991e+00,  1.033491155e+00,  1.033724372e+00,
   1.033957641e+00,  1.034190964e+00,  1.034424338e+00,  1.034657766e+00,
   1.034891246e+00,  1.035124779e+00,  1.035358364e+00,  1.035592003e+00,
   1.035825694e+00,  1.036059437e+00,  1.036293234e+00,  1.036527083e+00,
   1.036760985e+00,  1.036994940e+00,  1.037228947e+00,  1.037463008e+00,
   1.037697121e+00,  1.037931287e+00,  1.038165506e+00,  1.038399777e+00,
   1.038634102e+00,  1.038868479e+00,  1.039102910e+00,  1.039337393e+00,
   1.039571929e+00,  1.039806518e+00,  1.040041160e+00,  1.040275855e+00,
   1.040510603e+00,  1.040745404e+00,  1.040980258e+00,  1.041215165e+00,
   1.041450125e+00,  1.041685138e+00,  1.041920204e+00,  1.042155323e+00,
   1.042390495e+00,  1.042625720e+00,  1.042860998e+00,  1.043096329e+00,
   1.043331714e+00,  1.043567151e+00,  1.043802642e+00,  1.044038185e+00,
   1.044273782e+00,  1.044509433e+00,  1.044745136e+00,  1.044980892e+00,
   1.045216702e+00,  1.045452565e+00,  1.045688481e+00,  1.045924450e+00,
   1.046160473e+00,  1.046396549e+00,  1.046632678e+00,  1.046868860e+00,
   1.047105096e+00,  1.047341385e+00,  1.047577727e+00,  1.047814123e+00,
   1.048050572e+00,  1.048287074e+00,  1.048523630e+00,  1.048760239e+00,
   1.048996902e+00,  1.049233618e+00,  1.049470387e+00,  1.049707210e+00,
   1.049944086e+00,  1.050181015e+00,  1.050417999e+00,  1.050655035e+00,
   1.050892125e+00,  1.051129269e+00,  1.051366466e+00,  1.051603717e+00,
   1.051841021e+00,  1.052078378e+00,  1.052315790e+00,  1.052553255e+00,
   1.052790773e+00,  1.053028345e+00,  1.053265971e+00,  1.053503650e+00,
   1.053741383e+00,  1.053979169e+00,  1.054217010e+00,  1.054454903e+00,
   1.054692851e+00,  1.054930852e+00,  1.055168907e+00,  1.055407016e+00,
   1.055645178e+00,  1.055883395e+00,  1.056121664e+00,  1.056359988e+00,
   1.056598366e+00,  1.056836797e+00,  1.057075282e+00,  1.057313821e+00,
   1.057552413e+00,  1.057791060e+00,  1.058029760e+00,  1.058268515e+00,
   1.058507323e+00,  1.058746185e+00,  1.058985101e+00,  1.059224071e+00,
};

inline float SemitonesToRatio(float semitones) {
  float pitch = semitones + 128.0f;
  MAKE_INTEGRAL_FRACTIONAL(pitch)

  return lut_pitch_ratio_high[pitch_integral] * \
      lut_pitch_ratio_low[static_cast<int32_t>(pitch_fractional * 256.0f)];
}

// =============================================================================
// filter.h (trimmed)
// =============================================================================

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
    return 0.0f;
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
    return 0.0f;
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

// =============================================================================
// buffer_allocator.h (trimmed)
// =============================================================================

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

}  // namespace stmlib
