// Original copyright Emilie Gillet, MIT license.

#pragma once

#include <algorithm>
#include <math.h>
#include "stmlib.h"

namespace plaits {

// =============================================================================
// Global constants (were in resources.h + resources.cc)
// =============================================================================

inline float kSampleRate = 48000.0f;
inline float a0 = 55.0f / kSampleRate;

constexpr size_t kMaxBlockSize = 16;
constexpr size_t kBlockSize = 8;

// =============================================================================
// Sines
// =============================================================================

inline const float lut_sine[] = {
   0.000000000e+00,  1.227153829e-02,  2.454122852e-02,  3.680722294e-02,
   4.906767433e-02,  6.132073630e-02,  7.356456360e-02,  8.579731234e-02,
   9.801714033e-02,  1.102222073e-01,  1.224106752e-01,  1.345807085e-01,
   1.467304745e-01,  1.588581433e-01,  1.709618888e-01,  1.830398880e-01,
   1.950903220e-01,  2.071113762e-01,  2.191012402e-01,  2.310581083e-01,
   2.429801799e-01,  2.548656596e-01,  2.667127575e-01,  2.785196894e-01,
   2.902846773e-01,  3.020059493e-01,  3.136817404e-01,  3.253102922e-01,
   3.368898534e-01,  3.484186802e-01,  3.598950365e-01,  3.713171940e-01,
   3.826834324e-01,  3.939920401e-01,  4.052413140e-01,  4.164295601e-01,
   4.275550934e-01,  4.386162385e-01,  4.496113297e-01,  4.605387110e-01,
   4.713967368e-01,  4.821837721e-01,  4.928981922e-01,  5.035383837e-01,
   5.141027442e-01,  5.245896827e-01,  5.349976199e-01,  5.453249884e-01,
   5.555702330e-01,  5.657318108e-01,  5.758081914e-01,  5.857978575e-01,
   5.956993045e-01,  6.055110414e-01,  6.152315906e-01,  6.248594881e-01,
   6.343932842e-01,  6.438315429e-01,  6.531728430e-01,  6.624157776e-01,
   6.715589548e-01,  6.806009978e-01,  6.895405447e-01,  6.983762494e-01,
   7.071067812e-01,  7.157308253e-01,  7.242470830e-01,  7.326542717e-01,
   7.409511254e-01,  7.491363945e-01,  7.572088465e-01,  7.651672656e-01,
   7.730104534e-01,  7.807372286e-01,  7.883464276e-01,  7.958369046e-01,
   8.032075315e-01,  8.104571983e-01,  8.175848132e-01,  8.245893028e-01,
   8.314696123e-01,  8.382247056e-01,  8.448535652e-01,  8.513551931e-01,
   8.577286100e-01,  8.639728561e-01,  8.700869911e-01,  8.760700942e-01,
   8.819212643e-01,  8.876396204e-01,  8.932243012e-01,  8.986744657e-01,
   9.039892931e-01,  9.091679831e-01,  9.142097557e-01,  9.191138517e-01,
   9.238795325e-01,  9.285060805e-01,  9.329927988e-01,  9.373390119e-01,
   9.415440652e-01,  9.456073254e-01,  9.495281806e-01,  9.533060404e-01,
   9.569403357e-01,  9.604305194e-01,  9.637760658e-01,  9.669764710e-01,
   9.700312532e-01,  9.729399522e-01,  9.757021300e-01,  9.783173707e-01,
   9.807852804e-01,  9.831054874e-01,  9.852776424e-01,  9.873014182e-01,
   9.891765100e-01,  9.909026354e-01,  9.924795346e-01,  9.939069700e-01,
   9.951847267e-01,  9.963126122e-01,  9.972904567e-01,  9.981181129e-01,
   9.987954562e-01,  9.993223846e-01,  9.996988187e-01,  9.999247018e-01,
   1.000000000e+00,  9.999247018e-01,  9.996988187e-01,  9.993223846e-01,
   9.987954562e-01,  9.981181129e-01,  9.972904567e-01,  9.963126122e-01,
   9.951847267e-01,  9.939069700e-01,  9.924795346e-01,  9.909026354e-01,
   9.891765100e-01,  9.873014182e-01,  9.852776424e-01,  9.831054874e-01,
   9.807852804e-01,  9.783173707e-01,  9.757021300e-01,  9.729399522e-01,
   9.700312532e-01,  9.669764710e-01,  9.637760658e-01,  9.604305194e-01,
   9.569403357e-01,  9.533060404e-01,  9.495281806e-01,  9.456073254e-01,
   9.415440652e-01,  9.373390119e-01,  9.329927988e-01,  9.285060805e-01,
   9.238795325e-01,  9.191138517e-01,  9.142097557e-01,  9.091679831e-01,
   9.039892931e-01,  8.986744657e-01,  8.932243012e-01,  8.876396204e-01,
   8.819212643e-01,  8.760700942e-01,  8.700869911e-01,  8.639728561e-01,
   8.577286100e-01,  8.513551931e-01,  8.448535652e-01,  8.382247056e-01,
   8.314696123e-01,  8.245893028e-01,  8.175848132e-01,  8.104571983e-01,
   8.032075315e-01,  7.958369046e-01,  7.883464276e-01,  7.807372286e-01,
   7.730104534e-01,  7.651672656e-01,  7.572088465e-01,  7.491363945e-01,
   7.409511254e-01,  7.326542717e-01,  7.242470830e-01,  7.157308253e-01,
   7.071067812e-01,  6.983762494e-01,  6.895405447e-01,  6.806009978e-01,
   6.715589548e-01,  6.624157776e-01,  6.531728430e-01,  6.438315429e-01,
   6.343932842e-01,  6.248594881e-01,  6.152315906e-01,  6.055110414e-01,
   5.956993045e-01,  5.857978575e-01,  5.758081914e-01,  5.657318108e-01,
   5.555702330e-01,  5.453249884e-01,  5.349976199e-01,  5.245896827e-01,
   5.141027442e-01,  5.035383837e-01,  4.928981922e-01,  4.821837721e-01,
   4.713967368e-01,  4.605387110e-01,  4.496113297e-01,  4.386162385e-01,
   4.275550934e-01,  4.164295601e-01,  4.052413140e-01,  3.939920401e-01,
   3.826834324e-01,  3.713171940e-01,  3.598950365e-01,  3.484186802e-01,
   3.368898534e-01,  3.253102922e-01,  3.136817404e-01,  3.020059493e-01,
   2.902846773e-01,  2.785196894e-01,  2.667127575e-01,  2.548656596e-01,
   2.429801799e-01,  2.310581083e-01,  2.191012402e-01,  2.071113762e-01,
   1.950903220e-01,  1.830398880e-01,  1.709618888e-01,  1.588581433e-01,
   1.467304745e-01,  1.345807085e-01,  1.224106752e-01,  1.102222073e-01,
   9.801714033e-02,  8.579731234e-02,  7.356456360e-02,  6.132073630e-02,
   4.906767433e-02,  3.680722294e-02,  2.454122852e-02,  1.227153829e-02,
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
  -2.449293598e-16,  1.227153829e-02,  2.454122852e-02,  3.680722294e-02,
   4.906767433e-02,  6.132073630e-02,  7.356456360e-02,  8.579731234e-02,
   9.801714033e-02,  1.102222073e-01,  1.224106752e-01,  1.345807085e-01,
   1.467304745e-01,  1.588581433e-01,  1.709618888e-01,  1.830398880e-01,
   1.950903220e-01,  2.071113762e-01,  2.191012402e-01,  2.310581083e-01,
   2.429801799e-01,  2.548656596e-01,  2.667127575e-01,  2.785196894e-01,
   2.902846773e-01,  3.020059493e-01,  3.136817404e-01,  3.253102922e-01,
   3.368898534e-01,  3.484186802e-01,  3.598950365e-01,  3.713171940e-01,
   3.826834324e-01,  3.939920401e-01,  4.052413140e-01,  4.164295601e-01,
   4.275550934e-01,  4.386162385e-01,  4.496113297e-01,  4.605387110e-01,
   4.713967368e-01,  4.821837721e-01,  4.928981922e-01,  5.035383837e-01,
   5.141027442e-01,  5.245896827e-01,  5.349976199e-01,  5.453249884e-01,
   5.555702330e-01,  5.657318108e-01,  5.758081914e-01,  5.857978575e-01,
   5.956993045e-01,  6.055110414e-01,  6.152315906e-01,  6.248594881e-01,
   6.343932842e-01,  6.438315429e-01,  6.531728430e-01,  6.624157776e-01,
   6.715589548e-01,  6.806009978e-01,  6.895405447e-01,  6.983762494e-01,
   7.071067812e-01,  7.157308253e-01,  7.242470830e-01,  7.326542717e-01,
   7.409511254e-01,  7.491363945e-01,  7.572088465e-01,  7.651672656e-01,
   7.730104534e-01,  7.807372286e-01,  7.883464276e-01,  7.958369046e-01,
   8.032075315e-01,  8.104571983e-01,  8.175848132e-01,  8.245893028e-01,
   8.314696123e-01,  8.382247056e-01,  8.448535652e-01,  8.513551931e-01,
   8.577286100e-01,  8.639728561e-01,  8.700869911e-01,  8.760700942e-01,
   8.819212643e-01,  8.876396204e-01,  8.932243012e-01,  8.986744657e-01,
   9.039892931e-01,  9.091679831e-01,  9.142097557e-01,  9.191138517e-01,
   9.238795325e-01,  9.285060805e-01,  9.329927988e-01,  9.373390119e-01,
   9.415440652e-01,  9.456073254e-01,  9.495281806e-01,  9.533060404e-01,
   9.569403357e-01,  9.604305194e-01,  9.637760658e-01,  9.669764710e-01,
   9.700312532e-01,  9.729399522e-01,  9.757021300e-01,  9.783173707e-01,
   9.807852804e-01,  9.831054874e-01,  9.852776424e-01,  9.873014182e-01,
   9.891765100e-01,  9.909026354e-01,  9.924795346e-01,  9.939069700e-01,
   9.951847267e-01,  9.963126122e-01,  9.972904567e-01,  9.981181129e-01,
   9.987954562e-01,  9.993223846e-01,  9.996988187e-01,  9.999247018e-01,
   1.000000000e+00,
};

const float kSineLUTSize = 512.0f;

inline float Sine(float phase) {
  return stmlib::InterpolateWrap(lut_sine, phase, kSineLUTSize);
}

inline float SineNoWrap(float phase) {
  return stmlib::Interpolate(lut_sine, phase, kSineLUTSize);
}

class SineOscillator {
 public:
  SineOscillator() { }
  ~SineOscillator() { }

