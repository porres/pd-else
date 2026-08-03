// Copyright 2016 Emilie Gillet.
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// 808-style HH (two noise sources) and the engine that combines them.
//
// This file merges (in order):
//   - polyblep.h      (stmlib::ThisBlepSample, NextBlepSample,
//                       ThisIntegratedBlepSample, NextIntegratedBlepSample)
//   - oscillator.h    (plaits::OscillatorShape, plaits::Oscillator)
//   - hi_hat.h        (SquareNoise, RingModNoise, SwingVCA, LinearVCA, HiHat<>)
//   - hi_hat_engine.h (HiHatEngine declaration)
//   - hi_hat_engine.cc (HiHatEngine implementation)

#ifndef PLAITS_DSP_ENGINE_HI_HAT_ENGINE_MERGED_H_
#define PLAITS_DSP_ENGINE_HI_HAT_ENGINE_MERGED_H_

#include "plaits/stmlib.h"
#include "plaits/engine/engine.h"

// ---------------------------------------------------------------------------
// From polyblep.h
// Polynomial approximation of band-limited step for band-limited waveform
// synthesis.
// ---------------------------------------------------------------------------

namespace stmlib {

inline float ThisBlepSample(float t) {
  return 0.5f * t * t;
}

inline float NextBlepSample(float t) {
  t = 1.0f - t;
  return -0.5f * t * t;
}

inline float NextIntegratedBlepSample(float t) {
  const float t1 = 0.5f * t;
  const float t2 = t1 * t1;
  const float t4 = t2 * t2;
  return 0.1875f - t1 + 1.5f * t2 - t4;
}

inline float ThisIntegratedBlepSample(float t) {
  return NextIntegratedBlepSample(1.0f - t);
}

}  // namespace stmlib

namespace plaits {

// ---------------------------------------------------------------------------
// From oscillator.h
// Single waveform oscillator. Can optionally do audio-rate linear FM, with
// through-zero capabilities (negative frequencies).
// ---------------------------------------------------------------------------

enum OscillatorShape {
  OSCILLATOR_SHAPE_IMPULSE_TRAIN,
  OSCILLATOR_SHAPE_SAW,
  OSCILLATOR_SHAPE_TRIANGLE,
  OSCILLATOR_SHAPE_SLOPE,
  OSCILLATOR_SHAPE_SQUARE,
  OSCILLATOR_SHAPE_SQUARE_BRIGHT,
  OSCILLATOR_SHAPE_SQUARE_DARK,
  OSCILLATOR_SHAPE_SQUARE_TRIANGLE
};

const float kMaxFrequency = 0.25f;
const float kMinFrequency = 0.000001f;

class Oscillator {
 public:
  Oscillator() { }
  ~Oscillator() { }

  void Init() {
    phase_ = 0.5f;
    next_sample_ = 0.0f;
    lp_state_ = 1.0f;
    hp_state_ = 0.0f;
    high_ = true;

    frequency_ = 0.001f;
    pw_ = 0.5f;
  }

  template<OscillatorShape shape>
  void Render(float frequency, float pw, float* out, size_t size) {
    Render<shape, false, false>(frequency, pw, NULL, out, size);
  }

  template<OscillatorShape shape>
  void Render(
      float frequency,
      float pw,
      const float* fm,
      float* out,
      size_t size) {
    if (!fm) {
      Render<shape, false, false>(frequency, pw, NULL, out, size);
    } else {
      Render<shape, true, true>(frequency, pw, fm, out, size);
    }
  }

