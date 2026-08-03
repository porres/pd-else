// Copyright 2016 Emilie Gillet.
// See http://creativecommons.org/licenses/MIT/ for more information.
//
// 808 and synthetic bass drum generators and the engine that combines them.
// Depends on stmlib.h for shared DSP primitives, the sine table, and Overdrive.

#ifndef PLAITS_BASS_DRUM_ENGINE_
#define PLAITS_BASS_DRUM_ENGINE_

#include "stmlib.h"

namespace plaits {

inline float kSampleRate = 48000.0f;

const size_t kMaxBlockSize = 24;
const size_t kBlockSize = 12;

inline float SemitonesToRatio(float semitones) {
  return pow(2, semitones / 12.f);
}

enum TriggerState {
  TRIGGER_LOW = 0,
  TRIGGER_RISING_EDGE = 1,
};

struct EngineParameters {
  int trigger;
  float note;
  float timbre;
  float morph;
  float harmonics;
  float accent;
};

// Fixed output gain applied to both the 808 (out) and 909-ish (aux) outputs.
const float kOutputGain = 0.8f;

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
    const float q = 1500.0f * SemitonesToRatio(decay * 80.0f);
    const float tone_f = std::min(4.0f * f0 * SemitonesToRatio(tone * 108.0f), 1.0f);
    const float exciter_leak = 0.08f * (tone + 0.25f);

    if (trigger) {
      pulse_remaining_samples_ = kTriggerPulseDuration;
      fm_pulse_remaining_samples_ = kFMPulseDuration;
      pulse_height_ = 3.0f + 7.0f * accent;
      lp_out_ = 0.0f;
    }

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

      ONE_POLE(fm_pulse_lp_, fm_pulse, 1.0f / kPulseFilterTime);

      float punch = 0.7f + Diode(10.0f * lp_out_ - 1.0f);

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
    transient_env_ = 0.0f;
    transient_env_lp_ = 0.0f;
    body_env_pulse_width_ = 0;
    fm_pulse_width_ = 0;
    tone_lp_ = 0.0f;