  void Init() {
    phase_ = 0.0f;
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

 private:
  float phase_;

  DISALLOW_COPY_AND_ASSIGN(SineOscillator);
};

// =============================================================================
// Overdrive
// =============================================================================

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
        &pre_gain_,
        pre_gain,
        size);

    stmlib::ParameterInterpolator post_gain_modulation(
        &post_gain_,
        post_gain,
        size);

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

// =============================================================================
// Envelopes
// =============================================================================

class DecayEnvelope {
 public:
  DecayEnvelope() { }
  ~DecayEnvelope() { }

  inline void Init() {
    value_ = 0.0f;
  }

  inline void Trigger() {
    value_ = 1.0f;
  }

  inline void Process(float decay) {
    value_ *= (1.0f - decay);
  }

  inline float value() const { return value_; }

 private:
  float value_;

  DISALLOW_COPY_AND_ASSIGN(DecayEnvelope);
};

// =============================================================================
// Engine parameters + trigger state
// =============================================================================

inline float NoteToFrequency(float midi_note) {
  midi_note -= 9.0f;
  CONSTRAIN(midi_note, -128.0f, 127.0f);
  return a0 * 0.25f * stmlib::SemitonesToRatio(midi_note);
}

enum TriggerState {
  TRIGGER_LOW = 0,
  TRIGGER_RISING_EDGE = 1,
  TRIGGER_UNPATCHED = 2,
  TRIGGER_HIGH = 4,
};

struct EngineParameters {
  int trigger;
  float note;
  float timbre;
  float morph;
  float harmonics;
  float accent;
};

struct PostProcessingSettings {
  float out_gain;
  float aux_gain;
  bool already_enveloped;
};

// =============================================================================
// Analog bass drum
// =============================================================================

class AnalogBassDrum {
 public:
  AnalogBassDrum() { }
  ~AnalogBassDrum() { }

