// Original copyright Emilie Gillet, MIT license.

#pragma once

#define _USE_MATH_DEFINES
#define TEST 1

#include <inttypes.h>
#include <stddef.h>
#include <math.h>
#include <cstdint>
#include <algorithm>

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

#define JOIN(lhs, rhs)    JOIN_1(lhs, rhs)
#define JOIN_1(lhs, rhs)  JOIN_2(lhs, rhs)
#define JOIN_2(lhs, rhs)  lhs##rhs

#define STATIC_ASSERT(expression, message)\
  struct JOIN(__static_assertion_at_line_, __LINE__)\
  {\
    impl::StaticAssertion<static_cast<bool>((expression))> JOIN(JOIN(JOIN(STATIC_ASSERTION_FAILED_AT_LINE_, __LINE__), _), message);\
  };\
  typedef impl::StaticAssertionTest<sizeof(JOIN(__static_assertion_at_line_, __LINE__))> JOIN(__static_assertion_test_at_line_, __LINE__)

namespace impl {

  template <bool>
  struct StaticAssertion;

  template <>
  struct StaticAssertion<true>
  {
  };

  template<int i>
  struct StaticAssertionTest
  {
  };

} // namespace impl

#define IN_RAM

#define UNROLL2(x) x; x;
#define UNROLL4(x) x; x; x; x;
#define UNROLL8(x) x; x; x; x; x; x; x; x;

template<bool b>
inline void StaticAssertImplementation() {
    char static_assert_size_mismatch[b] = { 0 };
}

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

template<uint32_t a, uint32_t b, uint32_t c, uint32_t d>
struct FourCC {
  static const uint32_t value = (((((d << 8) | c) << 8) | b) << 8) | a;
};

// =============================================================================
// dsp.h
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

inline float InterpolateHermite(const float* table, float index, float size) {
  index *= size;
  MAKE_INTEGRAL_FRACTIONAL(index)
  const float xm1 = table[index_integral - 1];
  const float x0 = table[index_integral + 0];
  const float x1 = table[index_integral + 1];
  const float x2 = table[index_integral + 2];
  const float c = (x1 - xm1) * 0.5f;
  const float v = x0 - x1;
  const float w = c + v;
  const float a = w + v + (x2 - x0) * 0.5f;
  const float b_neg = w + a;
  const float f = index_fractional;
  return (((a * f) - b_neg) * f + c) * f + x0;
}

inline float InterpolateWrap(const float* table, float index, float size) {
  index -= static_cast<float>(static_cast<int32_t>(index));
  index *= size;
  MAKE_INTEGRAL_FRACTIONAL(index)
  float a = table[index_integral];
  float b = table[index_integral + 1];
  return a + (b - a) * index_fractional;
}

inline float SmoothStep(float value) {
  return value * value * (3.0f - 2.0f * value);
}

#define ONE_POLE(out, in, coefficient) out += (coefficient) * ((in) - out);
#define SLOPE(out, in, positive, negative) { \
  float error = (in) - out; \
  out += (error > 0 ? positive : negative) * error; \
}
#define SLEW(out, in, delta) { \
  float error = (in) - out; \
  float d = (delta); \
  if (error > d) { \
    error = d; \
  } else if (error < -d) { \
    error = -d; \
  } \
  out += error; \
}

