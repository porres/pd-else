// based on the plaits' bassdrum engine by Mutable Instruments (MIT)
// Original copyright Emilie Gillet, MIT license.

#include <m_pd.h>
#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <buffer.h>

// Globals
float k_SR = 48000.0f;
float a0 = 55.0f / 48000.0f;

// stmlib utilities
#define CONSTRAIN(var, min, max) \
  if(var < (min)){ \
    var = (min); \
  } else if(var > (max)){ \
    var = (max); \
  }

#define ONE_POLE(out, in, coefficient) out += (coefficient) * ((in) - out);
#define SLOPE(out, in, positive, negative){ \
  float error = (in) - out; \
  out += (error > 0 ? positive : negative) * error; \
}

static inline float soft_clip(float x){
  if(x < -3.0f)
    return -1.0f;
  else if(x > 3.0f)
    return 1.0f;
  else
    return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

// Random
static uint32_t stmlib_rng_state = 0x21;

static inline float random_get_float(void){
  stmlib_rng_state = stmlib_rng_state * 1664525L + 1013904223L;
  return (float)stmlib_rng_state / 4294967296.0f;
}

// ParameterInterpolator
typedef struct{
    float    *state;
    float     value;
    float     increment;
}param_interp;

static inline void pi_init(param_interp* p, float* state, float new_value, size_t size){
    p->state = state;
    p->value = *state;
    p->increment = (new_value - *state) / (float)size;
}

static inline float pi_next(param_interp* p){
    p->value += p->increment;
    return p->value;
}

static inline void pi_finish(param_interp* p){
    *p->state = p->value;
}

// units (SemitonesToRatio)
static inline float st2ratio(float semitones){
    return powf(2.0f, semitones / 12.0f);
}

// filter (Svf)
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define M_PI_F ((float)M_PI)
#define M_PI_POW_3 (M_PI_F * M_PI_F * M_PI_F)
#define M_PI_POW_5 (M_PI_POW_3 * M_PI_F * M_PI_F)

typedef struct{
    float g;
    float r;
    float h;
    float state_1;
    float state_2;
}svf;

static inline float onepole_tan_dirty(float f){
    const float a = 3.736e-01f * M_PI_POW_3;
    return f * (M_PI_F + a * f * f);
}

static inline void svf_init(svf* s){
  s->g = onepole_tan_dirty(0.01f);
  s->r = 1.0f / 100.0f;
  s->h = 1.0f / (1.0f + s->r * s->g + s->g * s->g);
  s->state_1 = 0.0f;
  s->state_2 = 0.0f;
}

static inline void svf_set_f_q_dirty(svf* s, float f, float resonance){
  s->g = onepole_tan_dirty(f);
  s->r = 1.0f / resonance;
  s->h = 1.0f / (1.0f + s->r * s->g + s->g * s->g);
}

static inline float onepole_tan_fast(float f){
  const float a = 3.260e-01f * M_PI_POW_3;
  const float b = 1.823e-01f * M_PI_POW_5;
  float f2 = f * f;
  return f * (M_PI_F + f2 * (a + b * f2));
}

static inline void svf_set_f_q_fast(svf* s, float f, float resonance){
  s->g = onepole_tan_fast(f);
  s->r = 1.0f / resonance;
  s->h = 1.0f / (1.0f + s->r * s->g + s->g * s->g);
}

static inline float svf_process_lp(svf* s, float in){
    float hp = (in - s->r * s->state_1 - s->g * s->state_1 - s->state_2) * s->h;
    float bp = s->g * hp + s->state_1;
    s->state_1 = s->g * hp + bp;
    float lp = s->g * bp + s->state_2;
    s->state_2 = s->g * bp + lp;
    return lp;
}

static inline void svf_process_bp_lp(svf* s, float in, float* out_bp, float* out_lp){
    float hp = (in - s->r * s->state_1 - s->g * s->state_1 - s->state_2) * s->h;
    float bp = s->g * hp + s->state_1;
    s->state_1 = s->g * hp + bp;
    float lp = s->g * bp + s->state_2;
    s->state_2 = s->g * bp + lp;
    *out_bp = bp;
    *out_lp = lp;
}

// Engine parameters + trigger state
#define kMaxBlockSize 16

// AnalogBassDrum ==================================================================
typedef struct{
    int   pulse_remaining_samples;
    int   fm_pulse_remaining_samples;
    float pulse;
    float pulse_height;
    float pulse_lp;
    float fm_pulse_lp;
    float retrig_pulse;
    float lp_out;
    float tone_lp;
    float od_pre_gain;
    float od_post_gain;
    svf   resonator;
}analog_bd;

static inline void analog_bd_init(analog_bd* d){
    d->pulse_remaining_samples = 0;
    d->fm_pulse_remaining_samples = 0;
    d->pulse = 0.0f;
    d->pulse_height = 0.0f;
    d->pulse_lp = 0.0f;
    d->fm_pulse_lp = 0.0f;
    d->retrig_pulse = 0.0f;
    d->lp_out = 0.0f;
    d->tone_lp = 0.0f;
    d->od_pre_gain = 0.0f;
    d->od_post_gain = 0.0f;
    svf_init(&d->resonator);
}

static inline float analog_bd_diode(float x){
    if(x >= 0.0f)
        return x;
    else{
        x *= 2.0f;
        return(0.7f * x / (1.0f + fabsf(x)));
    }
}

static inline void analog_bd_render(analog_bd* d, int trigger, float accent, float f0,
float tone, float decay, float attack_fm_amount, float self_fm_amount, float *out,
size_t size){
    const int trigger_pulse_duration = (int)(1.0e-3f * k_SR);
    const int fm_pulse_duration = (int)(6.0e-3f * k_SR);
    const float pulse_decay_time = 0.2e-3f * k_SR;
    const float pulse_filter_time = 0.1e-3f * k_SR;
    const float retrig_pulse_duration = 0.05f * k_SR;
    const float scale = 0.001f / f0;
    const float q = 1500.0f * st2ratio(decay * 80.0f);
    const float tone_f_raw = 4.0f * f0 * st2ratio(tone * 108.0f);
    const float tone_f = tone_f_raw < 1.0f ? tone_f_raw : 1.0f;
    const float exciter_leak = 0.08f * (tone + 0.25f);
    if(trigger){
        d->pulse_remaining_samples = trigger_pulse_duration;
        d->fm_pulse_remaining_samples = fm_pulse_duration;
        d->pulse_height = 3.0f + 7.0f * accent;
        d->lp_out = 0.0f;
    }
    while(size--){
        float pulse = 0.0f;
        if(d->pulse_remaining_samples){
            --d->pulse_remaining_samples;
            pulse = d->pulse_remaining_samples ? d->pulse_height : d->pulse_height - 1.0f;
            d->pulse = pulse;
        }
        else{
            d->pulse *= 1.0f - 1.0f / pulse_decay_time;
            pulse = d->pulse;
        }
        ONE_POLE(d->pulse_lp, pulse, 1.0f / pulse_filter_time);
        pulse = analog_bd_diode((pulse - d->pulse_lp) + pulse * 0.044f);
        float fm_pulse = 0.0f;
        if(d->fm_pulse_remaining_samples){
            --d->fm_pulse_remaining_samples;
            fm_pulse = 1.0f;
            d->retrig_pulse = d->fm_pulse_remaining_samples ? 0.0f : -0.8f;
        }
        else
            d->retrig_pulse *= 1.0f - 1.0f / retrig_pulse_duration;
        ONE_POLE(d->fm_pulse_lp, fm_pulse, 1.0f / pulse_filter_time);
        float punch = 0.7f + analog_bd_diode(10.0f * d->lp_out - 1.0f);
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

// SyntheticBassDrum ===============================================================
typedef struct{
  float lp;
  float hp;
  svf   filter;
}synth_bd_click;

static inline float synth_bd_click_process(synth_bd_click* c, float in){
  SLOPE(c->lp, in, 0.5f, 0.1f);
  ONE_POLE(c->hp, c->lp, 0.04f);
  return svf_process_lp(&c->filter, c->lp - c->hp);
}

typedef struct{
  float lp;
  float hp;
}synth_bd_attack_noise;

static inline float synth_bd_attack_noise_render(synth_bd_attack_noise* n){
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
  synth_bd_click click;
  synth_bd_attack_noise noise;
  int body_env_pulse_width;
  int fm_pulse_width;
}synth_bd;

static inline void synth_bd_click_init(synth_bd_click* c){
  c->lp = 0.0f;
  c->hp = 0.0f;
  c->filter.state_1 = 0.0f;
  c->filter.state_2 = 0.0f;
  svf_set_f_q_fast(&c->filter, 5000.0f / k_SR, 2.0f);
}

static inline void synth_bd_init(synth_bd* d){
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
  d->noise.lp = 0.0f;
  d->noise.hp = 0.0f;
  synth_bd_click_init(&d->click);
}

static inline float synth_bd_distorted_sine(float phase, float phase_noise, float dirtiness){
  phase += phase_noise * dirtiness;
  phase -= (float)((int32_t)phase);
  float triangle = (phase < 0.5f ? phase : 1.0f - phase) * 4.0f - 1.0f;
  float s = 2.0f * triangle / (1.0f + fabsf(triangle));
  float clean_sine = read_sintab(phase + 0.75f);
  return s + (1.0f - dirtiness) * (clean_sine - s);
}

static inline float synth_bd_transistor_vca(float s, float gain){
  s = (s - 0.6f) * gain;
  return 3.0f * s / (2.0f + fabsf(s)) + gain * 0.3f;
}

static inline void synth_bd_render(
    synth_bd* d,
    int trigger,
    float accent,
    float f0,
    float tone,
    float decay,
    float dirtiness,
    float fm_envelope_amount,
    float fm_envelope_decay,
    float* out,
    size_t size){
  decay *= decay;
  fm_envelope_decay *= fm_envelope_decay;
  param_interp f0_mod;
  pi_init(&f0_mod, &d->f0, f0, size);
  {
    float max_d = 1.0f - 8.0f * f0;
    dirtiness *= max_d > 0.0f ? max_d : 0.0f;
  }
  const float fm_decay = 1.0f - 1.0f / (0.008f * (1.0f + fm_envelope_decay * 4.0f) * k_SR);
  const float body_env_decay = 1.0f - 1.0f / (0.02f * k_SR) * st2ratio(-decay * 60.0f);
  const float transient_env_decay = 1.0f - 1.0f / (0.005f * k_SR);
  const float tone_f_raw = 4.0f * f0 * st2ratio(tone * 108.0f);
  const float tone_f = tone_f_raw < 1.0f ? tone_f_raw : 1.0f;
  const float transient_level = tone;
  if(trigger){
    d->fm = 1.0f;
    d->body_env = d->transient_env = 0.3f + 0.7f * accent;
    d->body_env_pulse_width = (int)(k_SR * 0.001f);
    d->fm_pulse_width = (int)(k_SR * 0.0013f);
  }
  while(size--){
    ONE_POLE(d->phase_noise, random_get_float() - 0.5f, 0.002f);
    float mix = 0.0f;
    if(d->fm_pulse_width){
      --d->fm_pulse_width;
      d->phase = 0.25f;
    }
    else{
      d->fm *= fm_decay;
      float fm = 1.0f + fm_envelope_amount * 3.5f * d->fm_lp;
      float phase_inc = pi_next(&f0_mod) * fm;
      if(phase_inc > 0.5f) phase_inc = 0.5f;
      d->phase += phase_inc;
      if(d->phase >= 1.0f)
        d->phase -= 1.0f;
    }
    if(d->body_env_pulse_width)
      --d->body_env_pulse_width;
    else{
      d->body_env *= body_env_decay;
      d->transient_env *= transient_env_decay;
    }
    const float envelope_lp_f = 0.1f;
    ONE_POLE(d->body_env_lp, d->body_env, envelope_lp_f);
    ONE_POLE(d->transient_env_lp, d->transient_env, envelope_lp_f);
    ONE_POLE(d->fm_lp, d->fm, envelope_lp_f);
    float body = synth_bd_distorted_sine(d->phase, d->phase_noise, dirtiness);
    float transient = synth_bd_click_process(&d->click, d->body_env_pulse_width ? 0.0f : 1.0f)
        + synth_bd_attack_noise_render(&d->noise);
    mix -= synth_bd_transistor_vca(body, d->body_env_lp);
    mix -= transient * d->transient_env_lp * transient_level;
    ONE_POLE(d->tone_lp, mix, tone_f);
    *out++ = d->tone_lp;
  }
  pi_finish(&f0_mod);
}

// Voice ========================================================================
typedef struct{
    analog_bd analog_bd;
    synth_bd synth_bd;
    float out_gain;
    int trigger_state;
    float decay_env;
    float out_buffer[kMaxBlockSize];
}voice;

static inline void voice_init(voice* v){
    v->out_gain = 0.8f;
    analog_bd_init(&v->analog_bd);
    synth_bd_init(&v->synth_bd);
    v->decay_env = 0.0f;
    v->trigger_state = 0;
}

// Pd glue ========================================================================
static t_class *bd_class;

typedef struct _bd{
    t_object    x_obj;
    t_int       x_mode;
    t_int       x_k_trig;
    t_int       x_n;
    t_int       x_nsize;
    t_int       x_block_count;
    t_int       x_nchans;
    voice      *x_voice;
    float       x_note;
    float       x_harmonics;
    float       x_timbre;
    float       x_morph;
    float       x_decay;
    float       x_pdepth;
    float       x_level;
    float       x_trigger;
}t_bd;

static void bd_freq(t_bd* x, t_floatarg f){
    t_floatarg pitch = log2f((f < 0 ? f * -1 : f) / 440) + 0.75;
    x->x_note = 60.0f + pitch * 12.f;
}

static void bd_bang(t_bd* x){
    x->x_k_trig = 1;
}

static void bd_mode(t_bd* x, t_floatarg f){
    x->x_mode = f != 0;
}

static void bd_punch(t_bd* x, t_floatarg f){
    x->x_harmonics = f < 0 ? 0 : f > 1 ? 1 : f;
}

static void bd_tone(t_bd* x, t_floatarg f){
    x->x_timbre = f < 0 ? 0 : f > 1 ? 1 : f;
}

static void bd_decay(t_bd* x, t_floatarg f){
    x->x_morph = f < 0 ? 0 : f > 1 ? 1 : f;
}

static void bd_level(t_bd* x, t_floatarg f){
    x->x_level = f < 0 ? 0 : f > 1 ? 1 : f;
}

static void bd_ptime(t_bd* x, t_floatarg f){
    x->x_decay = f < 0 ? 0 : f > 1 ? 1 : f;
}

static void bd_pdepth(t_bd* x, t_floatarg f){
    x->x_pdepth = f < 0 ? 0 : f > 1 ? 1 : f;
}

static inline void ch_post_process(float gain, float* in, short* out, size_t size){
    const float post_gain = gain * -32767.0f;
    while(size--){
        int32_t v = 1 + (int32_t)(*in++ * post_gain);
        if(v < -32768)
            v = -32768;
        else if(v > 32767)
            v = 32767;
        *out++ = (short)v;
    }
}

static inline void voice_render(t_bd* x, int channel, short* out, size_t size){
  voice* v = &x->x_voice[channel];
  float trigger_value = x->x_trigger;
  int previous_trigger_state = v->trigger_state;
  if(!previous_trigger_state){
    if(trigger_value > 0.3f){
      v->trigger_state = 1;
      v->decay_env = 1.0f;
    }
  }
  else if(trigger_value < 0.1f)
    v->trigger_state = 0;
  int rising_edge = v->trigger_state && !previous_trigger_state;
  const float short_decay = (100.0f * kMaxBlockSize) / k_SR * st2ratio(-96.0f * x->x_decay);
  v->decay_env *= (1.0f - short_decay * 2.0f);
  float accent = 1.3f * x->x_level / (0.3f + fabsf(x->x_level));
  float note;
  {
    float env_val = v->decay_env;
    float mod_amt = x->x_pdepth;
    float m = fabsf(mod_amt) - 0.05f;
    if(m < 0.05f) m = 0.05f;
    mod_amt *= m * 1.05f;
    note = x->x_note + mod_amt * (env_val * env_val * 48.0f);
    CONSTRAIN(note, -119.0f, 120.0f);
  }
  float nn = note - 9.0f; // note_to_frequency
  CONSTRAIN(nn, -128.0f, 127.0f);
  const float f0 = a0 * 0.25f * st2ratio(nn);
  if(x->x_mode){
    float d_aux = 0.4f - 0.25f * x->x_morph * x->x_morph;
    float fma = x->x_harmonics * 2.0f;
    float fmd = x->x_harmonics * 2.0f - 1.0f;
    synth_bd_render(&v->synth_bd, rising_edge, accent, f0, x->x_timbre, x->x_morph,
        d_aux, fma < 1.0f ? fma : 1.0f, fmd > 0.0f ? fmd : 0.0f, v->out_buffer, size);
  }
  else{
    float a = x->x_harmonics * 4.0f;
    float b = x->x_harmonics * 4.0f - 1.0f;
    float c = x->x_harmonics * 2.0f - 1.0f;
    float d = 1.0f - 16.0f * f0;
    float attack_fm_amount = a < 1.0f ? a : 1.0f;
    float self_fm_amount = (b < 1.0f ? b : 1.0f);
    if(self_fm_amount < 0.0f) self_fm_amount = 0.0f;
    float drive = (c > 0.0f ? c : 0.0f) * (d > 0.0f ? d : 0.0f);
    analog_bd_render(&v->analog_bd, rising_edge, accent, f0, x->x_timbre, x->x_morph,
        attack_fm_amount, self_fm_amount, v->out_buffer, size);
    float dr = 0.5f + 0.5f * drive;
    float dr2 = dr * dr;
    float pre_gain_a = dr * 0.5f;
    float pre_gain_b = dr2 * dr2 * dr * 24.0f;
    float pre_gain = pre_gain_a + (pre_gain_b - pre_gain_a) * dr2;
    float dr_sq = dr * (2.0f - dr);
    float post_gain = 1.0f / soft_clip(0.33f + dr_sq * (pre_gain - 0.33f));
    param_interp pre;
    param_interp post;
    pi_init(&pre, &v->analog_bd.od_pre_gain, pre_gain, size);
    pi_init(&post, &v->analog_bd.od_post_gain, post_gain, size);
    for(size_t i = 0; i < size; i++){
        float s = pi_next(&pre) * v->out_buffer[i];
        v->out_buffer[i] = soft_clip(s) * pi_next(&post);
    }
    pi_finish(&pre);
    pi_finish(&post);
  }
  ch_post_process(v->out_gain, v->out_buffer, out, size);
}

static t_int* bd_perform(t_int* w){
    t_bd* x = (t_bd*)(w[1]);
    t_sample* in = (t_sample*)(w[2]);
    t_sample* out = (t_sample*)(w[3]);
    int n = x->x_n;
    short output[kMaxBlockSize];
    for(int c = 0; c < x->x_nchans; c++){
        t_sample* trig = in + c * n;
        t_sample* outc = out + c * n;
        for(int j = 0; j < x->x_block_count; j++){
            int base = x->x_nsize * j;
            int trigger_at = -1;
            for(int i = 0; i < x->x_nsize; i++){
                if(trig[base + i] != 0){
                    float lvl = fabsf(trig[base + i]);
                    x->x_level = lvl < 0 ? 0 : lvl > 1 ? 1 : lvl;
                    trigger_at = i;
                    break;
                }
            }
            if(x->x_k_trig){
                if(trigger_at < 0)
                    trigger_at = 0;
                x->x_k_trig = 0;
            }
            if(trigger_at > 0){
                x->x_trigger = 0;
                voice_render(x, c, output, trigger_at);
                for(int i = 0; i < trigger_at; i++)
                    outc[i + base] = (float)output[i] / 32768.0f;
                x->x_trigger = 1;
                voice_render(x, c, output, x->x_nsize - trigger_at);
                for(int i = 0; i < x->x_nsize - trigger_at; i++)
                    outc[i + base + trigger_at] = (float)output[i] / 32768.0f;
            }
            else{
                x->x_trigger = (trigger_at == 0);
                voice_render(x, c, output, x->x_nsize);
                for(int i = 0; i < x->x_nsize; i++)
                    outc[i + base] = (float)output[i] / 32768.0f;
            }
        }
    }
    return(w+4);
}

static void bd_dsp(t_bd* x, t_signal** sp){
    k_SR = (float)sp[0]->s_sr;
    a0 = 55.f / k_SR;
    int n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    if(n != x->x_n){
        if(n >= 16){
            x->x_nsize = 16;
            x->x_block_count = n / x->x_nsize;
        }
        else{
            x->x_nsize = n;
            x->x_block_count = 1;
        }
        x->x_n = n;
    }
    if(chs != x->x_nchans){
        x->x_voice = (voice*)resizebytes(x->x_voice,
            x->x_nchans * sizeof(voice), chs * sizeof(voice));
        for(int c = x->x_nchans; c < chs; c++)
            voice_init(&x->x_voice[c]);
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(bd_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
}

static void* bd_new(t_symbol* s, int ac, t_atom* av){
    (void)s;
    t_bd* x = (t_bd*)pd_new(bd_class);
    float pitch = 50, punch = 0.5f, tone = 0.5f, decay = 0.5f, ptime = 0, pdepth = 0, lvl = 0.5f;
    int nfloats = 0;
    x->x_mode = 0;
    while(ac){
        if(av->a_type == A_SYMBOL){
            t_symbol* sym = atom_getsymbol(av);
            ac--, av++;
            if(sym == gensym("-mode") && ac && av->a_type == A_FLOAT){
                x->x_mode = atom_getfloat(av) != 0;
                ac--, av++;
            } else
                goto errstate;
        }
        else{
            float f = atom_getfloat(av);
            ac--, av++;
            switch(nfloats++){
                case 0: pitch = f; break;
                case 1: lvl = f; break;
                case 2: punch = f; break;
                case 3: tone = f; break;
                case 4: decay = f; break;
                case 5: ptime = f; break;
                case 6: pdepth = f; break;
                default: goto errstate;
            }
        }
    }
    x->x_trigger = 0;
    x->x_k_trig = 0;
    x->x_n = 0;
    x->x_nchans = 1;
    x->x_voice = (voice*)getbytes(sizeof(voice));
    bd_level(x, lvl);
    bd_freq(x, pitch);
    bd_punch(x, punch);
    bd_tone(x, tone);
    bd_decay(x, decay);
    bd_ptime(x, ptime);
    bd_pdepth(x, pdepth);
    voice_init(&x->x_voice[0]);
    outlet_new(&x->x_obj, &s_signal);
    return(void*)x;
errstate:
    pd_error(x, "[bassdrum~]: improper args");
    return (NULL);
}

void bassdrum_tilde_setup(void){
    init_sine_table();
    bd_class = class_new(gensym("bassdrum~"), (t_newmethod)bd_new,
        0, sizeof(t_bd), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(bd_class, (t_method)bd_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(bd_class, nullfn, gensym("signal"), A_NULL);
    class_addbang(bd_class, (t_method)bd_bang);
    class_addmethod(bd_class, (t_method)bd_mode, gensym("mode"), A_FLOAT, 0);
    class_addmethod(bd_class, (t_method)bd_freq, gensym("freq"), A_FLOAT, 0);
    class_addmethod(bd_class, (t_method)bd_level, gensym("level"), A_FLOAT, 0);
    class_addmethod(bd_class, (t_method)bd_punch, gensym("punch"), A_FLOAT, 0);
    class_addmethod(bd_class, (t_method)bd_tone, gensym("tone"), A_FLOAT, 0);
    class_addmethod(bd_class, (t_method)bd_decay, gensym("decay"), A_FLOAT, 0);
    class_addmethod(bd_class, (t_method)bd_ptime, gensym("ptime"), A_FLOAT, 0);
    class_addmethod(bd_class, (t_method)bd_pdepth, gensym("pdepth"), A_FLOAT, 0);
}

void bd_tilde_setup(void){
    init_sine_table();
    bd_class = class_new(gensym("bd~"), (t_newmethod)bd_new,
        0, sizeof(t_bd), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(bd_class, (t_method)bd_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(bd_class, nullfn, gensym("signal"), A_NULL);
    class_addbang(bd_class, (t_method)bd_bang);
    class_addmethod(bd_class, (t_method)bd_mode, gensym("mode"), A_FLOAT, 0);
    class_addmethod(bd_class, (t_method)bd_freq, gensym("freq"), A_FLOAT, 0);
    class_addmethod(bd_class, (t_method)bd_level, gensym("level"), A_FLOAT, 0);
    class_addmethod(bd_class, (t_method)bd_punch, gensym("punch"), A_FLOAT, 0);
    class_addmethod(bd_class, (t_method)bd_tone, gensym("tone"), A_FLOAT, 0);
    class_addmethod(bd_class, (t_method)bd_decay, gensym("decay"), A_FLOAT, 0);
    class_addmethod(bd_class, (t_method)bd_ptime, gensym("ptime"), A_FLOAT, 0);
    class_addmethod(bd_class, (t_method)bd_pdepth, gensym("pdepth"), A_FLOAT, 0);
    class_sethelpsymbol(bd_class, gensym("bassdrum~"));
}