    click_.Init();
    noise_.Init();
  }

  inline float DistortedSine(float phase, float phase_noise, float dirtiness) {
    phase += phase_noise * dirtiness;
    MAKE_INTEGRAL_FRACTIONAL(phase);
    phase = phase_fractional;
    float triangle = (phase < 0.5f ? phase : 1.0f - phase) * 4.0f - 1.0f;
    float sine = 2.0f * triangle / (1.0f + fabsf(triangle));
    float clean_sine = stmlib::Sine(phase + 0.75f);
    return sine + (1.0f - dirtiness) * (clean_sine - sine);
  }

  inline float TransistorVCA(float s, float gain) {
    s = (s - 0.6f) * gain;
    return 3.0f * s / (2.0f + fabsf(s)) + gain * 0.3f;
  }

  void Render(
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
        SemitonesToRatio(-decay * 60.0f);
    const float transient_env_decay = 1.0f - 1.0f / (0.005f * kSampleRate);
    const float tone_f = std::min(4.0f * f0 * SemitonesToRatio(tone * 108.0f), 1.0f);
    const float transient_level = tone;

    if (trigger) {
      fm_ = 1.0f;
      body_env_ = transient_env_ = 0.3f + 0.7f * accent;
      body_env_pulse_width_ = kSampleRate * 0.001f;
      fm_pulse_width_ = kSampleRate * 0.0013f;
    }

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

class BassDrumEngine {
 public:
  BassDrumEngine() { }
  ~BassDrumEngine() { }

  void Init() {
    analog_bass_drum_.Init();
    synthetic_bass_drum_.Init();
    overdrive_.Init();
  }

  void Render(const EngineParameters& parameters,
      float* out,
      float* aux,
      size_t size) {
    const float f0 = parameters.note / kSampleRate;

    const float attack_fm_amount = std::min(parameters.harmonics * 4.0f, 1.0f);
    const float self_fm_amount = std::max(
        std::min(parameters.harmonics * 4.0f - 1.0f, 1.0f), 0.0f);
    const float drive = std::max(parameters.harmonics * 2.0f - 1.0f, 0.0f) * \
        std::max(1.0f - 16.0f * f0, 0.0f);

    analog_bass_drum_.Render(
        parameters.trigger & TRIGGER_RISING_EDGE,
        parameters.accent,
        f0,
        parameters.timbre,
        parameters.morph,
        attack_fm_amount,
        self_fm_amount,
        out,
        size);

    overdrive_.Process(0.5f + 0.5f * drive, out, size);

    synthetic_bass_drum_.Render(
        parameters.trigger & TRIGGER_RISING_EDGE,
        parameters.accent,
        f0,
        parameters.timbre,
        parameters.morph,
        0.4f - 0.25f * parameters.morph * parameters.morph,
        std::min(parameters.harmonics * 2.0f, 1.0f),
        std::max(parameters.harmonics * 2.0f - 1.0f, 0.0f),
        aux,
        size);
  }

 private:
  AnalogBassDrum analog_bass_drum_;
  SyntheticBassDrum synthetic_bass_drum_;

  stmlib::Overdrive overdrive_;

  DISALLOW_COPY_AND_ASSIGN(BassDrumEngine);
};

// ---------------------------------------------------------------------------
// From plaits/envelope.h
// Internal envelope.
// ---------------------------------------------------------------------------

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

struct Patch {
  float note;
  float harmonics;
  float timbre;
  float morph;
  float decay;
  float frequency_modulation_amount;
};

struct Modulations {
  float trigger;
  float level;
};

class Voice {
 public:
  Voice() {}
  ~Voice() {}

  struct Frame {
    float out;
    float aux;
  };

  void Init() {
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
      if (trigger_value > 0.0f) {
        trigger_state_ = true;
        decay_envelope_.Trigger();
      }
    } else {
      if (trigger_value <= 0.0f) {
        trigger_state_ = false;
      }
    }

    BassDrumEngine* e = &bass_drum_engine_;

    EngineParameters p;

    bool rising_edge = trigger_state_ && !previous_trigger_state;

    p.trigger = rising_edge ? TRIGGER_RISING_EDGE : TRIGGER_LOW;

    const float short_decay = (200.0f * kBlockSize) / kSampleRate *
        SemitonesToRatio(-96.0f * patch.decay);

    decay_envelope_.Process(short_decay * 2.0f);

    float compressed_level = 1.3f * modulations.level / (0.3f + fabsf(modulations.level));
    CONSTRAIN(compressed_level, 0.0f, 1.0f);
    p.accent = compressed_level;

    p.harmonics = patch.harmonics;

    p.note = ApplyModulations(patch.note,
        patch.frequency_modulation_amount,
        decay_envelope_.value() * decay_envelope_.value() * 4.0f);

    p.timbre = patch.timbre;
    p.morph = patch.morph;

    e->Render(p, out_buffer_, aux_buffer_, size);

    for (size_t i = 0; i < size; ++i) {
      frames->out = out_buffer_[i] * kOutputGain;
      frames->aux = aux_buffer_[i] * kOutputGain;
      ++frames;
    }
  }

 private:
  inline float ApplyModulations(float base_hz,
      float modulation_amount,
      float envelope) {
    modulation_amount *= std::max(fabsf(modulation_amount) - 0.05f, 0.05f);
    modulation_amount *= 1.05f;
    return base_hz * powf(2.0f, modulation_amount * envelope);
  }

  BassDrumEngine bass_drum_engine_;

  bool trigger_state_;

  DecayEnvelope decay_envelope_;

  float out_buffer_[kMaxBlockSize];
  float aux_buffer_[kMaxBlockSize];

  DISALLOW_COPY_AND_ASSIGN(Voice);
};

}  // namespace plaits

#endif  // PLAITS_BASS_DRUM_ENGINE_