  template<OscillatorShape shape, bool has_external_fm, bool through_zero_fm>
  void Render(
      float frequency,
      float pw,
      const float* external_fm,
      float* out,
      size_t size) {

    if (!has_external_fm) {
      if (!through_zero_fm) {
        CONSTRAIN(frequency, kMinFrequency, kMaxFrequency);
      } else {
        CONSTRAIN(frequency, -kMaxFrequency, kMaxFrequency);
      }
      CONSTRAIN(pw, fabsf(frequency) * 2.0f, 1.0f - 2.0f * fabsf(frequency))
    }

    stmlib::ParameterInterpolator fm(&frequency_, frequency, size);
    stmlib::ParameterInterpolator pwm(&pw_, pw, size);

    float next_sample = next_sample_;

    while (size--) {
      float this_sample = next_sample;
      next_sample = 0.0f;

      float frequency = fm.Next();
      if (has_external_fm) {
        frequency *= (1.0f + *external_fm++);
        if (!through_zero_fm) {
          CONSTRAIN(frequency, kMinFrequency, kMaxFrequency);
        } else {
          CONSTRAIN(frequency, -kMaxFrequency, kMaxFrequency);
        }
      }
      float pw = (shape == OSCILLATOR_SHAPE_SQUARE_TRIANGLE ||
        shape == OSCILLATOR_SHAPE_TRIANGLE) ? 0.5f : pwm.Next();
      if (has_external_fm) {
        CONSTRAIN(pw, fabsf(frequency) * 2.0f, 1.0f - 2.0f * fabsf(frequency))
      }
      phase_ += frequency;

      if (shape <= OSCILLATOR_SHAPE_SAW) {
        if (phase_ >= 1.0f) {
          phase_ -= 1.0f;
          float t = phase_ / frequency;
          this_sample -= stmlib::ThisBlepSample(t);
          next_sample -= stmlib::NextBlepSample(t);
        } else if (through_zero_fm && phase_ < 0.0f) {
          float t = phase_ / frequency;
          phase_ += 1.0f;
          this_sample += stmlib::ThisBlepSample(t);
          next_sample += stmlib::NextBlepSample(t);
        }
        next_sample += phase_;

        if (shape == OSCILLATOR_SHAPE_SAW) {
          *out++ = 2.0f * this_sample - 1.0f;
        } else {
          lp_state_ += 0.25f * ((hp_state_ - this_sample) - lp_state_);
          *out++ = 4.0f * lp_state_;
          hp_state_ = this_sample;
        }
      } else if (shape <= OSCILLATOR_SHAPE_SLOPE) {
        float slope_up = 2.0f;
        float slope_down = 2.0f;
        if (shape == OSCILLATOR_SHAPE_SLOPE) {
          slope_up = 1.0f / (pw);
          slope_down = 1.0f / (1.0f - pw);
        }
        if (high_ ^ (phase_ < pw)) {
          float t = (phase_ - pw) / frequency;
          float discontinuity = (slope_up + slope_down) * frequency;
          if (through_zero_fm && frequency < 0.0f) {
            discontinuity = -discontinuity;
          }
          this_sample -= stmlib::ThisIntegratedBlepSample(t) * discontinuity;
          next_sample -= stmlib::NextIntegratedBlepSample(t) * discontinuity;
          high_ = phase_ < pw;
        }
        if (phase_ >= 1.0f) {
          phase_ -= 1.0f;
          float t = phase_ / frequency;
          float discontinuity = (slope_up + slope_down) * frequency;
          this_sample += stmlib::ThisIntegratedBlepSample(t) * discontinuity;
          next_sample += stmlib::NextIntegratedBlepSample(t) * discontinuity;
          high_ = true;
        } else if (through_zero_fm && phase_ < 0.0f) {
          float t = phase_ / frequency;
          phase_ += 1.0f;
          float discontinuity = (slope_up + slope_down) * frequency;
          this_sample -= stmlib::ThisIntegratedBlepSample(t) * discontinuity;
          next_sample -= stmlib::NextIntegratedBlepSample(t) * discontinuity;
          high_ = false;
        }
        next_sample += high_
          ? phase_ * slope_up
          : 1.0f - (phase_ - pw) * slope_down;
        *out++ = 2.0f * this_sample - 1.0f;
      } else {
        if (high_ ^ (phase_ >= pw)) {
          float t = (phase_ - pw) / frequency;
          float discontinuity = 1.0f;
          if (through_zero_fm && frequency < 0.0f) {
            discontinuity = -discontinuity;
          }
          this_sample += stmlib::ThisBlepSample(t) * discontinuity;
          next_sample += stmlib::NextBlepSample(t) * discontinuity;
          high_ = phase_ >= pw;
        }
        if (phase_ >= 1.0f) {
          phase_ -= 1.0f;
          float t = phase_ / frequency;
          this_sample -= stmlib::ThisBlepSample(t);
          next_sample -= stmlib::NextBlepSample(t);
          high_ = false;
        } else if (through_zero_fm && phase_ < 0.0f) {
          float t = phase_ / frequency;
          phase_ += 1.0f;
          this_sample += stmlib::ThisBlepSample(t);
          next_sample += stmlib::NextBlepSample(t);
          high_ = true;
        }
        next_sample += phase_ < pw ? 0.0f : 1.0f;

        if (shape == OSCILLATOR_SHAPE_SQUARE_TRIANGLE) {
          const float integrator_coefficient = frequency * 0.0625f;
          this_sample = 128.0f * (this_sample - 0.5f);
          lp_state_ += integrator_coefficient * (this_sample - lp_state_);
          *out++ = lp_state_;
        } else if (shape == OSCILLATOR_SHAPE_SQUARE_DARK) {
          const float integrator_coefficient = frequency * 2.0f;
          this_sample = 4.0f * (this_sample - 0.5f);
          lp_state_ += integrator_coefficient * (this_sample - lp_state_);
          *out++ = lp_state_;
        } else if (shape == OSCILLATOR_SHAPE_SQUARE_BRIGHT) {
          const float integrator_coefficient = frequency * 2.0f;
          this_sample = 2.0f * this_sample - 1.0f;
          lp_state_ += integrator_coefficient * (this_sample - lp_state_);
          *out++ = (this_sample - lp_state_) * 0.5f;
        } else {
          this_sample = 2.0f * this_sample - 1.0f;
          *out++ = this_sample;
        }
      }
    }
    next_sample_ = next_sample;
  }

