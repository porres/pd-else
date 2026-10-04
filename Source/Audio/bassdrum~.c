// based on plaits' bassdrums by Mutable Instruments copyright Emilie Gillet (MIT License)
// Rewritten, refactored, redesigned, ported to plain C and inlined code by Porres

#include <m_pd.h>
#include <math.h>
#include <stddef.h>
#include <string.h>
#include <time.h>
#include <buffer.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define M_PI_F ((float)M_PI)
#define M_PI_POW_3 (M_PI_F * M_PI_F * M_PI_F)
#define M_PI_POW_5 (M_PI_POW_3 * M_PI_F * M_PI_F)
#define kBlockSize 16
#define REF_SR 48000.0f // original sample rate plaits constants were tuned at

static t_class *bd_class;

typedef struct{
    float g, r, h;
    float state_1, state_2;
}svf;

typedef struct{
    int   pulse_nsamps;
    int   fm_pulse_nsamps;
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
    float click_lp, click_hp;
    float noise_lp, noise_hp;
    svf   click_filter;
    int   body_env_pulse_width;
    int   fm_pulse_width;
}synth_bd;

typedef struct{
    analog_bd analog_bd;
    synth_bd  synth_bd;
    unsigned  rng; // each channel has its random state
    int       trigger_state;
    float     decay_env;
}voice; // channel voice

typedef struct _bd{
    t_object  x_obj;
    voice    *x_channel_voice;
    t_int     x_mode;
    t_int     x_k_trig;
    t_int     x_n;
    t_int     x_nsize;
    t_int     x_block_count;
    t_int     x_nchans;
    unsigned  x_seed;
    float     x_sr;
    float     x_freq;
    float     x_punch;
    float     x_tone;
    float     x_decay;
    float     x_ptime;
    float     x_pdepth;
    float     x_level;
    float     x_klvl;
}t_bd;

static inline float clampf(float x, float lo, float hi){ // utils <=====
    return(x < lo ? lo : x > hi ? hi : x);
}

static inline float st2ratio(float semitones){
    return(powf(2.0f, semitones / 12.0f));
}

static inline unsigned hash_seed(unsigned h){
    h ^= h >> 16; h *= 0x85ebca6bu;
    h ^= h >> 13; h *= 0xc2b2ae35u;
    h ^= h >> 16;
    return(h);
}

static inline float onepole_lp(float out, float in, float c){ // filters <=====
    return(out + c * (in - out));
}

static inline float onepole_coef(float c, float sr){
    return(-expm1f(log1pf(-c) * (REF_SR / sr)));
}

static inline void svf_process_bp_lp(svf* s, float in, float* out_bp, float* out_lp){
    float hp = (in - s->r * s->state_1 - s->g * s->state_1 - s->state_2) * s->h;
    float bp = s->g * hp + s->state_1;
    s->state_1 = s->g * hp + bp;
    float lp = s->g * bp + s->state_2;
    s->state_2 = s->g * bp + lp;
    if(PD_BIGORSMALL(s->state_1)) s->state_1 = 0.0f;
    if(PD_BIGORSMALL(s->state_2)) s->state_2 = 0.0f;
    *out_bp = bp;
    *out_lp = lp;
}

static inline void svf_set(svf* s, float f0, float reson){
    float a = 3.260e-01f * M_PI_POW_3, b = 1.823e-01f * M_PI_POW_5;
    float f2 = f0 * f0;
    s->g = f0 * (M_PI_F + f2 * (a + b * f2)); // tan(pi*f) approximation
    s->r = 1.0f / reson;
    s->h = 1.0f / (1.0f + s->r * s->g + s->g * s->g);
}

// Rendering ====================================================================
static inline void drum_init(voice* v, unsigned seed){
    memset(v, 0, sizeof(*v));
    v->rng = seed;
}

static inline float analog_bd_diode(float x){
    if(x >= 0.0f)
        return(x);
    x *= 2.0f;
    return(0.7f * x / (1.0f + fabsf(x)));
}

static inline float analog_soft_clip(float x){
    if(x < -3.0f)
        return(-1.0f);
    else if(x > 3.0f)
        return(1.0f);
    else
        return(x * (27.0f + x * x) / (27.0f + 9.0f * x * x));
}