inline float Crossfade(float a, float b, float fade) {
  return a + (b - a) * fade;
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

// =============================================================================
// rsqrt.h
// =============================================================================

template<typename To, typename From>
struct unsafe_bit_cast_t {
  union {
    From from;
    To to;
  };
};

template<typename To, typename From>
To unsafe_bit_cast(From from) {
    unsafe_bit_cast_t<To, From> u;
    u.from = from;
    return u.to;
}

static inline float fast_rsqrt_carmack(float x) {
  uint32_t i;
  float x2, y;
  const float threehalfs = 1.5f;
  y = x;
  i = unsafe_bit_cast<uint32_t, float>(y);
  i = 0x5f3759df - (i >> 1);
  y = unsafe_bit_cast<float, uint32_t>(i);
  x2 = x * 0.5f;
  y = y * (threehalfs - (x2 * y * y));
    return y;
}

static inline float fast_rsqrt_accurate(float fp0) {
  float _min = 1.0e-38;
  float _1p5 = 1.5;
  float fp1, fp2, fp3;

  uint32_t q = unsafe_bit_cast<uint32_t, float>(fp0);
  fp2 = unsafe_bit_cast<float, uint32_t>(0x5F3997BB - ((q >> 1) & 0x3FFFFFFF));
  fp1 = _1p5 * fp0 - fp0;
  fp3 = fp2 * fp2;
  if (fp0 < _min) {
    return fp0 > 0 ? fp2 : 1000.0f;
  }
  fp3 = _1p5 - fp1 * fp3;
  fp2 = fp2 * fp3;
  fp3 = fp2 * fp2;
  fp3 = _1p5 - fp1 * fp3;
  fp2 = fp2 * fp3;
  fp3 = fp2 * fp2;
  fp3 = _1p5 - fp1 * fp3;
  return fp2 * fp3;
}

// =============================================================================
// random.h + random.cc
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

// Was in random.cc. `inline` so every TU that pulls this header can define it
// without link-time duplicate-symbol errors.
inline uint32_t Random::rng_state_ = 0x21;

// =============================================================================
// parameter_interpolator.h
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

  inline float subsample(float t) {
    return value_ + increment_ * t;
  }

 private:
  float* state_;
  float value_;
  float increment_;
};

// =============================================================================
// units.h + units.cc
// =============================================================================

// These were `const float[]` in units.cc (external linkage). `static` here
// gives each TU its own private copy -- avoids duplicate-symbol link errors
// at the cost of a small amount of binary bloat.
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

inline float SemitonesToRatioSafe(float semitones) {
  float scale = 1.0f;
  while (semitones > 120.0f) {
    semitones -= 120.0f;
    scale *= 1024.0f;
  }
  while (semitones < -120.0f) {
    semitones += 120.0f;
    scale *= 1.0f / 1024.0f;
  }
  return scale * SemitonesToRatio(semitones);
}

inline float Exp2Safe(float value) {
  return SemitonesToRatioSafe(value * 12.0f);
}

// =============================================================================
// filter.h
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

class DCBlocker {
 public:
  DCBlocker() { }
  ~DCBlocker() { }

  void Init(float pole) {
    x_ = 0.0f;
    y_ = 0.0f;
    pole_ = pole;
  }

  inline void Process(float* in_out, size_t size) {
    float x = x_;
    float y = y_;
    const float pole = pole_;
    while (size--) {
      float old_x = x;
      x = *in_out;
      *in_out++ = y = y * pole + x - old_x;
    }
    x_ = x;
    y_ = y;
  }

 private:
  float pole_;
  float x_;
  float y_;
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

class NaiveSvf {
 public:
  NaiveSvf() { }
  ~NaiveSvf() { }

  void Init() {
    set_f_q<FREQUENCY_DIRTY>(0.01f, 100.0f);
    Reset();
  }

  void Reset() {
    lp_ = bp_ = 0.0f;
  }

  template<FrequencyApproximation approximation>
  inline void set_f_q(float f, float resonance) {
    if (approximation == FREQUENCY_EXACT) {
      f = f < 0.497f ? f : 0.497f;
      f_ = 2.0f * sinf(M_PI_F * f);
    } else {
      f = f < 0.158f ? f : 0.158f;
      f_ = 2.0f * M_PI_F * f;
    }
    damp_ = 1.0f / resonance;
  }

  template<FilterMode mode>
  inline float Process(float in) {
    float hp, notch, bp_normalized;
    bp_normalized = bp_ * damp_;
    notch = in - bp_normalized;
    lp_ += f_ * bp_;
    hp = notch - lp_;
    bp_ += f_ * hp;

    if (mode == FILTER_MODE_LOW_PASS) {
      return lp_;
    } else if (mode == FILTER_MODE_BAND_PASS) {
      return bp_;
    } else if (mode == FILTER_MODE_BAND_PASS_NORMALIZED) {
      return bp_normalized;
    } else if (mode == FILTER_MODE_HIGH_PASS) {
      return hp;
    }
    return 0.0f;
  }

  inline float lp() const { return lp_; }
  inline float bp() const { return bp_; }

