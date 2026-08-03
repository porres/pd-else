// Copyright 2016 Emilie Gillet.
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// 808 and synthetic snare drum generators, and the engine that combines them.
//
// This file merges (in order):
//   - analog_snare_drum.h    (AnalogSnareDrum)
//   - synthetic_snare_drum.h (SyntheticSnareDrum)
//   - snare_drum_engine.h    (SnareDrumEngine declaration)
//   - snare_drum_engine.cc   (SnareDrumEngine implementation)

#ifndef PLAITS_DSP_ENGINE_SNARE_DRUM_ENGINE_MERGED_H_
#define PLAITS_DSP_ENGINE_SNARE_DRUM_ENGINE_MERGED_H_

#include <algorithm>

#include "plaits/stmlib.h"
#include "plaits/engine/engine.h"
#include "plaits/sine_oscillator.h"

namespace plaits {

// ---------------------------------------------------------------------------
// From analog_snare_drum.h
// 808 snare drum model, revisited.
// ---------------------------------------------------------------------------

class AnalogSnareDrum {
 public:
  AnalogSnareDrum() { }
  ~AnalogSnareDrum() { }

  static const int kNumModes = 5;

  void Init() {
    pulse_remaining_samples_ = 0;
    pulse_ = 0.0f;
    pulse_height_ = 0.0f;
    pulse_lp_ = 0.0f;
    noise_envelope_ = 0.0f;
    sustain_gain_ = 0.0f;

    for (int i = 0; i < kNumModes; ++i) {
      resonator_[i].Init();
      oscillator_[i].Init();
    }
    noise_filter_.Init();
  }

  void Render(
      bool sustain,
      bool trigger,
      float accent,
      float f0,
      float tone,
      float decay,
      float snappy,
      float* out,
      size_t size) {
    const float decay_xt = decay * (1.0f + decay * (decay - 1.0f));
    const int kTriggerPulseDuration = 1.0e-3f * kSampleRate;
    const float kPulseDecayTime = 0.1e-3f * kSampleRate;
    const float q = 2000.0f * stmlib::SemitonesToRatio(decay_xt * 84.0f);
    const float noise_envelope_decay = 1.0f - 0.0017f * \
        stmlib::SemitonesToRatio(-decay * (50.0f + snappy * 10.0f));
    const float exciter_leak = snappy * (2.0f - snappy) * 0.1f;

    snappy = snappy * 1.1f - 0.05f;
    CONSTRAIN(snappy, 0.0f, 1.0f);

    if (trigger) {
      pulse_remaining_samples_ = kTriggerPulseDuration;
      pulse_height_ = 3.0f + 7.0f * accent;
      noise_envelope_ = 2.0f;
    }

    static const float kModeFrequencies[kNumModes] = {
        1.00f,
        2.00f,
        3.18f,
        4.16f,
        5.62f};

    float f[kNumModes];
    float gain[kNumModes];

    for (int i = 0; i < kNumModes; ++i) {
      f[i] = std::min(f0 * kModeFrequencies[i], 0.499f);
      resonator_[i].set_f_q<stmlib::FREQUENCY_FAST>(
          f[i],
          1.0f + f[i] * (i == 0 ? q : q * 0.25f));
    }

    if (tone < 0.666667f) {
      // 808-style (2 modes)
      tone *= 1.5f;
      gain[0] = 1.5f + (1.0f - tone) * (1.0f - tone) * 4.5f;
      gain[1] = 2.0f * tone + 0.15f;
      std::fill(&gain[2], &gain[kNumModes], 0.0f);
    } else {
      // What the 808 could have been if there were extra modes!
      tone = (tone - 0.666667f) * 3.0f;
      gain[0] = 1.5f - tone * 0.5f;
      gain[1] = 2.15f - tone * 0.7f;
      for (int i = 2; i < kNumModes; ++i) {
        gain[i] = tone;
        tone *= tone;
      }
    }

    float f_noise = f0 * 16.0f;
    CONSTRAIN(f_noise, 0.0f, 0.499f);
    noise_filter_.set_f_q<stmlib::FREQUENCY_FAST>(
        f_noise, 1.0f + f_noise * 1.5f);


    stmlib::ParameterInterpolator sustain_gain(
        &sustain_gain_,
        accent * decay,
        size);

    while (size--) {
      // Q45 / Q46
      float pulse = 0.0f;
      if (pulse_remaining_samples_) {
        --pulse_remaining_samples_;
        pulse = pulse_remaining_samples_ ? pulse_height_ : pulse_height_ - 1.0f;
        pulse_ = pulse;
      } else {
        pulse_ *= 1.0f - 1.0f / kPulseDecayTime;
        pulse = pulse_;
      }

      float sustain_gain_value = sustain_gain.Next();

      // R189 / C57 / R190 + C58 / C59 / R197 / R196 / IC14
      ONE_POLE(pulse_lp_, pulse, 0.75f);

      float shell = 0.0f;
      for (int i = 0; i < kNumModes; ++i) {
        float excitation = i == 0
            ? (pulse - pulse_lp_) + 0.006f * pulse
            : 0.026f * pulse;
        shell += gain[i] * (sustain
            ? oscillator_[i].Next(f[i]) * sustain_gain_value * 0.25f
            : resonator_[i].Process<stmlib::FILTER_MODE_BAND_PASS>(
                  excitation) + excitation * exciter_leak);
      }
      shell = stmlib::SoftClip(shell);

      // C56 / R194 / Q48 / C54 / R188 / D54
      float noise = 2.0f * stmlib::Random::GetFloat() - 1.0f;
      if (noise < 0.0f) noise = 0.0f;
      noise_envelope_ *= noise_envelope_decay;
      noise *= (sustain ? sustain_gain_value : noise_envelope_) * snappy * 2.0f;

      // C66 / R201 / C67 / R202 / R203 / Q49
      noise = noise_filter_.Process<stmlib::FILTER_MODE_BAND_PASS>(noise);

      // IC13
      *out++ = noise + shell * (1.0f - snappy);
    }
  }