static inline void analog_bd_render(t_bd* x, voice* v, int trigger, float accent, float f0, t_sample* out, size_t size){
    analog_bd* d = &v->analog_bd;
    float sr_ratio = x->x_sr / REF_SR;
    float f0_ref = f0 * sr_ratio; // f0 as it would be at 48k (for the Hz-tuned constants below)
    float attack_fm_amount = fminf(x->x_punch * 4.0f, 1.0f);
    float self_fm_amount = clampf(x->x_punch * 4.0f - 1.0f, 0.0f, 1.0f);
    float drive = fmaxf(x->x_punch * 2.0f - 1.0f, 0.0f) * fmaxf(1.0f - 16.0f * f0_ref, 0.0f);
    int trigger_pulse_duration = (int)(1.0e-3f * x->x_sr);
    int fm_pulse_duration = (int)(6.0e-3f * x->x_sr);
    float pulse_decay_time = 0.2e-3f * x->x_sr;
    float pulse_filter_time = 0.1e-3f * x->x_sr;
    float retrig_pulse_duration = 0.05f * x->x_sr;
    float scale = 0.001f / f0_ref;
    float q = 1500.0f * st2ratio(x->x_decay * 80.0f) * sr_ratio;
    float tone_f = fminf(4.0f * f0 * st2ratio(x->x_tone * 108.0f), 1.0f);
    float exciter_leak = 0.08f * (x->x_tone + 0.25f);
    float dr = 0.5f + 0.5f * drive; // overdrive gains (interpolated per sample below)
    float dr2 = dr * dr;
    float pre_gain_a = dr * 0.5f;
    float pre_gain_b = dr2 * dr2 * dr * 24.0f;
    float pre_gain = pre_gain_a + (pre_gain_b - pre_gain_a) * dr2;
    float dr_sq = dr * (2.0f - dr);
    float post_gain = 1.0f / analog_soft_clip(0.33f + dr_sq * (pre_gain - 0.33f));
    float pre = d->od_pre_gain, pre_inc = (pre_gain - pre) / (float)size;
    float post = d->od_post_gain, post_inc = (post_gain - post) / (float)size;
    if(trigger){
        d->pulse_nsamps = trigger_pulse_duration;
        d->fm_pulse_nsamps = fm_pulse_duration;
        d->pulse_height = 3.0f + 7.0f * accent;
        d->lp_out = 0.0f;
    }
    while(size--){
        float pulse = 0.0f;
        if(d->pulse_nsamps){
            --d->pulse_nsamps;
            pulse = d->pulse_nsamps ? d->pulse_height : d->pulse_height - 1.0f;
            d->pulse = pulse;
        }
        else{
            d->pulse *= 1.0f - 1.0f / pulse_decay_time;
            pulse = d->pulse;
        }
        d->pulse_lp = onepole_lp(d->pulse_lp, pulse, 1.0f / pulse_filter_time);
        pulse = analog_bd_diode((pulse - d->pulse_lp) + pulse * 0.044f);
        float fm_pulse = 0.0f;
        if(d->fm_pulse_nsamps){
            --d->fm_pulse_nsamps;
            fm_pulse = 1.0f;
            d->retrig_pulse = d->fm_pulse_nsamps ? 0.0f : -0.8f;
        }
        else
            d->retrig_pulse *= 1.0f - 1.0f / retrig_pulse_duration;
        d->fm_pulse_lp = onepole_lp(d->fm_pulse_lp, fm_pulse, 1.0f / pulse_filter_time);
        float punch_env = 0.7f + analog_bd_diode(10.0f * d->lp_out - 1.0f);
        float attack_fm = d->fm_pulse_lp * 1.7f * attack_fm_amount;
        float self_fm = punch_env * 0.08f * self_fm_amount;
        float f = clampf(f0 * (1.0f + attack_fm + self_fm), 0.0f, 0.4f);
        float resonator_out;
        svf_set(&d->resonator, f, 1.0f + q * f);
        svf_process_bp_lp(&d->resonator, (pulse - d->retrig_pulse * 0.2f) * scale,
            &resonator_out, &d->lp_out);
        d->tone_lp = onepole_lp(d->tone_lp, pulse * exciter_leak + resonator_out, tone_f);
        pre += pre_inc;
        post += post_inc;
        *out++ = analog_soft_clip(pre * d->tone_lp) * post;
    }
    d->od_pre_gain = PD_BIGORSMALL(pre) ? 0.0f : pre;
    d->od_post_gain = PD_BIGORSMALL(post) ? 0.0f : post;
}

