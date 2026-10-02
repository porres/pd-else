// Original copyright Emilie Gillet, MIT license.

#ifndef PLAITS_BASS_DRUM_H_
#define PLAITS_BASS_DRUM_H_

#include <stdint.h>
#include <stddef.h>
#include <math.h>

#ifndef NULL
#define NULL 0
#endif

// stmlib utilities
#define CONSTRAIN(var, min, max) \
  if (var < (min)) { \
    var = (min); \
  } else if (var > (max)) { \
    var = (max); \
  }

#define MAKE_INTEGRAL_FRACTIONAL(x) \
  int32_t x ## _integral = (int32_t)(x); \
  float x ## _fractional = (x) - (float)(x ## _integral);

#define ONE_POLE(out, in, coefficient) out += (coefficient) * ((in) - out);
#define SLOPE(out, in, positive, negative) { \
  float error = (in) - out; \
  out += (error > 0 ? positive : negative) * error; \
}

static inline float interpolate(const float* table, float index, float size) {
  index *= size;
  if (index == size) { index--; }
  MAKE_INTEGRAL_FRACTIONAL(index)
  if (!table || index_integral < 0) { return 0.0f; }
  float a = table[index_integral];
  float b = table[index_integral + 1];
  return a + (b - a) * index_fractional;
}

static inline float interpolate_wrap(const float* table, float index, float size) {
  index -= (float)((int32_t)index);
  index *= size;
  MAKE_INTEGRAL_FRACTIONAL(index)
  float a = table[index_integral];
  float b = table[index_integral + 1];
  return a + (b - a) * index_fractional;
}

static inline float soft_clip(float x) {
  if (x < -3.0f) {
    return -1.0f;
  } else if (x > 3.0f) {
    return 1.0f;
  } else {
      return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
  }
}

static inline int32_t clip16(int32_t x) {
  if (x < -32768) {
    return -32768;
  } else if (x > 32767) {
    return 32767;
  } else {
    return x;
  }
}

// Random
static uint32_t stmlib_rng_state = 0x21;

static inline uint32_t random_get_word(void) {
  stmlib_rng_state = stmlib_rng_state * 1664525L + 1013904223L;
  return stmlib_rng_state;
}

static inline float random_get_float(void) {
  return (float)random_get_word() / 4294967296.0f;
}

// ParameterInterpolator
typedef struct {
  float* state;
  float value;
  float increment;
} param_interp;

static inline void pi_init(param_interp* p, float* state, float new_value, size_t size) {
  p->state = state;
  p->value = *state;
  p->increment = (new_value - *state) / (float)size;
}

static inline float pi_next(param_interp* p) {
  p->value += p->increment;
  return p->value;
}

static inline void pi_finish(param_interp* p) {
  *p->state = p->value;
}

// units (SemitonesToRatio)
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

static inline float semitones_to_ratio(float semitones) {
  float pitch = semitones + 128.0f;
  MAKE_INTEGRAL_FRACTIONAL(pitch)
  return lut_pitch_ratio_high[pitch_integral] * \
      lut_pitch_ratio_low[(int32_t)(pitch_fractional * 256.0f)];
}

// filter (Svf)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define M_PI_F ((float)M_PI)
#define M_PI_POW_3 (M_PI_F * M_PI_F * M_PI_F)
#define M_PI_POW_5 (M_PI_POW_3 * M_PI_F * M_PI_F)

static inline float onepole_tan_dirty(float f) {
  const float a = 3.736e-01f * M_PI_POW_3;
  return f * (M_PI_F + a * f * f);
}

static inline float onepole_tan_fast(float f) {
  const float a = 3.260e-01f * M_PI_POW_3;
  const float b = 1.823e-01f * M_PI_POW_5;
  float f2 = f * f;
  return f * (M_PI_F + f2 * (a + b * f2));
}

typedef struct {
  float g;
  float r;
  float h;
  float state_1;
  float state_2;
} svf;

static inline void svf_reset(svf* s) {
  s->state_1 = 0.0f;
  s->state_2 = 0.0f;
}

static inline void svf_init(svf* s) {
  s->g = onepole_tan_dirty(0.01f);
  s->r = 1.0f / 100.0f;
  s->h = 1.0f / (1.0f + s->r * s->g + s->g * s->g);
  svf_reset(s);
}