 private:
  // Oscillator state.
  float phase_;
  float next_sample_;
  float lp_state_;
  float hp_state_;
  bool high_;

  // For interpolation of parameters.
  float frequency_;
  float pw_;

  DISALLOW_COPY_AND_ASSIGN(Oscillator);
};

// ---------------------------------------------------------------------------
// From hi_hat.h
// 808 HH, with a few extra parameters to push things to the CY territory...
// The template parameter MetallicNoiseSource allows another kind of
// "metallic noise" to be used, for results which are more similar to KR-55
// or FM hi-hats.
// ---------------------------------------------------------------------------

// 808 style "metallic noise" with 6 square oscillators.
class SquareNoise {
 public:
  SquareNoise() { }
  ~SquareNoise() { }

  void Init() {
    std::fill(&phase_[0], &phase_[6], 0);
  }

  void Render(float f0, float* temp_1, float* temp_2, float* out, size_t size) {
    const float ratios[6] = {
        // Nominal f0: 414 Hz
        1.0f, 1.304f, 1.466f, 1.787f, 1.932f, 2.536f
    };

    uint32_t increment[6];
    uint32_t phase[6];
    for (int i = 0; i < 6; ++i) {
      float f = f0 * ratios[i];
      if (f >= 0.499f) f = 0.499f;
      increment[i] = static_cast<uint32_t>(f * 4294967296.0f);
      phase[i] = phase_[i];
    }

    while (size--) {
      phase[0] += increment[0];
      phase[1] += increment[1];
      phase[2] += increment[2];
      phase[3] += increment[3];
      phase[4] += increment[4];
      phase[5] += increment[5];
      uint32_t noise = 0;
      noise += (phase[0] >> 31);
      noise += (phase[1] >> 31);
      noise += (phase[2] >> 31);
      noise += (phase[3] >> 31);
      noise += (phase[4] >> 31);
      noise += (phase[5] >> 31);
      *out++ = 0.33f * static_cast<float>(noise) - 1.0f;
    }

    for (int i = 0; i < 6; ++i) {
      phase_[i] = phase[i];
    }
  }