 private:
  int pulse_remaining_samples_;
  float pulse_;
  float pulse_height_;
  float pulse_lp_;
  float noise_envelope_;
  float sustain_gain_;

  stmlib::Svf resonator_[kNumModes];
  stmlib::Svf noise_filter_;

  // Replace the resonators in "free running" (sustain) mode.
  SineOscillator oscillator_[kNumModes];

  DISALLOW_COPY_AND_ASSIGN(AnalogSnareDrum);
};

// ---------------------------------------------------------------------------
// From synthetic_snare_drum.h
// Naive snare drum model (two modulated oscillators + filtered noise).
// ---------------------------------------------------------------------------

class SyntheticSnareDrum {
 public:
  SyntheticSnareDrum() { }
  ~SyntheticSnareDrum() { }

  void Init() {
    phase_[0] = 0.0f;
    phase_[1] = 0.0f;
    drum_amplitude_ = 0.0f;
    snare_amplitude_ = 0.0f;
    fm_ = 0.0f;
    hold_counter_ = 0;
    sustain_gain_ = 0.0f;

    drum_lp_.Init();
    snare_hp_.Init();
    snare_lp_.Init();
  }

  inline float DistortedSine(float phase) {
    float triangle = (phase < 0.5f ? phase : 1.0f - phase) * 4.0f - 1.3f;
    return 2.0f * triangle / (1.0f + fabsf(triangle));
  }