  void Init() {
    pulse_remaining_samples_ = 0;
    fm_pulse_remaining_samples_ = 0;
    pulse_ = 0.0f;
    pulse_height_ = 0.0f;
    pulse_lp_ = 0.0f;
    fm_pulse_lp_ = 0.0f;
    retrig_pulse_ = 0.0f;
    lp_out_ = 0.0f;
    tone_lp_ = 0.0f;
    sustain_gain_ = 0.0f;

    resonator_.Init();
    oscillator_.Init();
  }

  inline float Diode(float x) {
    if (x >= 0.0f) {
      return x;
    } else {
      x *= 2.0f;
      return 0.7f * x / (1.0f + fabsf(x));
    }
  }

  void Render(
      bool sustain,
      bool trigger,
      float accent,
      float f0,
      float tone,
      float decay,
      float attack_fm_amount,
      float self_fm_amount,
      float* out,
      size_t size) {
    const int kTriggerPulseDuration = 1.0e-3f * kSampleRate;
    const int kFMPulseDuration = 6.0e-3f * kSampleRate;
    const float kPulseDecayTime = 0.2e-3f * kSampleRate;
    const float kPulseFilterTime = 0.1e-3f * kSampleRate;
    const float kRetrigPulseDuration = 0.05f * kSampleRate;

    const float scale = 0.001f / f0;
    const float q = 1500.0f * stmlib::SemitonesToRatio(decay * 80.0f);
    const float tone_f = std::min(
        4.0f * f0 * stmlib::SemitonesToRatio(tone * 108.0f),
        1.0f);
    const float exciter_leak = 0.08f * (tone + 0.25f);

    if (trigger) {
      pulse_remaining_samples_ = kTriggerPulseDuration;
      fm_pulse_remaining_samples_ = kFMPulseDuration;
      pulse_height_ = 3.0f + 7.0f * accent;
      lp_out_ = 0.0f;
    }

    stmlib::ParameterInterpolator sustain_gain(
        &sustain_gain_,
        accent * decay,
        size);

    while (size--) {
      float pulse = 0.0f;
      if (pulse_remaining_samples_) {
        --pulse_remaining_samples_;
        pulse = pulse_remaining_samples_ ? pulse_height_ : pulse_height_ - 1.0f;
        pulse_ = pulse;
      } else {
        pulse_ *= 1.0f - 1.0f / kPulseDecayTime;
        pulse = pulse_;
      }
      if (sustain) {
        pulse = 0.0f;
      }

      ONE_POLE(pulse_lp_, pulse, 1.0f / kPulseFilterTime);
      pulse = Diode((pulse - pulse_lp_) + pulse * 0.044f);

      float fm_pulse = 0.0f;
      if (fm_pulse_remaining_samples_) {
        --fm_pulse_remaining_samples_;
        fm_pulse = 1.0f;
        retrig_pulse_ = fm_pulse_remaining_samples_ ? 0.0f : -0.8f;
      } else {
        retrig_pulse_ *= 1.0f - 1.0f / kRetrigPulseDuration;
      }
      if (sustain) {
        fm_pulse = 0.0f;
      }
      ONE_POLE(fm_pulse_lp_, fm_pulse, 1.0f / kPulseFilterTime);

      float punch = 0.7f + Diode(10.0f * lp_out_ - 1.0f);

      float attack_fm = fm_pulse_lp_ * 1.7f * attack_fm_amount;
      float self_fm = punch * 0.08f * self_fm_amount;
      float f = f0 * (1.0f + attack_fm + self_fm);
      CONSTRAIN(f, 0.0f, 0.4f);

      float resonator_out;
      if (sustain) {
        oscillator_.Next(f, sustain_gain.Next(), &resonator_out, &lp_out_);
      } else {
        resonator_.set_f_q<stmlib::FREQUENCY_DIRTY>(f, 1.0f + q * f);
        resonator_.Process<stmlib::FILTER_MODE_BAND_PASS,
                           stmlib::FILTER_MODE_LOW_PASS>(
            (pulse - retrig_pulse_ * 0.2f) * scale,
            &resonator_out,
            &lp_out_);
      }

      ONE_POLE(tone_lp_, pulse * exciter_leak + resonator_out, tone_f);

      *out++ = tone_lp_;
    }
  }