  template<FilterMode mode>
  inline void Process(const float* in, float* out, size_t size) {
    float hp, notch, bp_normalized;
    float lp = lp_;
    float bp = bp_;
    while (size--) {
      bp_normalized = bp * damp_;
      notch = *in++ - bp_normalized;
      lp += f_ * bp;
      hp = notch - lp;
      bp += f_ * hp;

      if (mode == FILTER_MODE_LOW_PASS) {
        *out++ = lp;
      } else if (mode == FILTER_MODE_BAND_PASS) {
        *out++ = bp;
      } else if (mode == FILTER_MODE_BAND_PASS_NORMALIZED) {
        *out++ = bp_normalized;
      } else if (mode == FILTER_MODE_HIGH_PASS) {
        *out++ = hp;
      }
    }
    lp_ = lp;
    bp_ = bp;
  }

  inline void Split(const float* in, float* low, float* high, size_t size) {
    float hp, notch, bp_normalized;
    float lp = lp_;
    float bp = bp_;
    while (size--) {
      bp_normalized = bp * damp_;
      notch = *in++ - bp_normalized;
      lp += f_ * bp;
      hp = notch - lp;
      bp += f_ * hp;
      *low++ = lp;
      *high++ = hp;
    }
    lp_ = lp;
    bp_ = bp;
  }

  template<FilterMode mode>
  inline void Process(const float* in, float* out, size_t size, size_t decimate) {
    float hp, notch, bp_normalized;
    float lp = lp_;
    float bp = bp_;
    size_t n = decimate - 1;
    while (size--) {
      bp_normalized = bp * damp_;
      notch = *in++ - bp_normalized;
      lp += f_ * bp;
      hp = notch - lp;
      bp += f_ * hp;

      ++n;
      if (n == decimate) {
        if (mode == FILTER_MODE_LOW_PASS) {
          *out++ = lp;
        } else if (mode == FILTER_MODE_BAND_PASS) {
          *out++ = bp;
        } else if (mode == FILTER_MODE_BAND_PASS_NORMALIZED) {
          *out++ = bp_normalized;
        } else if (mode == FILTER_MODE_HIGH_PASS) {
          *out++ = hp;
        }
        n = 0;
      }
    }
    lp_ = lp;
    bp_ = bp;
  }

 private:
  float f_;
  float damp_;
  float lp_;
  float bp_;

  DISALLOW_COPY_AND_ASSIGN(NaiveSvf);
};

class ModifiedSvf {
 public:
  ModifiedSvf() { }
  ~ModifiedSvf() { }

  void Init() {
    Reset();
  }

  void Reset() {
    lp_ = bp_ = 0.0f;
  }

  inline void set_f_fq(float f, float fq) {
    f_ = f;
    fq_ = fq;
    x_ = 0.0f;
  }

  template<FilterMode mode>
  inline void Process(const float* in, float* out, size_t size) {
    float lp = lp_;
    float bp = bp_;
    float x = x_;
    const float fq = fq_;
    const float f = f_;
    while (size--) {
      lp += f * bp;
      bp += -fq * bp -f * lp + *in;
      if (mode == FILTER_MODE_BAND_PASS ||
          mode == FILTER_MODE_BAND_PASS_NORMALIZED) {
        bp += x;
      }
      x = *in++;

      if (mode == FILTER_MODE_LOW_PASS) {
        *out++ = lp * f;
      } else if (mode == FILTER_MODE_BAND_PASS) {
        *out++ = bp * f;
      } else if (mode == FILTER_MODE_BAND_PASS_NORMALIZED) {
        *out++ = bp * fq;
      } else if (mode == FILTER_MODE_HIGH_PASS) {
        *out++ = x - lp * f - bp * fq;
      }
    }
    lp_ = lp;
    bp_ = bp;
    x_ = x;
  }

 private:
  float f_;
  float fq_;
  float x_;
  float lp_;
  float bp_;

  DISALLOW_COPY_AND_ASSIGN(ModifiedSvf);
};

class CrossoverSvf {
 public:
  CrossoverSvf() { }
  ~CrossoverSvf() { }

  void Init() {
    Reset();
  }

  void Reset() {
    lp_[0] = bp_[0] = lp_[1] = bp_[1] = 0.0f;
    x_[0] = 0.0f;
    x_[1] = 0.0f;
  }

  inline void set_f_fq(float f, float fq) {
    f_ = f;
    fq_ = fq;
  }

