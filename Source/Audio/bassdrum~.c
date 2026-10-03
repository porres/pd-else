// based on the plaits' bassdrum engine by Mutable Instruments (MIT)
// rewritten, refactored and redesigned by Porres
// Original copyright Emilie Gillet, MIT license.

#include <m_pd.h>
#include <math.h>
#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <buffer.h>

#define kBlockSize 16
#define kOutGain   0.8f

// utilities ========================================================================
#define ONE_POLE(out, in, coefficient) out += (coefficient) * ((in) - out)

static inline float clampf(float x, float lo, float hi){
    return x < lo ? lo : x > hi ? hi : x;
}

static inline float clamp01(float x){
    return clampf(x, 0.0f, 1.0f);
}

static inline float st2ratio(float semitones){
    return powf(2.0f, semitones / 12.0f);
}

// Cutoff of the output low-pass shared by both engines
static inline float tone_filter_coeff(float f0, float tone){
    return fminf(4.0f * f0 * st2ratio(tone * 108.0f), 1.0f);
}

// Random (state lives in the voice)
static inline float random_get_float(uint32_t* state){
    *state = *state * 1664525u + 1013904223u;
    return (float)*state / 4294967296.0f;
}

// scrambles a seed so that similar inputs (e.g. consecutive seconds) end up far apart
static inline uint32_t hash_seed(uint32_t h){
    h ^= h >> 16; h *= 0x85ebca6bu;
    h ^= h >> 13; h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return h;
}