 private:
  int pulse_remaining_samples_;
  int fm_pulse_remaining_samples_;
  float pulse_;
  float pulse_height_;
  float pulse_lp_;
  float fm_pulse_lp_;
  float retrig_pulse_;
  float lp_out_;
  float tone_lp_;
  float sustain_gain_;

  stmlib::Svf resonator_;
  SineOscillator oscillator_;

  DISALLOW_COPY_AND_ASSIGN(AnalogBassDrum);
};

// =============================================================================
// Synthetic bass drum
// =============================================================================

class SyntheticBassDrumClick {
 public:
  SyntheticBassDrumClick() { }
  ~SyntheticBassDrumClick() { }

  void Init() {
    lp_ = 0.0f;
    hp_ = 0.0f;
    filter_.Init();
    filter_.set_f_q<stmlib::FREQUENCY_FAST>(5000.0f / kSampleRate, 2.0f);
  }

  float Process(float in) {
    SLOPE(lp_, in, 0.5f, 0.1f);
    ONE_POLE(hp_, lp_, 0.04f);
    return filter_.Process<stmlib::FILTER_MODE_LOW_PASS>(lp_ - hp_);
  }

 private:
  float lp_;
  float hp_;
  stmlib::Svf filter_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticBassDrumClick);
};

class SyntheticBassDrumAttackNoise {
 public:
  SyntheticBassDrumAttackNoise() { }
  ~SyntheticBassDrumAttackNoise() { }