static inline void svf_set_f_q_dirty(svf* s, float f, float resonance) {
  s->g = onepole_tan_dirty(f);
  s->r = 1.0f / resonance;
  s->h = 1.0f / (1.0f + s->r * s->g + s->g * s->g);
}

static inline void svf_set_f_q_fast(svf* s, float f, float resonance) {
  s->g = onepole_tan_fast(f);
  s->r = 1.0f / resonance;
  s->h = 1.0f / (1.0f + s->r * s->g + s->g * s->g);
}

static inline float svf_process_lp(svf* s, float in) {
  float hp = (in - s->r * s->state_1 - s->g * s->state_1 - s->state_2) * s->h;
  float bp = s->g * hp + s->state_1;
  s->state_1 = s->g * hp + bp;
  float lp = s->g * bp + s->state_2;
  s->state_2 = s->g * bp + lp;
  return lp;
}

static inline void svf_process_bp_lp(svf* s, float in, float* out_bp, float* out_lp) {
  float hp = (in - s->r * s->state_1 - s->g * s->state_1 - s->state_2) * s->h;
  float bp = s->g * hp + s->state_1;
  s->state_1 = s->g * hp + bp;
  float lp = s->g * bp + s->state_2;
  s->state_2 = s->g * bp + lp;
  *out_bp = bp;
  *out_lp = lp;
}

// Global constants + sine LUT
extern float kSampleRate;
extern float a0;

#define kSineLUTSize 512.0f

extern const float lut_sine[];

static inline float sine(float phase) {
  return interpolate_wrap(lut_sine, phase, kSineLUTSize);
}

static inline float sine_no_wrap(float phase) {
  return interpolate(lut_sine, phase, kSineLUTSize);
}

// SineOscillator
typedef struct {
  float phase;
} sine_oscillator;

static inline void sine_oscillator_init(sine_oscillator* o) {
  o->phase = 0.0f;
}

static inline void sine_oscillator_next(sine_oscillator* o, float frequency, float amplitude, float* out_sin, float* out_cos) {
  if (frequency >= 0.5f) {
    frequency = 0.5f;
  }
  o->phase += frequency;
  if (o->phase >= 1.0f) {
    o->phase -= 1.0f;
  }
  *out_sin = amplitude * sine_no_wrap(o->phase);
  *out_cos = amplitude * sine_no_wrap(o->phase + 0.25f);
}

// Overdrive
typedef struct {
  float pre_gain;
  float post_gain;
} overdrive;

static inline void overdrive_init(overdrive* o) {
  o->pre_gain = 0.0f;
  o->post_gain = 0.0f;
}

static inline void overdrive_process(overdrive* o, float drive, float* in_out, size_t size) {
  const float drive_2 = drive * drive;
  const float pre_gain_a = drive * 0.5f;
  const float pre_gain_b = drive_2 * drive_2 * drive * 24.0f;
  const float pre_gain = pre_gain_a + (pre_gain_b - pre_gain_a) * drive_2;
  const float drive_squashed = drive * (2.0f - drive);
  const float post_gain = 1.0f / soft_clip(0.33f + drive_squashed * (pre_gain - 0.33f));
  param_interp pre;
  param_interp post;
  pi_init(&pre, &o->pre_gain, pre_gain, size);
  pi_init(&post, &o->post_gain, post_gain, size);
  while (size--) {
    float p = pi_next(&pre) * *in_out;
    *in_out++ = soft_clip(p) * pi_next(&post);
  }
  pi_finish(&pre);
  pi_finish(&post);
}

// Engine parameters + trigger state
#define kMaxBlockSize 16
#define kBlockSize 8

#define TRIGGER_LOW          0
#define TRIGGER_RISING_EDGE  1
#define TRIGGER_UNPATCHED    2
#define TRIGGER_HIGH         4

static inline float note_to_frequency(float midi_note) {
  midi_note -= 9.0f;
  CONSTRAIN(midi_note, -128.0f, 127.0f);
  return a0 * 0.25f * semitones_to_ratio(midi_note);
}

typedef struct {
  int trigger;
  float note;
  float timbre;
  float morph;
  float harmonics;
  float accent;
} engine_parameters;

typedef struct {
  float out_gain;
  float aux_gain;
} post_processing_settings;

// =============================================================================
// AnalogBassDrum
// =============================================================================
typedef struct {
  int pulse_remaining_samples;
  int fm_pulse_remaining_samples;
  float pulse;
  float pulse_height;
  float pulse_lp;
  float fm_pulse_lp;
  float retrig_pulse;
  float lp_out;
  float tone_lp;
  float sustain_gain;
  svf resonator;
  sine_oscillator oscillator;
} analog_bass_drum;

