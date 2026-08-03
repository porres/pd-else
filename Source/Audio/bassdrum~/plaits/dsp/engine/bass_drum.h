// Copyright 2016 Emilie Gillet.
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// 808 and synthetic bass drum generators, and the engine that combines them.
//
// This file merges (in order):
//   - analog_bass_drum.h    (AnalogBassDrum)
//   - synthetic_bass_drum.h (SyntheticBassDrumClick, SyntheticBassDrumAttackNoise,
//                             SyntheticBassDrum)
//   - bass_drum_engine.h    (BassDrumEngine declaration)
//   - bass_drum_engine.cc   (BassDrumEngine implementation)

#ifndef PLAITS_DSP_ENGINE_BASS_DRUM_ENGINE_MERGED_H_
#define PLAITS_DSP_ENGINE_BASS_DRUM_ENGINE_MERGED_H_

#include "stmlib/dsp/dsp.h"
#include "stmlib/dsp/filter.h"
#include "stmlib/dsp/parameter_interpolator.h"
#include "stmlib/dsp/units.h"
#include "stmlib/utils/random.h"

#include "plaits/dsp/dsp.h"
#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/fx/overdrive.h"
#include "plaits/dsp/oscillator/sine_oscillator.h"

namespace plaits {

// ---------------------------------------------------------------------------
// From analog_bass_drum.h
// 808 bass drum model, revisited.
// ---------------------------------------------------------------------------

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
      // Q39 / Q40
      float pulse = 0.0f;
      if (pulse_remaining_samples_) {
        --pulse_remaining_samples_;
        pulse = pulse_remaining_samples_ ? pulse_height_ : pulse_height_ - 1.0f;
        pulse_ = pulse;
      } else {
        pulse_ *= 1.0f - 1.0f / kPulseDecayTime;
        pulse = pulse_;
      }

      // C40 / R163 / R162 / D83
      ONE_POLE(pulse_lp_, pulse, 1.0f / kPulseFilterTime);
      pulse = Diode((pulse - pulse_lp_) + pulse * 0.044f);

      // Q41 / Q42
      float fm_pulse = 0.0f;
      if (fm_pulse_remaining_samples_) {
        --fm_pulse_remaining_samples_;
        fm_pulse = 1.0f;
        // C39 / C52
        retrig_pulse_ = fm_pulse_remaining_samples_ ? 0.0f : -0.8f;
      } else {
        // C39 / R161
        retrig_pulse_ *= 1.0f - 1.0f / kRetrigPulseDuration;
      }

      ONE_POLE(fm_pulse_lp_, fm_pulse, 1.0f / kPulseFilterTime);

      // Q43 and R170 leakage
      float punch = 0.7f + Diode(10.0f * lp_out_ - 1.0f);

      // Q43 / R165
      float attack_fm = fm_pulse_lp_ * 1.7f * attack_fm_amount;
      float self_fm = punch * 0.08f * self_fm_amount;
      float f = f0 * (1.0f + attack_fm + self_fm);
      CONSTRAIN(f, 0.0f, 0.4f);

      float resonator_out;

      resonator_.set_f_q<stmlib::FREQUENCY_DIRTY>(f, 1.0f + q * f);
      resonator_.Process<stmlib::FILTER_MODE_BAND_PASS,
          stmlib::FILTER_MODE_LOW_PASS>(
              (pulse - retrig_pulse_ * 0.2f) * scale,
              &resonator_out,
              &lp_out_);

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

  DISALLOW_COPY_AND_ASSIGN(AnalogBassDrum);
};

// ---------------------------------------------------------------------------
// From synthetic_bass_drum.h
// Naive bass drum model (modulated oscillator with FM + envelope).
// Inadvertently 909-ish.
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// From bass_drum_engine.h / bass_drum_engine.cc
// 808 and synthetic bass drum generators, combined into a single engine.
// ---------------------------------------------------------------------------

class BassDrumEngine final : public Engine {
 public:
  BassDrumEngine() { }
  ~BassDrumEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    analog_bass_drum_.Init();
    synthetic_bass_drum_.Init();
    overdrive_.Init();
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

    const float attack_fm_amount = std::min(parameters.harmonics * 4.0f, 1.0f);
    const float self_fm_amount = std::max(
        std::min(parameters.harmonics * 4.0f - 1.0f, 1.0f), 0.0f);
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

 private:
  AnalogBassDrum analog_bass_drum_;
  SyntheticBassDrum synthetic_bass_drum_;

  Overdrive overdrive_;

  DISALLOW_COPY_AND_ASSIGN(BassDrumEngine);
};

}  // namespace plaits

#endif  // PLAITS_DSP_ENGINE_BASS_DRUM_ENGINE_MERGED_H_