  void Init() {
    lp_ = 0.0f;
    hp_ = 0.0f;
  }

  float Render() {
    float sample = stmlib::Random::GetFloat();
    ONE_POLE(lp_, sample, 0.05f);
    ONE_POLE(hp_, lp_, 0.005f);
    return lp_ - hp_;
  }

 private:
  float lp_;
  float hp_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticBassDrumAttackNoise);
};

class SyntheticBassDrum {
 public:
  SyntheticBassDrum() { }
  ~SyntheticBassDrum() { }

  void Init() {
    phase_ = 0.0f;
    phase_noise_ = 0.0f;
    f0_ = 0.0f;
    fm_ = 0.0f;
    fm_lp_ = 0.0f;
    body_env_lp_ = 0.0f;
    body_env_ = 0.0f;
    body_env_pulse_width_ = 0;
    fm_pulse_width_ = 0;
    tone_lp_ = 0.0f;
    sustain_gain_ = 0.0f;

    click_.Init();
    noise_.Init();
  }

  inline float DistortedSine(float phase, float phase_noise, float dirtiness) {
    phase += phase_noise * dirtiness;
    MAKE_INTEGRAL_FRACTIONAL(phase);
    phase = phase_fractional;
    float triangle = (phase < 0.5f ? phase : 1.0f - phase) * 4.0f - 1.0f;
    float sine = 2.0f * triangle / (1.0f + fabsf(triangle));
    float clean_sine = Sine(phase + 0.75f);
    return sine + (1.0f - dirtiness) * (clean_sine - sine);
  }

  inline float TransistorVCA(float s, float gain) {
    s = (s - 0.6f) * gain;
    return 3.0f * s / (2.0f + fabsf(s)) + gain * 0.3f;
  }

  void Render(
      bool sustain,
      bool trigger,
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

    stmlib::ParameterInterpolator f0_mod(&f0_, f0, size);

    dirtiness *= std::max(1.0f - 8.0f * f0, 0.0f);

    const float fm_decay = 1.0f - \
        1.0f / (0.008f * (1.0f + fm_envelope_decay * 4.0f) * kSampleRate);

    const float body_env_decay = 1.0f - 1.0f / (0.02f * kSampleRate) * \
        stmlib::SemitonesToRatio(-decay * 60.0f);
    const float transient_env_decay = 1.0f - 1.0f / (0.005f * kSampleRate);
    const float tone_f = std::min(
        4.0f * f0 * stmlib::SemitonesToRatio(tone * 108.0f),
        1.0f);
    const float transient_level = tone;

    if (trigger) {
      fm_ = 1.0f;
      body_env_ = transient_env_ = 0.3f + 0.7f * accent;
      body_env_pulse_width_ = kSampleRate * 0.001f;
      fm_pulse_width_ = kSampleRate * 0.0013f;
    }

    stmlib::ParameterInterpolator sustain_gain(
        &sustain_gain_,
        accent * decay,
        size);

    while (size--) {
      ONE_POLE(phase_noise_, stmlib::Random::GetFloat() - 0.5f, 0.002f);

      float mix = 0.0f;

      if (sustain) {
        phase_ += f0_mod.Next();
        if (phase_ >= 1.0f) {
          phase_ -= 1.0f;
        }
        float body = DistortedSine(phase_, phase_noise_, dirtiness);
        mix -= TransistorVCA(body, sustain_gain.Next());
      } else {
        if (fm_pulse_width_) {
          --fm_pulse_width_;
          phase_ = 0.25f;
        } else {
          fm_ *= fm_decay;
          float fm = 1.0f + fm_envelope_amount * 3.5f * fm_lp_;
          phase_ += std::min(f0_mod.Next() * fm, 0.5f);
          if (phase_ >= 1.0f) {
            phase_ -= 1.0f;
          }
        }

        if (body_env_pulse_width_) {
          --body_env_pulse_width_;
        } else {
          body_env_ *= body_env_decay;
          transient_env_ *= transient_env_decay;
        }

        const float envelope_lp_f = 0.1f;
        ONE_POLE(body_env_lp_, body_env_, envelope_lp_f);
        ONE_POLE(transient_env_lp_, transient_env_, envelope_lp_f);
        ONE_POLE(fm_lp_, fm_, envelope_lp_f);

        float body = DistortedSine(phase_, phase_noise_, dirtiness);
        float transient = click_.Process(
            body_env_pulse_width_ ? 0.0f : 1.0f) + noise_.Render();

        mix -= TransistorVCA(body, body_env_lp_);
        mix -= transient * transient_env_lp_ * transient_level;
      }

      ONE_POLE(tone_lp_, mix, tone_f);
      *out++ = tone_lp_;
    }
  }