static inline void analog_bass_drum_init(analog_bass_drum* d) {
  d->pulse_remaining_samples = 0;
  d->fm_pulse_remaining_samples = 0;
  d->pulse = 0.0f;
  d->pulse_height = 0.0f;
  d->pulse_lp = 0.0f;
  d->fm_pulse_lp = 0.0f;
  d->retrig_pulse = 0.0f;
  d->lp_out = 0.0f;
  d->tone_lp = 0.0f;
  d->sustain_gain = 0.0f;
  svf_init(&d->resonator);
  sine_oscillator_init(&d->oscillator);
}

static inline float analog_bass_drum_diode(float x) {
  if (x >= 0.0f) {
    return x;
  } else {
    x *= 2.0f;
    return 0.7f * x / (1.0f + fabsf(x));
  }
}

static inline void analog_bass_drum_render(
    analog_bass_drum* d,
    int sustain,
    int trigger,
    float accent,
    float f0,
    float tone,
    float decay,
    float attack_fm_amount,
    float self_fm_amount,
    float* out,
    size_t size) {
  const int trigger_pulse_duration = (int)(1.0e-3f * kSampleRate);
  const int fm_pulse_duration = (int)(6.0e-3f * kSampleRate);
  const float pulse_decay_time = 0.2e-3f * kSampleRate;
  const float pulse_filter_time = 0.1e-3f * kSampleRate;
  const float retrig_pulse_duration = 0.05f * kSampleRate;

  const float scale = 0.001f / f0;
  const float q = 1500.0f * semitones_to_ratio(decay * 80.0f);
  const float tone_f_raw = 4.0f * f0 * semitones_to_ratio(tone * 108.0f);
  const float tone_f = tone_f_raw < 1.0f ? tone_f_raw : 1.0f;
  const float exciter_leak = 0.08f * (tone + 0.25f);

  if (trigger) {
    d->pulse_remaining_samples = trigger_pulse_duration;
    d->fm_pulse_remaining_samples = fm_pulse_duration;
    d->pulse_height = 3.0f + 7.0f * accent;
    d->lp_out = 0.0f;
  }

  param_interp sustain_gain;
  pi_init(&sustain_gain, &d->sustain_gain, accent * decay, size);

  while (size--) {
    float pulse = 0.0f;
    if (d->pulse_remaining_samples) {
      --d->pulse_remaining_samples;
      pulse = d->pulse_remaining_samples ? d->pulse_height : d->pulse_height - 1.0f;
      d->pulse = pulse;
    } else {
      d->pulse *= 1.0f - 1.0f / pulse_decay_time;
      pulse = d->pulse;
    }
    if (sustain) {
      pulse = 0.0f;
    }

    ONE_POLE(d->pulse_lp, pulse, 1.0f / pulse_filter_time);
    pulse = analog_bass_drum_diode((pulse - d->pulse_lp) + pulse * 0.044f);

    float fm_pulse = 0.0f;
    if (d->fm_pulse_remaining_samples) {
      --d->fm_pulse_remaining_samples;
      fm_pulse = 1.0f;
      d->retrig_pulse = d->fm_pulse_remaining_samples ? 0.0f : -0.8f;
    } else {
      d->retrig_pulse *= 1.0f - 1.0f / retrig_pulse_duration;
    }
    if (sustain) {
      fm_pulse = 0.0f;
    }
    ONE_POLE(d->fm_pulse_lp, fm_pulse, 1.0f / pulse_filter_time);

    float punch = 0.7f + analog_bass_drum_diode(10.0f * d->lp_out - 1.0f);

    float attack_fm = d->fm_pulse_lp * 1.7f * attack_fm_amount;
    float self_fm = punch * 0.08f * self_fm_amount;
    float f = f0 * (1.0f + attack_fm + self_fm);
    CONSTRAIN(f, 0.0f, 0.4f);

    float resonator_out;
    if (sustain) {
      sine_oscillator_next(&d->oscillator, f, pi_next(&sustain_gain), &resonator_out, &d->lp_out);
    } else {
      svf_set_f_q_dirty(&d->resonator, f, 1.0f + q * f);
      svf_process_bp_lp(&d->resonator, (pulse - d->retrig_pulse * 0.2f) * scale, &resonator_out, &d->lp_out);
    }

    ONE_POLE(d->tone_lp, pulse * exciter_leak + resonator_out, tone_f);

    *out++ = d->tone_lp;
  }

  pi_finish(&sustain_gain);
}