static inline float random_get_float(unsigned* state){
    *state = *state * 1664525u + 1013904223u;
    return((float)*state / 4294967296.0f);
}

static inline void synth_bd_render(t_bd* x, voice* v, int trigger, float accent, float f0, t_sample* out, size_t size){
    synth_bd* d = &v->synth_bd;
    unsigned* rng = &v->rng;
    float transient_level = x->x_tone, decay = x->x_decay, sr = x->x_sr;
    float sr_ratio = sr / REF_SR;
    float noise_gain = sqrtf(sr_ratio); // keeps the filtered noise level the same at any sr
    float phase_noise_f = onepole_coef(0.002f, sr);
    float envelope_lp_f = onepole_coef(0.1f, sr);
    float click_up_f = onepole_coef(0.5f, sr), click_down_f = onepole_coef(0.1f, sr);
    float click_hp_f = onepole_coef(0.04f, sr);
    float noise_lp_f = onepole_coef(0.05f, sr), noise_hp_f = onepole_coef(0.005f, sr);
    float dirtiness = 0.4f - 0.25f * decay * decay;
    float fm_envelope_amount = fminf(x->x_punch * 2.0f, 1.0f);
    float fm_envelope_decay = fmaxf(x->x_punch * 2.0f - 1.0f, 0.0f);
    decay *= decay, fm_envelope_decay *= fm_envelope_decay;
    float f0_cur = d->f0, f0_inc = (f0 - f0_cur) / (float)size;
    dirtiness *= fmaxf(1.0f - 8.0f * f0 * sr_ratio, 0.0f);
    float fm_decay = 1.0f - 1.0f / (0.008f * (1.0f + fm_envelope_decay * 4.0f) * sr);
    float body_env_decay = 1.0f - 1.0f / (0.02f * sr) * st2ratio(-decay * 60.0f);
    float transient_env_decay = 1.0f - 1.0f / (0.005f * sr);
    float tone_f = fminf(4.0f * f0 * st2ratio(transient_level * 108.0f), 1.0f);
    svf_set(&d->click_filter, 5000.0f / sr, 2.0f);
    if(trigger){
        d->fm = 1.0f;
        d->body_env = d->transient_env = 0.3f + 0.7f * accent;
        d->body_env_pulse_width = (int)(sr * 0.001f);
        d->fm_pulse_width = (int)(sr * 0.0013f);
    }
    while(size--){
        d->phase_noise = onepole_lp(d->phase_noise, (random_get_float(rng) - 0.5f) * noise_gain, phase_noise_f);
        f0_cur += f0_inc;
        if(d->fm_pulse_width){
            --d->fm_pulse_width;
            d->phase = 0.25f;
        }
        else{
            d->fm *= fm_decay;
            float fm = 1.0f + fm_envelope_amount * 3.5f * d->fm_lp;
            float phase_inc = fminf(f0_cur * fm, 0.5f);
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
        d->body_env_lp = onepole_lp(d->body_env_lp, d->body_env, envelope_lp_f);
        d->transient_env_lp = onepole_lp(d->transient_env_lp, d->transient_env, envelope_lp_f);
        d->fm_lp = onepole_lp(d->fm_lp, d->fm, envelope_lp_f);
        float ph = d->phase + d->phase_noise * dirtiness;
        ph -= (float)((int)ph);
        float triangle = (ph < 0.5f ? ph : 1.0f - ph) * 4.0f - 1.0f;
        float tr = 2.0f * triangle / (1.0f + fabsf(triangle));
        float clean_sine = read_sintab(ph + 0.75f);
        float body = tr + (1.0f - dirtiness) * (clean_sine - tr); // distorted sine
        float error = (d->body_env_pulse_width ? 0.0f : 1.0f) - d->click_lp; // click
        d->click_lp += (error > 0 ? click_up_f : click_down_f) * error;
        d->click_hp = onepole_lp(d->click_hp, d->click_lp, click_hp_f);
        float click, click_bp; //  click_bp is dummy/unused, we just want low pass (click)
        svf_process_bp_lp(&d->click_filter, d->click_lp - d->click_hp, &click_bp, &click);
        d->noise_lp = onepole_lp(d->noise_lp, random_get_float(rng) * noise_gain, noise_lp_f); // attack noise
        d->noise_hp = onepole_lp(d->noise_hp, d->noise_lp, noise_hp_f);
        float transient = click + d->noise_lp - d->noise_hp;
        float gain = d->body_env_lp;
        float tr_vca = (body - 0.6f) * gain; // transistor_vca
        float body_out = 3.0f * tr_vca / (2.0f + fabsf(tr_vca)) + gain * 0.3f;
        float mix = -body_out - transient * d->transient_env_lp * transient_level;
        d->tone_lp = onepole_lp(d->tone_lp, mix, tone_f);
        *out++ = d->tone_lp;
    }
    d->f0 = PD_BIGORSMALL(f0_cur) ? 0.0f : f0_cur;
}

static inline void drum_render(t_bd* x, int ch, int trigger, t_sample* out, size_t size){
    voice* v = &x->x_channel_voice[ch];
    int rising_edge = trigger && !v->trigger_state;
    v->trigger_state = trigger;
    if(rising_edge)
        v->decay_env = 1.0f;
    float short_decay = (100.0f * kBlockSize) / x->x_sr * st2ratio(-96.0f * x->x_ptime);
    v->decay_env *= 1.0f - short_decay * 2.0f;
    float accent = 1.3f * x->x_level / (0.3f + fabsf(x->x_level));
    float env = (v->decay_env * v->decay_env) * 4.0f; // 4 octaves
    float mod_amt = x->x_pdepth;
    mod_amt *= fmaxf(fabsf(mod_amt) - 0.05f, 0.05f) * 1.05f;
    float f0 = clampf(x->x_freq*powf(2.0f, mod_amt*env), 0.0f, x->x_sr * 0.4f) / x->x_sr;
    if(!x->x_mode) // analog bass drum model TR-808 like
        analog_bd_render(x, v, rising_edge, accent, f0, out, size);
    else // synthetic bass drum model (inadvertedly tr-909ish)
        synth_bd_render(x, v, rising_edge, accent, f0, out, size);
    for(size_t i = 0; i < size; i++){
        float s = out[i];
        out[i] = PD_BIGORSMALL(s) ? 0.0f : clampf(s, -1.0f, 1.0f);
    }
}

static t_int* bd_perform(t_int* w){
    t_bd* x = (t_bd*)(w[1]);
    t_sample* in = (t_sample*)(w[2]);
    t_sample* out = (t_sample*)(w[3]);
    int n = x->x_n, size = x->x_nsize;
    for(int ch = 0; ch < x->x_nchans; ch++){
        for(int j = 0; j < x->x_block_count; j++){
            t_sample* trig = in + ch * n + size * j;
            t_sample* outb = out + ch * n + size * j;
            int trigger_at = -1;
            for(int i = 0; i < size; i++){
                if(trig[i] != 0){
                    x->x_level = fminf(fabsf(trig[i]), 1.0f);
                    if(ch == 0)
                        x->x_klvl = x->x_level;
                    trigger_at = i;
                    break;
                }
            }
            if(x->x_k_trig){
                x->x_level = x->x_klvl;
                if(trigger_at < 0)
                    trigger_at = 0;
                x->x_k_trig = 0;
            }
            if(trigger_at > 0){
                drum_render(x, ch, 0, outb, trigger_at);
                drum_render(x, ch, 1, outb + trigger_at, size - trigger_at);
            }
            else
                drum_render(x, ch, trigger_at == 0, outb, size);
        }
    }
    return(w+4);
}

static void bd_dsp(t_bd* x, t_signal** sp){
    x->x_sr = (float)sp[0]->s_sr;
    int n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    if(n != x->x_n){ // should not actually be less than 16!!!!????!?!?!?
        x->x_nsize = n < kBlockSize ? n : kBlockSize;
        x->x_block_count = n / x->x_nsize;
        x->x_n = n;
    }
    if(chs != x->x_nchans){
        x->x_channel_voice = (voice*)resizebytes(x->x_channel_voice,
            x->x_nchans * sizeof(voice), chs * sizeof(voice));
        for(int ch = x->x_nchans; ch < chs; ch++)
            drum_init(&x->x_channel_voice[ch], x->x_seed + (unsigned)ch * 2654435761u);
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(bd_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
}

static void bd_bang(t_bd* x){
    x->x_k_trig = 1;
}

static void bd_mode(t_bd* x, t_floatarg f){
    t_int new_mode = f != 0;
    if(new_mode != x->x_mode){
        x->x_mode = new_mode;
        for(int ch = 0; ch < x->x_nchans; ch++){
            voice* v = &x->x_channel_voice[ch];
            if(new_mode)
                memset(&v->synth_bd, 0, sizeof(synth_bd));
            else
                memset(&v->analog_bd, 0, sizeof(analog_bd));
        }
    }
}

static void bd_freq(t_bd* x, t_floatarg f){
    x->x_freq = f;  // input in Hz
}

static void bd_punch(t_bd* x, t_floatarg f){
    x->x_punch = clampf(f, 0.0f, 1.0f);
}

static void bd_tone(t_bd* x, t_floatarg f){
    x->x_tone = clampf(f, 0.0f, 1.0f);
}

static void bd_decay(t_bd* x, t_floatarg f){
    x->x_decay = clampf(f, 0.0f, 1.0f);
}

static void bd_level(t_bd* x, t_floatarg f){
    x->x_klvl = clampf(f, 0.0f, 1.0f);
}

static void bd_ptime(t_bd* x, t_floatarg f){
    x->x_ptime = clampf(f, 0.0f, 1.0f);
}

static void bd_pdepth(t_bd* x, t_floatarg f){
    x->x_pdepth = clampf(f, 0.0f, 1.0f);
}

static void bd_free(t_bd* x){
    if(x->x_channel_voice)
        freebytes(x->x_channel_voice, x->x_nchans * sizeof(voice));
}

static void* bd_new(t_symbol* s, int ac, t_atom* av){
    (void)s;
    t_bd* x = (t_bd*)pd_new(bd_class);
    x->x_sr = sys_getsr();
    x->x_mode = x->x_k_trig = x->x_n = 0;
    float pitch = 50, punch = 0.5f, tone = 0.5f, decay = 0.5f, ptime = 0, pdepth = 0, lvl = 0.5f;
    int nfloats = 0;
    while(ac){
        if(av->a_type == A_SYMBOL){
            if(nfloats)
                goto errstate;
            t_symbol* sym = atom_getsymbol(av);
            ac--, av++;
            if(sym == gensym("-mode") && ac && av->a_type == A_FLOAT){
                x->x_mode = atom_getfloat(av) != 0;
                ac--, av++;
            }
            else
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
    x->x_nchans = 1;
    bd_level(x, lvl);
    bd_freq(x, pitch);
    bd_punch(x, punch);
    bd_tone(x, tone);
    bd_decay(x, decay);
    bd_ptime(x, ptime);
    bd_pdepth(x, pdepth);
    x->x_seed = hash_seed((unsigned)time(NULL) ^ (unsigned)(uintptr_t)x);
    x->x_channel_voice = (voice*)getbytes(sizeof(voice));
    drum_init(&x->x_channel_voice[0], x->x_seed);
    outlet_new(&x->x_obj, &s_signal);
    return(void*)x;
errstate:
    pd_error(x, "[bassdrum~]: improper args");
    return(NULL);
}

static void bd_class_setup(const char* name){
    init_sine_table();
    bd_class = class_new(gensym(name), (t_newmethod)bd_new, (t_method)bd_free,
        sizeof(t_bd), CLASS_MULTICHANNEL, A_GIMME, 0);
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