 private:
  float f0_;
  float phase_;
  float phase_noise_;

  float fm_;
  float fm_lp_;
  float body_env_;
  float body_env_lp_;
  float transient_env_;
  float transient_env_lp_;

  float sustain_gain_;

  float tone_lp_;

  SyntheticBassDrumClick click_;
  SyntheticBassDrumAttackNoise noise_;

  int body_env_pulse_width_;
  int fm_pulse_width_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticBassDrum);
};

// =============================================================================
// BassDrumEngine
// =============================================================================

class BassDrumEngine {
 public:
  BassDrumEngine() { }
  ~BassDrumEngine() { }

  void Init() {
    analog_bass_drum_.Init();
    synthetic_bass_drum_.Init();
    overdrive_.Init();
  }

  void Render(
      const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size) {
    const float f0 = NoteToFrequency(parameters.note);

    const float attack_fm_amount = std::min(parameters.harmonics * 4.0f, 1.0f);
    const float self_fm_amount = std::max(std::min(parameters.harmonics * 4.0f - 1.0f, 1.0f), 0.0f);
    const float drive = std::max(parameters.harmonics * 2.0f - 1.0f, 0.0f) * \
        std::max(1.0f - 16.0f * f0, 0.0f);

    const bool sustain = parameters.trigger & TRIGGER_UNPATCHED;

    analog_bass_drum_.Render(
        sustain,
        parameters.trigger & TRIGGER_RISING_EDGE,
        parameters.accent,
        f0,
        parameters.timbre,
        parameters.morph,
        attack_fm_amount,
        self_fm_amount,
        out,
        size);

    overdrive_.Process(
        0.5f + 0.5f * drive,
        out,
        size);

    synthetic_bass_drum_.Render(
        sustain,
        parameters.trigger & TRIGGER_RISING_EDGE,
        parameters.accent,
        f0,
        parameters.timbre,
        parameters.morph,
        sustain
            ? parameters.harmonics
            : 0.4f - 0.25f * parameters.morph * parameters.morph,
        std::min(parameters.harmonics * 2.0f, 1.0f),
        std::max(parameters.harmonics * 2.0f - 1.0f, 0.0f),
        aux,
        size);
  }

  PostProcessingSettings post_processing_settings;

 private:
  AnalogBassDrum analog_bass_drum_;
  SyntheticBassDrum synthetic_bass_drum_;

  Overdrive overdrive_;

  DISALLOW_COPY_AND_ASSIGN(BassDrumEngine);
};

// =============================================================================
// Voice + support structs
// =============================================================================

class ChannelPostProcessor {
 public:
  ChannelPostProcessor() { }
  ~ChannelPostProcessor() { }

  void Process(float gain, float* in, short* out, size_t size, size_t stride) {
    const float post_gain = (gain < 0.0f ? 1.0f : gain) * -32767.0f;
    while (size--) {
      *out = stmlib::Clip16(1 + static_cast<int32_t>(*in++ * post_gain));
      out += stride;
    }
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(ChannelPostProcessor);
};

struct Patch {
  float note;
  float harmonics;
  float timbre;
  float morph;
  float frequency_modulation_amount;
  float timbre_modulation_amount;
  float morph_modulation_amount;
  float decay;
};

struct Modulations {
  float trigger;
  float level;
  bool level_patched;
};

class Voice {
 public:
  Voice() {}
  ~Voice() {}