// =============================================================================
// SyntheticBassDrum
// =============================================================================
typedef struct {
  float lp;
  float hp;
  svf filter;
} synthetic_bass_drum_click;

static inline void synthetic_bass_drum_click_init(synthetic_bass_drum_click* c) {
  c->lp = 0.0f;
  c->hp = 0.0f;
  svf_init(&c->filter);
  svf_set_f_q_fast(&c->filter, 5000.0f / kSampleRate, 2.0f);
}

static inline float synthetic_bass_drum_click_process(synthetic_bass_drum_click* c, float in) {
  SLOPE(c->lp, in, 0.5f, 0.1f);
  ONE_POLE(c->hp, c->lp, 0.04f);
  return svf_process_lp(&c->filter, c->lp - c->hp);
}

typedef struct {
  float lp;
  float hp;
} synthetic_bass_drum_attack_noise;

static inline void synthetic_bass_drum_attack_noise_init(synthetic_bass_drum_attack_noise* n) {
  n->lp = 0.0f;
  n->hp = 0.0f;
}

static inline float synthetic_bass_drum_attack_noise_render(synthetic_bass_drum_attack_noise* n) {
  float sample = random_get_float();
  ONE_POLE(n->lp, sample, 0.05f);
  ONE_POLE(n->hp, n->lp, 0.005f);
  return n->lp - n->hp;
}

typedef struct{
  float f0;
  float phase;
  float phase_noise;
  float fm;
  float fm_lp;
  float body_env;
  float body_env_lp;
  float transient_env;
  float transient_env_lp;
  float sustain_gain;
  float tone_lp;
  synthetic_bass_drum_click click;
  synthetic_bass_drum_attack_noise noise;
  int body_env_pulse_width;
  int fm_pulse_width;
}synthetic_bass_drum;

static inline void synthetic_bass_drum_init(synthetic_bass_drum* d){
  d->phase = 0.0f;
  d->phase_noise = 0.0f;
  d->f0 = 0.0f;
  d->fm = 0.0f;
  d->fm_lp = 0.0f;
  d->body_env_lp = 0.0f;
  d->body_env = 0.0f;
  d->body_env_pulse_width = 0;
  d->fm_pulse_width = 0;
  d->tone_lp = 0.0f;
  d->sustain_gain = 0.0f;
  synthetic_bass_drum_click_init(&d->click);
  synthetic_bass_drum_attack_noise_init(&d->noise);
}

static inline float synthetic_bass_drum_distorted_sine(float phase, float phase_noise, float dirtiness) {
  phase += phase_noise * dirtiness;
  MAKE_INTEGRAL_FRACTIONAL(phase);
  phase = phase_fractional;
  float triangle = (phase < 0.5f ? phase : 1.0f - phase) * 4.0f - 1.0f;
  float s = 2.0f * triangle / (1.0f + fabsf(triangle));
  float clean_sine = sine(phase + 0.75f);
  return s + (1.0f - dirtiness) * (clean_sine - s);
}

static inline float synthetic_bass_drum_transistor_vca(float s, float gain) {
  s = (s - 0.6f) * gain;
  return 3.0f * s / (2.0f + fabsf(s)) + gain * 0.3f;
}