  void Render(
      bool sustain,
      bool trigger,
      float accent,
      float f0,
      float fm_amount,
      float decay,
      float snappy,
      float* out,
      size_t size) {
    const float decay_xt = decay * (1.0f + decay * (decay - 1.0f));
    fm_amount *= fm_amount;
    const float drum_decay = 1.0f - 1.0f / (0.015f * kSampleRate) * \
        stmlib::SemitonesToRatio(
           -decay_xt * 72.0f - fm_amount * 12.0f + snappy * 7.0f);
    const float snare_decay = 1.0f - 1.0f / (0.01f * kSampleRate) * \
        stmlib::SemitonesToRatio(-decay * 60.0f - snappy * 7.0f);
    const float fm_decay = 1.0f - 1.0f / (0.007f * kSampleRate);

    snappy = snappy * 1.1f - 0.05f;
    CONSTRAIN(snappy, 0.0f, 1.0f);

    const float drum_level = stmlib::Sqrt(1.0f - snappy);
    const float snare_level = stmlib::Sqrt(snappy);

    const float snare_f_min = std::min(10.0f * f0, 0.5f);
    const float snare_f_max = std::min(35.0f * f0, 0.5f);

    snare_hp_.set_f<stmlib::FREQUENCY_FAST>(snare_f_min);
    snare_lp_.set_f_q<stmlib::FREQUENCY_FAST>(snare_f_max,
        0.5f + 2.0f * snappy);
    drum_lp_.set_f<stmlib::FREQUENCY_FAST>(3.0f * f0);

    if (trigger) {
      snare_amplitude_ = drum_amplitude_ = 0.3f + 0.7f * accent;
      fm_ = 1.0f;
      phase_[0] = phase_[1] = 0.0f;
      hold_counter_ = static_cast<int>((0.04f + decay * 0.03f) * kSampleRate);
    }

    stmlib::ParameterInterpolator sustain_gain(
        &sustain_gain_,
        accent * decay,
        size);
    while (size--) {
      if (sustain) {
        snare_amplitude_ = sustain_gain.Next();
        drum_amplitude_ = snare_amplitude_;
        fm_ = 0.0f;
      } else {
        // Compute all D envelopes.
        // The envelope for the drum has a very long tail.
        // The envelope for the snare has a "hold" stage which lasts between
        // 40 and 70 ms
        drum_amplitude_ *= (drum_amplitude_ > 0.03f || !(size & 1))
            ? drum_decay
            : 1.0f;
        if (hold_counter_) {
          --hold_counter_;
        } else {
          snare_amplitude_ *= snare_decay;
        }
        fm_ *= fm_decay;
      }

      // The 909 circuit has a funny kind of oscillator coupling - the signal
      // leaving Q40's collector and resetting all oscillators allow some
      // intermodulation.
      float reset_noise = 0.0f;
      float reset_noise_amount = (0.125f - f0) * 8.0f;
      CONSTRAIN(reset_noise_amount, 0.0f, 1.0f);
      reset_noise_amount *= reset_noise_amount;
      reset_noise_amount *= fm_amount;
      reset_noise += phase_[0] > 0.5f ? -1.0f : 1.0f;
      reset_noise += phase_[1] > 0.5f ? -1.0f : 1.0f;
      reset_noise *= reset_noise_amount * 0.025f;

      float f = f0 * (1.0f + fm_amount * (4.0f * fm_));
      phase_[0] += f;
      phase_[1] += f * 1.47f;
      if (reset_noise_amount > 0.1f) {
        if (phase_[0] >= 1.0f + reset_noise) {
          phase_[0] = 1.0f - phase_[0];
        }
        if (phase_[1] >= 1.0f + reset_noise) {
          phase_[1] = 1.0f - phase_[1];
        }
      } else {
        if (phase_[0] >= 1.0f) {
          phase_[0] -= 1.0f;
        }
        if (phase_[1] >= 1.0f) {
          phase_[1] -= 1.0f;
        }
      }

      float drum = -0.1f;
      drum += DistortedSine(phase_[0]) * 0.60f;
      drum += DistortedSine(phase_[1]) * 0.25f;
      drum *= drum_amplitude_ * drum_level;
      drum = drum_lp_.Process<stmlib::FILTER_MODE_LOW_PASS>(drum);

      float noise = stmlib::Random::GetFloat();
      float snare = snare_lp_.Process<stmlib::FILTER_MODE_LOW_PASS>(noise);
      snare = snare_hp_.Process<stmlib::FILTER_MODE_HIGH_PASS>(snare);
      snare = (snare + 0.1f) * (snare_amplitude_ + fm_) * snare_level;

      *out++ = snare + drum;  // It's a snare, it's a drum, it's a snare drum.
    }
  }

 private:
  float phase_[2];
  float drum_amplitude_;
  float snare_amplitude_;
  float fm_;
  float sustain_gain_;
  int hold_counter_;

  stmlib::OnePole drum_lp_;
  stmlib::OnePole snare_hp_;
  stmlib::Svf snare_lp_;

  DISALLOW_COPY_AND_ASSIGN(SyntheticSnareDrum);
};

// ---------------------------------------------------------------------------
// From snare_drum_engine.h / snare_drum_engine.cc
// 808 and synthetic snare drum generators, combined into a single engine.
// ---------------------------------------------------------------------------

class SnareDrumEngine final : public Engine {
 public:
  SnareDrumEngine() { }
  ~SnareDrumEngine() { }

  virtual void Init(stmlib::BufferAllocator* allocator) {
    analog_snare_drum_.Init();
    synthetic_snare_drum_.Init();
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

    analog_snare_drum_.Render(
        parameters.trigger & TRIGGER_UNPATCHED,
        parameters.trigger & TRIGGER_RISING_EDGE,
        parameters.accent,
        f0,
        parameters.timbre,
        parameters.morph,
        parameters.harmonics,
        out,
        size);

    synthetic_snare_drum_.Render(
        parameters.trigger & TRIGGER_UNPATCHED,
        parameters.trigger & TRIGGER_RISING_EDGE,
        parameters.accent,
        f0,
        parameters.timbre,
        parameters.morph,
        parameters.harmonics,
        aux,
        size);
  }

 private:
  AnalogSnareDrum analog_snare_drum_;
  SyntheticSnareDrum synthetic_snare_drum_;

  DISALLOW_COPY_AND_ASSIGN(SnareDrumEngine);
};

}  // namespace plaits

#endif  // PLAITS_DSP_ENGINE_SNARE_DRUM_ENGINE_MERGED_H_