  template<FilterMode mode>
  inline void Process(const float* in, float* out, size_t size) {
    float lp_1 = lp_[0];
    float bp_1 = bp_[0];
    float lp_2 = lp_[1];
    float bp_2 = bp_[1];
    float x_1 = x_[0];
    float x_2 = x_[1];
    const float fq = fq_;
    const float f = f_;
    while (size--) {
      lp_1 += f * bp_1;
      bp_1 += -fq * bp_1 -f * lp_1 + *in;
      if (mode == FILTER_MODE_BAND_PASS ||
          mode == FILTER_MODE_BAND_PASS_NORMALIZED) {
        bp_1 += x_1;
      }
      x_1 = *in++;

      float y;
      if (mode == FILTER_MODE_LOW_PASS) {
        y = lp_1 * f;
      } else if (mode == FILTER_MODE_BAND_PASS) {
        y = bp_1 * f;
      } else if (mode == FILTER_MODE_BAND_PASS_NORMALIZED) {
        y = bp_1 * fq;
      } else if (mode == FILTER_MODE_HIGH_PASS) {
        y = x_1 - lp_1 * f - bp_1 * fq;
      }

      lp_2 += f * bp_2;
      bp_2 += -fq * bp_2 -f * lp_2 + y;
      if (mode == FILTER_MODE_BAND_PASS ||
          mode == FILTER_MODE_BAND_PASS_NORMALIZED) {
        bp_2 += x_2;
      }
      x_2 = y;

      if (mode == FILTER_MODE_LOW_PASS) {
        *out++ = lp_2 * f;
      } else if (mode == FILTER_MODE_BAND_PASS) {
        *out++ = bp_2 * f;
      } else if (mode == FILTER_MODE_BAND_PASS_NORMALIZED) {
        *out++ = bp_2 * fq;
      } else if (mode == FILTER_MODE_HIGH_PASS) {
        *out++ = x_2 - lp_2 * f - bp_2 * fq;
      }
    }
    lp_[0] = lp_1;
    bp_[0] = bp_1;
    lp_[1] = lp_2;
    bp_[1] = bp_2;
    x_[0] = x_1;
    x_[1] = x_2;
  }

 private:
  float f_;
  float fq_;
  float x_[2];
  float lp_[2];
  float bp_[2];

  DISALLOW_COPY_AND_ASSIGN(CrossoverSvf);
};

// =============================================================================
// delay_line.h
// =============================================================================

template<typename T, size_t max_delay>
class DelayLine {
 public:
  DelayLine() { }
  ~DelayLine() { }

  void Init() {
    Reset();
  }

  void Reset() {
    std::fill(&line_[0], &line_[max_delay], T(0));
    delay_ = 1;
    write_ptr_ = 0;
  }

  inline void set_delay(size_t delay) {
    delay_ = delay;
  }

  inline void Write(const T sample) {
    line_[write_ptr_] = sample;
    write_ptr_ = (write_ptr_ - 1 + max_delay) % max_delay;
  }

  inline const T Allpass(const T sample, size_t delay, const T coefficient) {
    T read = line_[(write_ptr_ + delay) % max_delay];
    T write = sample + coefficient * read;
    Write(write);
    return -write * coefficient + read;
  }

  inline const T WriteRead(const T sample, float delay) {
    Write(sample);
    return Read(delay);
  }

  inline const T Read() const {
    return line_[(write_ptr_ + delay_) % max_delay];
  }

  inline const T Read(size_t delay) const {
    return line_[(write_ptr_ + delay) % max_delay];
  }

  inline const T Read(float delay) const {
    MAKE_INTEGRAL_FRACTIONAL(delay)
    const T a = line_[(write_ptr_ + delay_integral) % max_delay];
    const T b = line_[(write_ptr_ + delay_integral + 1) % max_delay];
    return a + (b - a) * delay_fractional;
  }

  inline const T ReadHermite(float delay) const {
    MAKE_INTEGRAL_FRACTIONAL(delay)
    int32_t t = (write_ptr_ + delay_integral + max_delay);
    const T xm1 = line_[(t - 1) % max_delay];
    const T x0 = line_[(t) % max_delay];
    const T x1 = line_[(t + 1) % max_delay];
    const T x2 = line_[(t + 2) % max_delay];
    const float c = (x1 - xm1) * 0.5f;
    const float v = x0 - x1;
    const float w = c + v;
    const float a = w + v + (x2 - x0) * 0.5f;
    const float b_neg = w + a;
    const float f = delay_fractional;
    return (((a * f) - b_neg) * f + c) * f + x0;
  }

 private:
  size_t write_ptr_;
  size_t delay_;
  T line_[max_delay];

  DISALLOW_COPY_AND_ASSIGN(DelayLine);
};

// =============================================================================
// buffer_allocator.h
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
