// Original copyright Emilie Gillet, MIT license.

#ifndef PLAITS_BASS_DRUM_H_
#define PLAITS_BASS_DRUM_H_

#include <stdint.h>
#include <stddef.h>
#include <math.h>

// stmlib utilities
#define CONSTRAIN(var, min, max) \
  if (var < (min)) { \
    var = (min); \
  } else if (var > (max)) { \
    var = (max); \
  }

#define ONE_POLE(out, in, coefficient) out += (coefficient) * ((in) - out);
#define SLOPE(out, in, positive, negative) { \
  float error = (in) - out; \
  out += (error > 0 ? positive : negative) * error; \
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

// Random
static uint32_t stmlib_rng_state = 0x21;

static inline float random_get_float(void) {
  stmlib_rng_state = stmlib_rng_state * 1664525L + 1013904223L;
  return (float)stmlib_rng_state / 4294967296.0f;
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
static inline float semitones_to_ratio(float semitones) {
  return powf(2.0f, semitones * (1.0f / 12.0f));
}

// filter (Svf)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define M_PI_F ((float)M_PI)
#define M_PI_POW_3 (M_PI_F * M_PI_F * M_PI_F)
#define M_PI_POW_5 (M_PI_POW_3 * M_PI_F * M_PI_F)

// TEMP: replaced LUT-based sine with libm sinf. Swap for ELSE LUT later.
static inline float sine(float phase) {
  return sinf(phase * 2.0f * M_PI_F);
}

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

static inline void svf_init(svf* s) {
  s->g = onepole_tan_dirty(0.01f);
  s->r = 1.0f / 100.0f;
  s->h = 1.0f / (1.0f + s->r * s->g + s->g * s->g);
  s->state_1 = 0.0f;
  s->state_2 = 0.0f;
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

// Global constants
extern float kSampleRate;
extern float a0;

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
  svf resonator;
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
  svf_init(&d->resonator);
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
    ONE_POLE(d->fm_pulse_lp, fm_pulse, 1.0f / pulse_filter_time);

    float punch = 0.7f + analog_bass_drum_diode(10.0f * d->lp_out - 1.0f);

    float attack_fm = d->fm_pulse_lp * 1.7f * attack_fm_amount;
    float self_fm = punch * 0.08f * self_fm_amount;
    float f = f0 * (1.0f + attack_fm + self_fm);
    CONSTRAIN(f, 0.0f, 0.4f);

    float resonator_out;
    svf_set_f_q_dirty(&d->resonator, f, 1.0f + q * f);
    svf_process_bp_lp(&d->resonator, (pulse - d->retrig_pulse * 0.2f) * scale, &resonator_out, &d->lp_out);

    ONE_POLE(d->tone_lp, pulse * exciter_leak + resonator_out, tone_f);

    *out++ = d->tone_lp;
  }
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
  synthetic_bass_drum_click_init(&d->click);
  synthetic_bass_drum_attack_noise_init(&d->noise);
}

static inline float synthetic_bass_drum_distorted_sine(float phase, float phase_noise, float dirtiness) {
  phase += phase_noise * dirtiness;
  phase -= (float)((int32_t)phase);
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

  while (size--) {
    ONE_POLE(d->phase_noise, random_get_float() - 0.5f, 0.002f);
    float mix = 0.0f;

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

    ONE_POLE(d->tone_lp, mix, tone_f);
    *out++ = d->tone_lp;
  }
  pi_finish(&f0_mod);
}

// =============================================================================
// BassDrumEngine
// =============================================================================

typedef struct {
  analog_bass_drum analog_bass_drum;
  synthetic_bass_drum synthetic_bass_drum;
  overdrive overdrive;
  float out_gain;
  float aux_gain;
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

    analog_bass_drum_render(
        &e->analog_bass_drum,
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
      float d_aux = 0.4f - 0.25f * parameters->morph * parameters->morph;
      float fma = parameters->harmonics * 2.0f;
      float fmd = parameters->harmonics * 2.0f - 1.0f;
      synthetic_bass_drum_render(
          &e->synthetic_bass_drum,
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
    int32_t v = 1 + (int32_t)(*in++ * post_gain);
    if (v < -32768) v = -32768;
    else if (v > 32767) v = 32767;
    *out = (short)v;
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
  v->bass_drum_engine.out_gain = 0.8f;
  v->bass_drum_engine.aux_gain = 0.8f;

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

  channel_post_processor_process(e->out_gain, v->out_buffer, &frames->out, size, 2);
  channel_post_processor_process(e->aux_gain, v->aux_buffer, &frames->aux, size, 2);
}

#endif  // PLAITS_BASS_DRUM_H_