 private:
  uint32_t phase_[6];

  DISALLOW_COPY_AND_ASSIGN(SquareNoise);
};

class RingModNoise {
 public:
  RingModNoise() { }
  ~RingModNoise() { }

  void Init() {
    for (int i = 0; i < 6; ++i) {
      oscillator_[i].Init();
    }
  }

  void Render(float f0, float* temp_1, float* temp_2, float* out, size_t size) {
    const float ratio = f0 / (0.01f + f0);
    const float f1a = 200.0f / kSampleRate * ratio;
    const float f1b = 7530.0f / kSampleRate * ratio;
    const float f2a = 510.0f / kSampleRate * ratio;
    const float f2b = 8075.0f / kSampleRate * ratio;
    const float f3a = 730.0f / kSampleRate * ratio;
    const float f3b = 10500.0f / kSampleRate * ratio;
    const float f[3][2] = { { f1a, f1b }, { f2a, f2b }, { f3a, f3b } };

    std::fill(&out[0], &out[size], 0.0f);

    for (int i = 0; i < 3; ++i) {
      RenderPair(&oscillator_[2 * i], f[i], temp_1, temp_2, out, size);
    }
  }

 private:
  void RenderPair(
      Oscillator* osc,
      const float* f,
      float* temp_1,
      float* temp_2,
      float* out,
      size_t size) {
    osc[0].Render<OSCILLATOR_SHAPE_SQUARE>(f[0], 0.5f, temp_1, size);
    osc[1].Render<OSCILLATOR_SHAPE_SAW>(f[1], 0.5f, temp_2, size);
    while (size--) {
      *out++ += *temp_1++ * *temp_2++;
    }
  }
  Oscillator oscillator_[6];

  DISALLOW_COPY_AND_ASSIGN(RingModNoise);
};

class SwingVCA {
 public:
  float operator()(float s, float gain) {
   s *= s > 0.0f ? 4.0f : 0.1f;
   s = s / (1.0f + fabsf(s));
   return (s + 0.1f) * gain;
  }
};

class LinearVCA {
 public:
  float operator()(float s, float gain) {
   return s * gain;
  }
};

template<
    typename MetallicNoiseSource,
    typename VCA,
    bool resonance,
    bool two_stage_envelope>
class HiHat {
 public:
  HiHat() { }
  ~HiHat() { }

  void Init() {
    envelope_ = 0.0f;
    noise_clock_ = 0.0f;
    noise_sample_ = 0.0f;
    sustain_gain_ = 0.0f;

    metallic_noise_.Init();
    noise_coloration_svf_.Init();
    hpf_.Init();
  }