static inline void synthetic_bass_drum_render(
    synthetic_bass_drum* d,
    int sustain,
    int trigger,
    float accent,
    float f0,
    float tone,
    float decay,
    float dirtiness,
    float fm_envelope_amount,
    float fm_envelope_decay,
    float* out,
    size_t size) {
  decay *= decay;
  fm_envelope_decay *= fm_envelope_decay;

  param_interp f0_mod;
  pi_init(&f0_mod, &d->f0, f0, size);

  {
    float max_d = 1.0f - 8.0f * f0;
    dirtiness *= max_d > 0.0f ? max_d : 0.0f;
  }

  const float fm_decay = 1.0f - \
      1.0f / (0.008f * (1.0f + fm_envelope_decay * 4.0f) * kSampleRate);

  const float body_env_decay = 1.0f - 1.0f / (0.02f * kSampleRate) * \
      semitones_to_ratio(-decay * 60.0f);
  const float transient_env_decay = 1.0f - 1.0f / (0.005f * kSampleRate);
  const float tone_f_raw = 4.0f * f0 * semitones_to_ratio(tone * 108.0f);
  const float tone_f = tone_f_raw < 1.0f ? tone_f_raw : 1.0f;
  const float transient_level = tone;

  if (trigger) {
    d->fm = 1.0f;
    d->body_env = d->transient_env = 0.3f + 0.7f * accent;
    d->body_env_pulse_width = (int)(kSampleRate * 0.001f);
    d->fm_pulse_width = (int)(kSampleRate * 0.0013f);
  }

  param_interp sustain_gain;
  pi_init(&sustain_gain, &d->sustain_gain, accent * decay, size);

  while (size--) {
    ONE_POLE(d->phase_noise, random_get_float() - 0.5f, 0.002f);
    float mix = 0.0f;
    if (sustain) {
      d->phase += pi_next(&f0_mod);
      if (d->phase >= 1.0f) {
        d->phase -= 1.0f;
      }
      float body = synthetic_bass_drum_distorted_sine(d->phase, d->phase_noise, dirtiness);
      mix -= synthetic_bass_drum_transistor_vca(body, pi_next(&sustain_gain));
    } else {
      if (d->fm_pulse_width) {
        --d->fm_pulse_width;
        d->phase = 0.25f;
      } else {
        d->fm *= fm_decay;
        float fm = 1.0f + fm_envelope_amount * 3.5f * d->fm_lp;
        float phase_inc = pi_next(&f0_mod) * fm;
        if (phase_inc > 0.5f) phase_inc = 0.5f;
        d->phase += phase_inc;
        if (d->phase >= 1.0f) {
          d->phase -= 1.0f;
        }
      }

      if (d->body_env_pulse_width) {
        --d->body_env_pulse_width;
      } else {
        d->body_env *= body_env_decay;
        d->transient_env *= transient_env_decay;
      }
      const float envelope_lp_f = 0.1f;
      ONE_POLE(d->body_env_lp, d->body_env, envelope_lp_f);
      ONE_POLE(d->transient_env_lp, d->transient_env, envelope_lp_f);
      ONE_POLE(d->fm_lp, d->fm, envelope_lp_f);
      float body = synthetic_bass_drum_distorted_sine(d->phase, d->phase_noise, dirtiness);
      float transient = synthetic_bass_drum_click_process(&d->click, d->body_env_pulse_width ? 0.0f : 1.0f)
                      + synthetic_bass_drum_attack_noise_render(&d->noise);
      mix -= synthetic_bass_drum_transistor_vca(body, d->body_env_lp);
      mix -= transient * d->transient_env_lp * transient_level;
    }
    ONE_POLE(d->tone_lp, mix, tone_f);
    *out++ = d->tone_lp;
  }
  pi_finish(&f0_mod);
  pi_finish(&sustain_gain);
}

// =============================================================================
// BassDrumEngine
// =============================================================================

typedef struct {
  analog_bass_drum analog_bass_drum;
  synthetic_bass_drum synthetic_bass_drum;
  overdrive overdrive;
  post_processing_settings post_processing_settings;
} bass_drum_engine;

static inline void bass_drum_engine_init(bass_drum_engine* e) {
  analog_bass_drum_init(&e->analog_bass_drum);
  synthetic_bass_drum_init(&e->synthetic_bass_drum);
  overdrive_init(&e->overdrive);
}

static inline void bass_drum_engine_render(
    bass_drum_engine* e,
    const engine_parameters* parameters,
    float* out,
    float* aux,
    size_t size) {
  const float f0 = note_to_frequency(parameters->note);

  {
    float a = parameters->harmonics * 4.0f;
    float b = parameters->harmonics * 4.0f - 1.0f;
    float c = parameters->harmonics * 2.0f - 1.0f;
    float d = 1.0f - 16.0f * f0;
    float attack_fm_amount = a < 1.0f ? a : 1.0f;
    float self_fm_amount = (b < 1.0f ? b : 1.0f);
    if (self_fm_amount < 0.0f) self_fm_amount = 0.0f;
    float drive = (c > 0.0f ? c : 0.0f) * (d > 0.0f ? d : 0.0f);

    const int sustain = parameters->trigger & TRIGGER_UNPATCHED;

    analog_bass_drum_render(
        &e->analog_bass_drum,
        sustain,
        parameters->trigger & TRIGGER_RISING_EDGE,
        parameters->accent,
        f0,
        parameters->timbre,
        parameters->morph,
        attack_fm_amount,
        self_fm_amount,
        out,
        size);

    overdrive_process(&e->overdrive, 0.5f + 0.5f * drive, out, size);

    {
      float d_aux = sustain
          ? parameters->harmonics
          : 0.4f - 0.25f * parameters->morph * parameters->morph;
      float fma = parameters->harmonics * 2.0f;
      float fmd = parameters->harmonics * 2.0f - 1.0f;
      synthetic_bass_drum_render(
          &e->synthetic_bass_drum,
          sustain,
          parameters->trigger & TRIGGER_RISING_EDGE,
          parameters->accent,
          f0,
          parameters->timbre,
          parameters->morph,
          d_aux,
          fma < 1.0f ? fma : 1.0f,
          fmd > 0.0f ? fmd : 0.0f,
          aux,
          size);
    }
  }
}