// ParameterInterpolator
typedef struct{
    float *state;
    float  value;
    float  increment;
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

// filter (Svf) =====================================================================
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define M_PI_F ((float)M_PI)
#define M_PI_POW_3 (M_PI_F * M_PI_F * M_PI_F)
#define M_PI_POW_5 (M_PI_POW_3 * M_PI_F * M_PI_F)

typedef struct{
    float g, r, h;
    float state_1, state_2;
}svf;

static inline float onepole_tan_dirty(float f){
    const float a = 3.736e-01f * M_PI_POW_3;
    return f * (M_PI_F + a * f * f);
}

static inline float onepole_tan_fast(float f){
    const float a = 3.260e-01f * M_PI_POW_3;
    const float b = 1.823e-01f * M_PI_POW_5;
    float f2 = f * f;
    return f * (M_PI_F + f2 * (a + b * f2));
}

// g: prewarped cutoff (onepole_tan_*), resonance: Q
static inline void svf_set(svf* s, float g, float resonance){
    s->g = g;
    s->r = 1.0f / resonance;
    s->h = 1.0f / (1.0f + s->r * s->g + s->g * s->g);
}

static inline void svf_init(svf* s){
    s->state_1 = s->state_2 = 0.0f;
    svf_set(s, onepole_tan_dirty(0.01f), 100.0f);
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

static inline float svf_process_lp(svf* s, float in){
    float bp, lp;
    svf_process_bp_lp(s, in, &bp, &lp);
    return lp;
}

static inline float soft_clip(float x){
    if(x < -3.0f)
        return -1.0f;
    else if(x > 3.0f)
        return 1.0f;
    else
        return x * (27.0f + x * x) / (27.0f + 9.0f * x * x);
}

// AnalogBassDrum ===================================================================
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
    memset(d, 0, sizeof(*d));
    svf_init(&d->resonator);
}

static inline float analog_bd_diode(float x){
    if(x >= 0.0f)
        return x;
    x *= 2.0f;
    return 0.7f * x / (1.0f + fabsf(x));
}

// SyntheticBassDrum ================================================================
typedef struct{
    float lp, hp;
    svf   filter;
}synth_bd_click;

static inline void synth_bd_click_init(synth_bd_click* c, float sr){
    memset(c, 0, sizeof(*c));
    svf_set(&c->filter, onepole_tan_fast(5000.0f / sr), 2.0f);
}

static inline float synth_bd_click_process(synth_bd_click* c, float in){
    float error = in - c->lp;
    c->lp += (error > 0 ? 0.5f : 0.1f) * error;
    ONE_POLE(c->hp, c->lp, 0.04f);
    return svf_process_lp(&c->filter, c->lp - c->hp);
}

typedef struct{
    float lp, hp;
}synth_bd_attack_noise;

static inline float synth_bd_attack_noise_render(synth_bd_attack_noise* n, uint32_t* rng){
    float sample = random_get_float(rng);
    ONE_POLE(n->lp, sample, 0.05f);
    ONE_POLE(n->hp, n->lp, 0.005f);
    return n->lp - n->hp;
}

typedef struct{
    float                 f0;
    float                 phase;
    float                 phase_noise;
    float                 fm;
    float                 fm_lp;
    float                 body_env;
    float                 body_env_lp;
    float                 transient_env;
    float                 transient_env_lp;
    float                 tone_lp;
    synth_bd_click        click;
    synth_bd_attack_noise noise;
    int                   body_env_pulse_width;
    int                   fm_pulse_width;
}synth_bd;

static inline void synth_bd_init(synth_bd* d, float sr){
    memset(d, 0, sizeof(*d));
    synth_bd_click_init(&d->click, sr);
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

// Voice + Pd object ================================================================
typedef struct{
    analog_bd analog_bd;
    synth_bd  synth_bd;
    uint32_t  rng;
    int       trigger_state;
    float     decay_env;
    float     out_buffer[kBlockSize];
}voice;

static t_class *bd_class;

typedef struct _bd{
    t_object  x_obj;
    t_int     x_mode;
    t_int     x_k_trig;
    t_int     x_n;
    t_int     x_nsize;
    t_int     x_block_count;
    t_int     x_nchans;
    uint32_t  x_seed;
    float     x_sr;
    voice    *x_voice;
    float     x_note;
    float     x_punch;
    float     x_tone;
    float     x_decay;
    float     x_ptime;
    float     x_pdepth;
    float     x_level;
}t_bd;

static inline void voice_init(voice* v, float sr, uint32_t seed){
    memset(v, 0, sizeof(*v));
    v->rng = seed;
    analog_bd_init(&v->analog_bd);
    synth_bd_init(&v->synth_bd, sr);
}

// Engine render ====================================================================
static inline void analog_bd_render(t_bd* x, voice* v, int trigger, float accent,
float f0, size_t size){
    analog_bd* d = &v->analog_bd;
    float* out = v->out_buffer;
    float tone = x->x_tone;
    float decay = x->x_decay;
    float sr = x->x_sr;
    const float punch = x->x_punch;
    const float attack_fm_amount = fminf(punch * 4.0f, 1.0f);
    const float self_fm_amount = clamp01(punch * 4.0f - 1.0f);
    const float drive = fmaxf(punch * 2.0f - 1.0f, 0.0f) * fmaxf(1.0f - 16.0f * f0, 0.0f);
    const int trigger_pulse_duration = (int)(1.0e-3f * sr);
    const int fm_pulse_duration = (int)(6.0e-3f * sr);
    const float pulse_decay_time = 0.2e-3f * sr;
    const float pulse_filter_time = 0.1e-3f * sr;
    const float retrig_pulse_duration = 0.05f * sr;
    const float scale = 0.001f / f0;
    const float q = 1500.0f * st2ratio(decay * 80.0f);
    const float tone_f = tone_filter_coeff(f0, tone);
    const float exciter_leak = 0.08f * (tone + 0.25f);
    // overdrive gains (interpolated per sample below)
    const float dr = 0.5f + 0.5f * drive;
    const float dr2 = dr * dr;
    const float pre_gain_a = dr * 0.5f;
    const float pre_gain_b = dr2 * dr2 * dr * 24.0f;
    const float pre_gain = pre_gain_a + (pre_gain_b - pre_gain_a) * dr2;
    const float dr_sq = dr * (2.0f - dr);
    const float post_gain = 1.0f / soft_clip(0.33f + dr_sq * (pre_gain - 0.33f));
    param_interp pre, post;
    pi_init(&pre, &d->od_pre_gain, pre_gain, size);
    pi_init(&post, &d->od_post_gain, post_gain, size);
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
        float punch_env = 0.7f + analog_bd_diode(10.0f * d->lp_out - 1.0f);
        float attack_fm = d->fm_pulse_lp * 1.7f * attack_fm_amount;
        float self_fm = punch_env * 0.08f * self_fm_amount;
        float f = clampf(f0 * (1.0f + attack_fm + self_fm), 0.0f, 0.4f);
        float resonator_out;
        svf_set(&d->resonator, onepole_tan_dirty(f), 1.0f + q * f);
        svf_process_bp_lp(&d->resonator, (pulse - d->retrig_pulse * 0.2f) * scale,
            &resonator_out, &d->lp_out);
        ONE_POLE(d->tone_lp, pulse * exciter_leak + resonator_out, tone_f);
        float s = pi_next(&pre) * d->tone_lp;
        *out++ = soft_clip(s) * pi_next(&post);
    }
    pi_finish(&pre);
    pi_finish(&post);
}

static inline void synth_bd_render(t_bd* x, voice* v, int trigger, float accent,
float f0, size_t size){
    synth_bd* d = &v->synth_bd;
    uint32_t* rng = &v->rng;
    float* out = v->out_buffer;
    float tone = x->x_tone;
    float decay = x->x_decay;
    float sr = x->x_sr;
    float dirtiness = 0.4f - 0.25f * decay * decay;
    float fm_envelope_amount = fminf(x->x_punch * 2.0f, 1.0f);
    float fm_envelope_decay = fmaxf(x->x_punch * 2.0f - 1.0f, 0.0f);
    decay *= decay;
    fm_envelope_decay *= fm_envelope_decay;
    param_interp f0_mod;
    pi_init(&f0_mod, &d->f0, f0, size);
    dirtiness *= fmaxf(1.0f - 8.0f * f0, 0.0f);
    const float fm_decay = 1.0f - 1.0f / (0.008f * (1.0f + fm_envelope_decay * 4.0f) * sr);
    const float body_env_decay = 1.0f - 1.0f / (0.02f * sr) * st2ratio(-decay * 60.0f);
    const float transient_env_decay = 1.0f - 1.0f / (0.005f * sr);
    const float tone_f = tone_filter_coeff(f0, tone);
    const float transient_level = tone;
    const float envelope_lp_f = 0.1f;
    if(trigger){
        d->fm = 1.0f;
        d->body_env = d->transient_env = 0.3f + 0.7f * accent;
        d->body_env_pulse_width = (int)(sr * 0.001f);
        d->fm_pulse_width = (int)(sr * 0.0013f);
    }
    while(size--){
        ONE_POLE(d->phase_noise, random_get_float(rng) - 0.5f, 0.002f);
        if(d->fm_pulse_width){
            --d->fm_pulse_width;
            d->phase = 0.25f;
        }
        else{
            d->fm *= fm_decay;
            float fm = 1.0f + fm_envelope_amount * 3.5f * d->fm_lp;
            float phase_inc = fminf(pi_next(&f0_mod) * fm, 0.5f);
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
        ONE_POLE(d->body_env_lp, d->body_env, envelope_lp_f);
        ONE_POLE(d->transient_env_lp, d->transient_env, envelope_lp_f);
        ONE_POLE(d->fm_lp, d->fm, envelope_lp_f);
        float body = synth_bd_distorted_sine(d->phase, d->phase_noise, dirtiness);
        float transient = synth_bd_click_process(&d->click, d->body_env_pulse_width ? 0.0f : 1.0f)
            + synth_bd_attack_noise_render(&d->noise, rng);
        float mix = -synth_bd_transistor_vca(body, d->body_env_lp)
            - transient * d->transient_env_lp * transient_level;
        ONE_POLE(d->tone_lp, mix, tone_f);
        *out++ = d->tone_lp;
    }
    pi_finish(&f0_mod);
}

// Pd glue ==========================================================================
static void bd_freq(t_bd* x, t_floatarg f){ // Hz -> MIDI note
    x->x_note = 69.0f + 12.0f * log2f(fabsf(f) / 440.0f);
}

static void bd_bang(t_bd* x){
    x->x_k_trig = 1;
}

static void bd_mode(t_bd* x, t_floatarg f){
    t_int new_mode = f != 0;
    if(new_mode != x->x_mode){
        x->x_mode = new_mode;
        for(int c = 0; c < x->x_nchans; c++){
            if(new_mode)
                synth_bd_init(&x->x_voice[c].synth_bd, x->x_sr);
            else
                analog_bd_init(&x->x_voice[c].analog_bd);
        }
    }
}

#define BD_SETTER(name, field) \
    static void name(t_bd* x, t_floatarg f){ x->field = clamp01(f); }

BD_SETTER(bd_punch, x_punch)
BD_SETTER(bd_tone, x_tone)
BD_SETTER(bd_decay, x_decay)
BD_SETTER(bd_level, x_level)
BD_SETTER(bd_ptime, x_ptime)
BD_SETTER(bd_pdepth, x_pdepth)

static inline void voice_render(t_bd* x, int channel, int trigger, t_sample* out, size_t size){
    voice* v = &x->x_voice[channel];
    int rising_edge = trigger && !v->trigger_state;
    v->trigger_state = trigger;
    if(rising_edge)
        v->decay_env = 1.0f;
    const float short_decay = (100.0f * kBlockSize) / x->x_sr * st2ratio(-96.0f * x->x_ptime);
    v->decay_env *= 1.0f - short_decay * 2.0f;
    const float accent = 1.3f * x->x_level / (0.3f + fabsf(x->x_level));
    float mod_amt = x->x_pdepth; // pitch envelope
    mod_amt *= fmaxf(fabsf(mod_amt) - 0.05f, 0.05f) * 1.05f;
    float note = clampf(x->x_note + mod_amt * (v->decay_env * v->decay_env * 48.0f), -119.0f, 120.0f);
    const float f0 = 13.75f / x->x_sr * st2ratio(clampf(note - 9.0f, -128.0f, 127.0f));
    if(x->x_mode) // synthetic bass drum model (inadvertedly tr-909ish)
        synth_bd_render(x, v, rising_edge, accent, f0, size);
    else // analog bass drum model TR-808 like
        analog_bd_render(x, v, rising_edge, accent, f0, size);
    const float gain = kOutGain * -32767.0f; // float -> 16 bit (legacy from plaits)
    for(size_t i = 0; i < size; i++){
        int32_t s = 1 + (int32_t)(v->out_buffer[i] * gain);
        out[i] = (float)(s < -32768 ? -32768 : s > 32767 ? 32767 : s) / 32768.0f;
    }
}

static t_int* bd_perform(t_int* w){
    t_bd* x = (t_bd*)(w[1]);
    t_sample* in = (t_sample*)(w[2]);
    t_sample* out = (t_sample*)(w[3]);
    int n = x->x_n;
    int size = x->x_nsize;
    for(int c = 0; c < x->x_nchans; c++){
        for(int j = 0; j < x->x_block_count; j++){
            t_sample* trig = in + c * n + size * j;
            t_sample* outb = out + c * n + size * j;
            int trigger_at = -1;
            for(int i = 0; i < size; i++){
                if(trig[i] != 0){
                    x->x_level = fminf(fabsf(trig[i]), 1.0f);
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
                voice_render(x, c, 0, outb, trigger_at);
                voice_render(x, c, 1, outb + trigger_at, size - trigger_at);
            }
            else
                voice_render(x, c, trigger_at == 0, outb, size);
        }
    }
    return(w+4);
}

static void bd_dsp(t_bd* x, t_signal** sp){
    float sr = (float)sp[0]->s_sr;
    if(sr != x->x_sr){
        x->x_sr = sr;
        for(int c = 0; c < x->x_nchans; c++)
            synth_bd_click_init(&x->x_voice[c].synth_bd.click, sr);
    }
    int n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    if(n != x->x_n){
        x->x_nsize = n < kBlockSize ? n : kBlockSize;
        x->x_block_count = n / x->x_nsize;
        x->x_n = n;
    }
    if(chs != x->x_nchans){
        x->x_voice = (voice*)resizebytes(x->x_voice,
            x->x_nchans * sizeof(voice), chs * sizeof(voice));
        for(int c = x->x_nchans; c < chs; c++)
            voice_init(&x->x_voice[c], x->x_sr, x->x_seed + (uint32_t)c * 2654435761u);
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
    x->x_k_trig = 0;
    x->x_n = 0;
    x->x_nchans = 1;
    x->x_sr = sys_getsr();
    x->x_seed = hash_seed((uint32_t)time(NULL) ^ (uint32_t)(uintptr_t)x);
    x->x_voice = (voice*)getbytes(sizeof(voice));
    bd_level(x, lvl);
    bd_freq(x, pitch);
    bd_punch(x, punch);
    bd_tone(x, tone);
    bd_decay(x, decay);
    bd_ptime(x, ptime);
    bd_pdepth(x, pdepth);
    voice_init(&x->x_voice[0], x->x_sr, x->x_seed);
    outlet_new(&x->x_obj, &s_signal);
    return(void*)x;
errstate:
    pd_error(x, "[bassdrum~]: improper args");
    return (NULL);
}

static void bd_class_setup(const char* name){
    init_sine_table();
    bd_class = class_new(gensym(name), (t_newmethod)bd_new,
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

void bassdrum_tilde_setup(void){
    bd_class_setup("bassdrum~");
}

void bd_tilde_setup(void){
    bd_class_setup("bd~");
    class_sethelpsymbol(bd_class, gensym("bassdrum~"));
}