  void Render(
      bool sustain,
      bool trigger,
      float accent,
      float f0,
      float tone,
      float decay,
      float noisiness,
      float* temp_1,
      float* temp_2,
      float* out,
      size_t size) {
    const float envelope_decay = 1.0f - 0.003f * stmlib::SemitonesToRatio(
        -decay * 84.0f);
    const float cut_decay = 1.0f - 0.0025f * stmlib::SemitonesToRatio(
        -decay * 36.0f);

    if (trigger) {
      envelope_ = (1.5f + 0.5f * (1.0f - decay)) * (0.3f + 0.7f * accent);
    }

    // Render the metallic noise.
    metallic_noise_.Render(2.0f * f0, temp_1, temp_2, out, size);

    // Apply BPF on the metallic noise.
    float cutoff = 150.0f / kSampleRate * stmlib::SemitonesToRatio(
        tone * 72.0f);
    CONSTRAIN(cutoff, 0.0f, 16000.0f / kSampleRate);
    noise_coloration_svf_.set_f_q<stmlib::FREQUENCY_ACCURATE>(
        cutoff, resonance ? 3.0f + 3.0f * tone : 1.0f);
    noise_coloration_svf_.Process<stmlib::FILTER_MODE_BAND_PASS>(
        out, out, size);

    // This is not at all part of the 808 circuit! But to add more variety, we
    // add a variable amount of clocked noise to the output of the 6 schmitt
    // trigger oscillators.
    noisiness *= noisiness;
    float noise_f = f0 * (16.0f + 16.0f * (1.0f - noisiness));
    CONSTRAIN(noise_f, 0.0f, 0.5f);

    for (size_t i = 0; i < size; ++i) {
      noise_clock_ += noise_f;
      if (noise_clock_ >= 1.0f) {
        noise_clock_ -= 1.0f;
        noise_sample_ = stmlib::Random::GetFloat() - 0.5f;
      }
      out[i] += noisiness * (noise_sample_ - out[i]);
    }

    // Apply VCA.
    stmlib::ParameterInterpolator sustain_gain(
        &sustain_gain_,
        accent * decay,
        size);
    for (size_t i = 0; i < size; ++i) {
      VCA vca;
      envelope_ *= envelope_ > 0.5f || !two_stage_envelope
          ? envelope_decay
          : cut_decay;
      out[i] = vca(out[i], sustain ? sustain_gain.Next() : envelope_);
    }

    hpf_.set_f_q<stmlib::FREQUENCY_ACCURATE>(cutoff, 0.5f);
    hpf_.Process<stmlib::FILTER_MODE_HIGH_PASS>(out, out, size);
  }

 private:
  float envelope_;
  float noise_clock_;
  float noise_sample_;
  float sustain_gain_;

  MetallicNoiseSource metallic_noise_;
  stmlib::Svf noise_coloration_svf_;
  stmlib::Svf hpf_;

  DISALLOW_COPY_AND_ASSIGN(HiHat);
};

// ---------------------------------------------------------------------------
// From hi_hat_engine.h / hi_hat_engine.cc
// 808-style HH with two noise sources - one faithful to the original, the
// other more metallic.
// ---------------------------------------------------------------------------

class HiHatEngine final : public Engine {
 public:
  HiHatEngine() { }
  ~HiHatEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    hi_hat_1_.Init();
    hi_hat_2_.Init();
    temp_buffer_ = allocator->Allocate<float>(kMaxBlockSize * 2);
  }

  virtual void Reset() {

  }

  virtual void LoadUserData(const uint8_t* user_data) { }

  virtual void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size,
      bool* already_enveloped) {
    const float f0 = NoteToFrequency(parameters.note);

    hi_hat_1_.Render(
        parameters.trigger & TRIGGER_UNPATCHED,
        parameters.trigger & TRIGGER_RISING_EDGE,
        parameters.accent,
        f0,
        parameters.timbre,
        parameters.morph,
        parameters.harmonics,
        temp_buffer_,
        temp_buffer_ + size,
        out,
        size);

    hi_hat_2_.Render(
        parameters.trigger & TRIGGER_UNPATCHED,
        parameters.trigger & TRIGGER_RISING_EDGE,
        parameters.accent,
        f0,
        parameters.timbre,
        parameters.morph,
        parameters.harmonics,
        temp_buffer_,
        temp_buffer_ + size,
        aux,
        size);
  }

 private:
  HiHat<SquareNoise, SwingVCA, true, false> hi_hat_1_;
  HiHat<RingModNoise, LinearVCA, false, true> hi_hat_2_;

  float* temp_buffer_;

  DISALLOW_COPY_AND_ASSIGN(HiHatEngine);
};

}  // namespace plaits

#endif  // PLAITS_DSP_ENGINE_HI_HAT_ENGINE_MERGED_H_