  struct Frame {
    short out;
    short aux;
  };

  void Init() {
    PostProcessingSettings* s = &bass_drum_engine_.post_processing_settings;
    s->already_enveloped = true;
    s->out_gain = 0.8f;
    s->aux_gain = 0.8f;

    bass_drum_engine_.Init();

    decay_envelope_.Init();

    trigger_state_ = false;
  }

  void Render(
      const Patch& patch,
      const Modulations& modulations,
      Frame* frames,
      size_t size) {
    float trigger_value = modulations.trigger;

    bool previous_trigger_state = trigger_state_;
    if (!previous_trigger_state) {
      if (trigger_value > 0.3f) {
        trigger_state_ = true;
        decay_envelope_.Trigger();
      }
    } else {
      if (trigger_value < 0.1f) {
        trigger_state_ = false;
      }
    }

    BassDrumEngine* e = &bass_drum_engine_;

    EngineParameters p;

    bool rising_edge = trigger_state_ && !previous_trigger_state;
    const PostProcessingSettings& pp_s = e->post_processing_settings;

    p.trigger = (rising_edge ? TRIGGER_RISING_EDGE : TRIGGER_LOW) | \
                (trigger_state_ ? TRIGGER_HIGH : TRIGGER_LOW);

    const float short_decay = (200.0f * kBlockSize) / kSampleRate *
        stmlib::SemitonesToRatio(-96.0f * patch.decay);

    decay_envelope_.Process(short_decay * 2.0f);

    float compressed_level = 1.3f * modulations.level / (0.3f + fabsf(modulations.level));
    CONSTRAIN(compressed_level, 0.0f, 1.0f);
    p.accent = modulations.level_patched ? compressed_level : 0.8f;

    const bool use_internal_envelope = true;

    p.harmonics = patch.harmonics;
    CONSTRAIN(p.harmonics, 0.0f, 1.0f);

    float internal_envelope_amplitude = 1.0f;
    float internal_envelope_amplitude_timbre = 1.0f;

    p.note = ApplyModulations(
        patch.note,
        patch.frequency_modulation_amount,
        false,
        0.0f,
        use_internal_envelope,
        internal_envelope_amplitude * \
            decay_envelope_.value() * decay_envelope_.value() * 48.0f,
        1.0f,
        -119.0f,
        120.0f);

    p.timbre = ApplyModulations(
        patch.timbre,
        patch.timbre_modulation_amount,
        false,
        0.0f,
        use_internal_envelope,
        internal_envelope_amplitude_timbre * decay_envelope_.value(),
        0.0f,
        0.0f,
        1.0f);

    p.morph = ApplyModulations(
        patch.morph,
        patch.morph_modulation_amount,
        false,
        0.0f,
        use_internal_envelope,
        internal_envelope_amplitude * decay_envelope_.value(),
        0.0f,
        0.0f,
        1.0f);

    e->Render(p, out_buffer_, aux_buffer_, size);

    out_post_processor_.Process(pp_s.out_gain, out_buffer_, &frames->out, size, 2);
    aux_post_processor_.Process(pp_s.aux_gain, aux_buffer_, &frames->aux, size, 2);
  }

 private:
  inline float ApplyModulations(
      float base_value,
      float modulation_amount,
      bool use_external_modulation,
      float external_modulation,
      bool use_internal_envelope,
      float envelope,
      float default_internal_modulation,
      float minimum_value,
      float maximum_value) {
    float value = base_value;
    modulation_amount *= std::max(fabsf(modulation_amount) - 0.05f, 0.05f);
    modulation_amount *= 1.05f;

    float modulation = use_external_modulation
        ? external_modulation
        : (use_internal_envelope ? envelope : default_internal_modulation);
    value += modulation_amount * modulation;
    CONSTRAIN(value, minimum_value, maximum_value);
    return value;
  }

  BassDrumEngine bass_drum_engine_;

  bool trigger_state_;

  DecayEnvelope decay_envelope_;

  ChannelPostProcessor out_post_processor_;
  ChannelPostProcessor aux_post_processor_;

  float out_buffer_[kMaxBlockSize];
  float aux_buffer_[kMaxBlockSize];

  DISALLOW_COPY_AND_ASSIGN(Voice);
};

}  // namespace plaits