// ChannelPostProcessor
static inline void channel_post_processor_process(float gain, float* in, short* out, size_t size, size_t stride) {
  const float post_gain = (gain < 0.0f ? 1.0f : gain) * -32767.0f;
  while (size--) {
    *out = (short)clip16(1 + (int32_t)(*in++ * post_gain));
    out += stride;
  }
}

// =============================================================================
// Voice + support structs
// =============================================================================
typedef struct {
  float note;
  float harmonics;
  float timbre;
  float morph;
  float frequency_modulation_amount;
  float decay;
} patch;

typedef struct {
  float trigger;
  float level;
} modulations;

typedef struct {
  short out;
  short aux;
} voice_frame;

typedef struct {
  bass_drum_engine bass_drum_engine;
  int trigger_state;
  float decay_env;
  float out_buffer[kMaxBlockSize];
  float aux_buffer[kMaxBlockSize];
} voice;

static inline void voice_init(voice* v) {
  post_processing_settings* s = &v->bass_drum_engine.post_processing_settings;
  s->out_gain = 0.8f;
  s->aux_gain = 0.8f;

  bass_drum_engine_init(&v->bass_drum_engine);
  v->decay_env = 0.0f;
  v->trigger_state = 0;
}

static inline void voice_render(
    voice* v,
    const patch* p_patch,
    const modulations* p_mods,
    voice_frame* frames,
    size_t size) {
  float trigger_value = p_mods->trigger;
  int previous_trigger_state = v->trigger_state;

  if (!previous_trigger_state) {
    if (trigger_value > 0.3f) {
      v->trigger_state = 1;
      v->decay_env = 1.0f;
    }
  } else {
    if (trigger_value < 0.1f) {
      v->trigger_state = 0;
    }
  }

  bass_drum_engine* e = &v->bass_drum_engine;

  engine_parameters p;

  int rising_edge = v->trigger_state && !previous_trigger_state;
  const post_processing_settings* pp_s = &e->post_processing_settings;

  p.trigger = (rising_edge ? TRIGGER_RISING_EDGE : TRIGGER_LOW)
            | (v->trigger_state ? TRIGGER_HIGH : TRIGGER_LOW);

  const float short_decay = (200.0f * kBlockSize) / kSampleRate *
      semitones_to_ratio(-96.0f * p_patch->decay);

  v->decay_env *= (1.0f - short_decay * 2.0f);

  float compressed_level = 1.3f * p_mods->level / (0.3f + fabsf(p_mods->level));
  CONSTRAIN(compressed_level, 0.0f, 1.0f);
  p.accent = compressed_level;

  p.harmonics = p_patch->harmonics;
  CONSTRAIN(p.harmonics, 0.0f, 1.0f);

  {
    float env_val = v->decay_env;
    float mod_amt = p_patch->frequency_modulation_amount;
    float m = fabsf(mod_amt) - 0.05f;
    if (m < 0.05f) m = 0.05f;
    mod_amt *= m * 1.05f;

    p.note = p_patch->note + mod_amt * (env_val * env_val * 48.0f);
    CONSTRAIN(p.note, -119.0f, 120.0f);
  }

  p.timbre = p_patch->timbre;
  p.morph = p_patch->morph;

  bass_drum_engine_render(e, &p, v->out_buffer, v->aux_buffer, size);

  channel_post_processor_process(pp_s->out_gain, v->out_buffer, &frames->out, size, 2);
  channel_post_processor_process(pp_s->aux_gain, v->aux_buffer, &frames->aux, size, 2);
}

#endif  // PLAITS_BASS_DRUM_H_
