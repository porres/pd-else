// based on the plaits engine by Mutable instruments
// also based on the pd port from github.com/jnonis/pd-plaits
// redesigned and rewritten by Porres 2023-2026
// Liscense: MIT Liscense (which is the original liscense of plaits)

#include <stdint.h>

#include "plaits/dsp/dsp.h"
#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/voice.h"

#include <m_pd.h>

#define SHARED_BUFFER_SIZE 16384

static t_class *plaits_class;

typedef struct _plaits{
    t_object            x_obj;
    t_float             x_f;
    t_int               x_n;
    t_int               x_nchans;
    t_int               x_model;
    t_int               x_pitch_mode;
    t_float             x_transp;
    t_float             x_midi_pitch;
    t_float             x_midi_tr;
    t_float             x_midi_lvl;
    bool                x_midi_mode;
    t_int               x_block_size;
    t_int               x_block_count;
    t_int               x_ch_trig;
    t_int               x_ch_level;
    t_int               x_ch_fmod;
    t_int               x_ch_tmod;
    t_int               x_ch_hmod;
    t_int               x_ch_mmod;
    t_int               x_ch_freq;
    t_int               x_nalloc;      // number of voices allocated (never shrinks)
    plaits::Voice       **x_voice;     // table of pointers: a Voice never moves
    char                **x_buffer;    // one shared RAM slice per voice
    plaits::Patch       x_patch;
    plaits::Modulations x_mod;
    t_outlet           *x_info_out;
}t_plaits;

extern "C" void plaits_tilde_setup(void);

static inline float flush(float s){
    return PD_BIGORSMALL(s) ? 0.0f : s;
}

static const char* modelLabels[24] = {
    "Pair of classic waveforms",
    "Waveshaping oscillator",
    "Two operators FM",
    "Granular formant oscillator",
    "Harmonic oscillator",
    "Wavetable oscillator",
    "Chords",
    "Vowel and speech synthesis",
    "Granular cloud",
    "Filtered noise",
    "Particle noise",
    "Inharmonic string modeling",
    "Modal resonator",
    "Analog bass drum",
    "Analog snare drum",
    "Analog hi-hat",
    "Virtual Analog VCF",
    "Phase Distortion",
    "Six operators FM #1",
    "Six operators FM #2",
    "Six operators FM #3",
    "Wave Terrain",
    "String Machine",
    "Chiptune"
};

static void plaits_print(t_plaits *x){
    post("[plaits~] settings:");
    post("- name: %s", modelLabels[x->x_model]);
    post("- harmonics: %f", x->x_patch.harmonics);
    post("- timbre: %f", x->x_patch.timbre);
    post("- morph: %f", x->x_patch.morph);
    post("- trigger mode: %d", x->x_mod.trigger_patched);
    post("- cutoff: %f", x->x_patch.lpg_colour);
    post("- decay: %f", x->x_patch.decay);
    post("- level active: %d", x->x_mod.level_patched);
    post("- morph active: %d", x->x_mod.morph_patched);
    post("- freq active: %d", x->x_mod.frequency_patched);
    post("- timbre active: %d", x->x_mod.timbre_patched);
    post("- midi active: %d", x->x_midi_mode);
}

static void plaits_dump(t_plaits *x){
    t_atom at[1];
    SETSYMBOL(at, gensym(modelLabels[x->x_model]));
    outlet_anything(x->x_info_out, gensym("name"), 1, at);
    SETFLOAT(at, x->x_patch.harmonics);
    outlet_anything(x->x_info_out, gensym("harmonics"), 1, at);
    SETFLOAT(at, x->x_patch.timbre);
    outlet_anything(x->x_info_out, gensym("timbre"), 1, at);
    SETFLOAT(at, x->x_patch.morph);
    outlet_anything(x->x_info_out, gensym("morph"), 1, at);
    SETFLOAT(at, x->x_patch.lpg_colour);
    outlet_anything(x->x_info_out, gensym("cutoff"), 1, at);
    SETFLOAT(at, x->x_patch.decay);
    outlet_anything(x->x_info_out, gensym("decay"), 1, at);
    SETFLOAT(at, x->x_mod.trigger_patched);
    outlet_anything(x->x_info_out, gensym("trigger mode"), 1, at);
    SETFLOAT(at, x->x_mod.level_patched);
    outlet_anything(x->x_info_out, gensym("level active"), 1, at);
    SETFLOAT(at, x->x_mod.morph_patched);
    outlet_anything(x->x_info_out, gensym("morph active"), 1, at);
    SETFLOAT(at, x->x_mod.frequency_patched);
    outlet_anything(x->x_info_out, gensym("freq active"), 1, at);
    SETFLOAT(at, x->x_mod.timbre_patched);
    outlet_anything(x->x_info_out, gensym("timbre active"), 1, at);
    SETFLOAT(at, x->x_midi_mode);
    outlet_anything(x->x_info_out, gensym("midi active"), 1, at);
}

static void plaits_list(t_plaits *x, t_symbol *s, int ac, t_atom *av){
    if(ac == 0)
        return;
    if(ac == 1 && s)
        obj_list(&x->x_obj, NULL, ac, av);
    else if(ac == 2 && s){
        t_atom at[3];
        SETFLOAT(at, atom_getfloat(av));
        SETFLOAT(at+1, atom_getfloat(av+1) / 127.);
        SETFLOAT(at+2, atom_getfloat(av+1) / 127.);
        obj_list(&x->x_obj, NULL, 3, at);
    }
    else if(ac == 3 && s){
        t_atom at[3];
        SETFLOAT(at, atom_getfloat(av));
        SETFLOAT(at+1, atom_getfloat(av+1) / 127.);
        SETFLOAT(at+2, atom_getfloat(av+2) / 127.);
        obj_list(&x->x_obj, NULL, 3, at);
    }
    s = NULL;
    x->x_midi_tr = x->x_midi_lvl = 0;
    x->x_midi_pitch = atom_getfloat(av);
    ac--, av++;
    if(ac){
        float vel = atom_getfloat(av);
        x->x_midi_tr = vel != 0;
        x->x_midi_lvl = vel / 127.;
        ac--, av++;
    }
    if(ac)
        x->x_midi_lvl = atom_getfloat(av) / 127.;
}

static void plaits_model(t_plaits *x, t_floatarg f){
    x->x_model = f < 0 ? 0 : f > 23 ? 23 : (int)f;
    t_atom at[1];
    SETSYMBOL(at, gensym(modelLabels[x->x_model]));
    outlet_anything(x->x_info_out, gensym("name"), 1, at);
}

static void plaits_harmonics(t_plaits *x, t_floatarg f){
    x->x_patch.harmonics = f < 0 ? 0 : f > 1 ? 1 : f;
}

static void plaits_timbre(t_plaits *x, t_floatarg f){
    x->x_patch.timbre = f < 0 ? 0 : f > 1 ? 1 : f;
}

static void plaits_timbreatt(t_plaits *x, t_floatarg f){
    x->x_patch.timbre_modulation_amount = f < -1 ? -1 : f > 1 ? 1 : f;
}

static void plaits_morph(t_plaits *x, t_floatarg f){
    x->x_patch.morph = f < 0 ? 0 : f > 1 ? 1 : f;
}

static void plaits_morphatt(t_plaits *x, t_floatarg f){
    x->x_patch.morph_modulation_amount = f < -1 ? -1 : f > 1 ? 1 : f;
}

static void plaits_fmatt(t_plaits *x, t_floatarg f){
    x->x_patch.frequency_modulation_amount = f < -1 ? -1 : f > 1 ? 1 : f;
}

static void plaits_lpg_cutoff(t_plaits *x, t_floatarg f){
    x->x_patch.lpg_colour = f < 0 ? 0 : f > 1 ? 1 : f;
}

static void plaits_decay(t_plaits *x, t_floatarg f){
    x->x_patch.decay = f < 0 ? 0 : f > 1 ? 1 : f;
}

static void plaits_transp(t_plaits *x, t_floatarg f){
    x->x_transp = 60 + f;
}

static void plaits_hz(t_plaits *x){
    x->x_pitch_mode = 0;
}

static void plaits_midi(t_plaits *x){
    x->x_pitch_mode = 1;
}

static void plaits_cv(t_plaits *x){
    x->x_pitch_mode = 2;
}

static void plaits_trigger_mode(t_plaits *x, t_floatarg f){
    x->x_mod.trigger_patched = (int)(f != 0);
}

static void plaits_level_active(t_plaits *x, t_floatarg f){
    x->x_mod.level_patched = (int)(f != 0);
}

static void plaits_morph_active(t_plaits *x, t_floatarg f){
    x->x_mod.morph_patched = (int)(f != 0);
}

static void plaits_freq_active(t_plaits *x, t_floatarg f){
    x->x_mod.frequency_patched = (int)(f != 0);
}

static void plaits_timbre_active(t_plaits *x, t_floatarg f){
    x->x_mod.timbre_patched = (int)(f != 0);
}

static void plaits_midi_active(t_plaits *x, t_floatarg f){
    x->x_midi_mode = (int)(f != 0);
}

static float plaits_get_pitch(t_plaits *x, t_floatarg f){
    if(x->x_pitch_mode == 0){
        f = log2f((f < 0 ? f * -1 : f)/440) + 0.75;
        return(f);
    }
    else if(x->x_pitch_mode == 1){
        f = f > 0 ? ((f - 60) / 12) : -1000;
        return(f);
    }
    else
        return(f*5);
}

// Voices are allocated once and individually, and are never moved or freed
// until the object dies. Only the table of pointers is resized.
static void plaits_grow(t_plaits *x, int chs){
    if(chs <= x->x_nalloc)
        return;
    plaits::Voice **v = (plaits::Voice **)getbytes(chs * sizeof(plaits::Voice *));
    char **b = (char **)getbytes(chs * sizeof(char *));
    for(int c = 0; c < x->x_nalloc; c++){
        v[c] = x->x_voice[c];
        b[c] = x->x_buffer[c];
    }
    for(int c = x->x_nalloc; c < chs; c++){
        v[c] = (plaits::Voice *)getbytes(sizeof(plaits::Voice));
        b[c] = (char *)getbytes(SHARED_BUFFER_SIZE);
        stmlib::BufferAllocator allocator(b[c], SHARED_BUFFER_SIZE);
        v[c]->Init(&allocator);
    }
    if(x->x_nalloc){
        freebytes(x->x_voice, x->x_nalloc * sizeof(plaits::Voice *));
        freebytes(x->x_buffer, x->x_nalloc * sizeof(char *));
    }
    x->x_voice = v;
    x->x_buffer = b;
    x->x_nalloc = chs;
}

static t_int *plaits_perform(t_int *w){
    t_plaits *x     = (t_plaits *) (w[1]);
    t_sample *freq  = (t_sample *) (w[2]);
    t_sample *trig  = (t_sample *) (w[3]);
    t_sample *level = (t_sample *) (w[4]);
    t_sample *fmod  = (t_sample *) (w[5]);
    t_sample *tmod  = (t_sample *) (w[6]);
    t_sample *hmod  = (t_sample *) (w[7]);
    t_sample *mmod  = (t_sample *) (w[8]);
    t_sample *out   = (t_sample *) (w[9]);
    t_sample *aux   = (t_sample *) (w[10]);
    int n = x->x_n;
    int nchans = x->x_nchans;
    x->x_patch.engine = x->x_model;
    plaits::Voice::Frame output[plaits::kMaxBlockSize];
    for(int c = 0; c < nchans; c++){
        t_sample *fc = x->x_ch_freq  == 1 ? freq  : freq  + c * n;
        t_sample *tc = x->x_ch_trig  == 1 ? trig  : trig  + c * n;
        t_sample *lc = x->x_ch_level == 1 ? level : level + c * n;
        t_sample *fmc= x->x_ch_fmod  == 1 ? fmod  : fmod  + c * n;
        t_sample *tmc= x->x_ch_tmod  == 1 ? tmod  : tmod  + c * n;
        t_sample *hmc= x->x_ch_hmod  == 1 ? hmod  : hmod  + c * n;
        t_sample *mmc= x->x_ch_mmod  == 1 ? mmod  : mmod  + c * n;
        t_sample *oc = out + c * n;
        t_sample *ac = aux + c * n;
        for(int j = 0; j < x->x_block_count; j++){
            int base = x->x_block_size * j;
            int trigger_at = -1;
            if(x->x_mod.trigger_patched && !x->x_midi_mode){
                for(int i = 0; i < x->x_block_size; i++){
                    if(tc[base + i] != 0){
                        trigger_at = i;
                        break;
                    }
                }
            }
            int sample_at = base + (trigger_at > 0 ? trigger_at : 0);
            if(x->x_midi_mode){
                x->x_patch.note = x->x_transp + plaits_get_pitch(x, x->x_midi_pitch) * 12.f;
                if(x->x_mod.trigger_patched)
                    x->x_mod.trigger = x->x_midi_tr;
                x->x_mod.level = x->x_midi_lvl;
            }
            else{
                x->x_patch.note = x->x_transp + plaits_get_pitch(x, fc[sample_at]) * 12.f;
                if(x->x_mod.trigger_patched)
                    x->x_mod.trigger = (trigger_at >= 0);
                x->x_mod.level = lc[sample_at];
            }
            x->x_mod.timbre = tmc[sample_at] * 0.5;
            x->x_mod.frequency = fmc[sample_at] * 60.f;
            x->x_mod.morph = mmc[sample_at] * 0.5;
            x->x_mod.harmonics = hmc[sample_at] * 0.5;
            if(trigger_at > 0){
                x->x_mod.trigger = 0;
                x->x_voice[c]->Render(x->x_patch, x->x_mod, output, trigger_at);
                for(int i = 0; i < trigger_at; i++){
                    oc[i + base] = flush(output[i].out);
                    ac[i + base] = flush(output[i].aux);
                }
                x->x_mod.trigger = 1;
                x->x_voice[c]->Render(x->x_patch, x->x_mod, output, x->x_block_size - trigger_at);
                for(int i = 0; i < x->x_block_size - trigger_at; i++){
                    oc[i + base + trigger_at] = flush(output[i].out);
                    ac[i + base + trigger_at] = flush(output[i].aux);
                }
            }
            else{
                x->x_voice[c]->Render(x->x_patch, x->x_mod, output, x->x_block_size);
                for(int i = 0; i < x->x_block_size; i++){
                    oc[i + base] = flush(output[i].out);
                    ac[i + base] = flush(output[i].aux);
                }
            }
        }
    }
    return(w+11);
}

static void plaits_dsp(t_plaits *x, t_signal **sp){
    plaits::kSampleRate = (float)sp[0]->s_sr;
    plaits::a0 = 55.f / plaits::kSampleRate;
    int n = sp[0]->s_n;
    int ch[7], chs = 1;
    for(int i = 0; i < 7; i++){
        ch[i] = sp[i]->s_nchans < 1 ? 1 : sp[i]->s_nchans;
        if(ch[i] > chs)
            chs = ch[i];
    }
    // every inlet is either mono (broadcast to all voices) or has 'chs' channels
    for(int i = 0; i < 7; i++){
        if(ch[i] > 1 && ch[i] != chs){
            signal_setmultiout(&sp[7], chs);
            signal_setmultiout(&sp[8], chs);
            dsp_add_zero(sp[7]->s_vec, chs * n);
            dsp_add_zero(sp[8]->s_vec, chs * n);
            pd_error(x, "[plaits~]: channel sizes mismatch");
            return;
        }
    }
    x->x_ch_freq  = ch[0];
    x->x_ch_trig  = ch[1];
    x->x_ch_level = ch[2];
    x->x_ch_fmod  = ch[3];
    x->x_ch_tmod  = ch[4];
    x->x_ch_hmod  = ch[5];
    x->x_ch_mmod  = ch[6];
    if(n != x->x_n){
        if(n >= 16){
            x->x_block_size = 16;
            x->x_block_count = n / x->x_block_size;
        }
        else{
            x->x_block_size = n;
            x->x_block_count = 1;
        }
        x->x_n = n;
    }
    plaits_grow(x, chs); // only ever grows, existing voices are untouched
    x->x_nchans = chs;
    signal_setmultiout(&sp[7], chs);
    signal_setmultiout(&sp[8], chs);
    dsp_add(plaits_perform, 10, x,
        sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec,
        sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec, sp[7]->s_vec, sp[8]->s_vec);
}

static void plaits_free(t_plaits *x){
    for(int c = 0; c < x->x_nalloc; c++){
        x->x_voice[c]->FreeEngines();
        freebytes(x->x_voice[c], sizeof(plaits::Voice));
        freebytes(x->x_buffer[c], SHARED_BUFFER_SIZE);
    }
    freebytes(x->x_voice, x->x_nalloc * sizeof(plaits::Voice *));
    freebytes(x->x_buffer, x->x_nalloc * sizeof(char *));
    outlet_free(x->x_info_out);
}

static void *plaits_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_plaits *x = (t_plaits *)pd_new(plaits_class);
    int nfloats = 0;
    x->x_model = 0;
    x->x_pitch_mode = 0;
    x->x_midi_mode = 0;
    x->x_patch.harmonics = 0.5;
    x->x_patch.timbre = 0.5;
    x->x_patch.morph = 0.5;
    x->x_patch.lpg_colour = 0.5;
    x->x_patch.decay = 0.5f;
    x->x_patch.frequency_modulation_amount = 0;
    x->x_patch.timbre_modulation_amount = 0;
    x->x_patch.morph_modulation_amount = 0;
    x->x_mod.frequency_patched = false;
    x->x_mod.timbre_patched = false;
    x->x_mod.morph_patched = false;
    x->x_mod.trigger_patched = false;
    x->x_mod.level_patched = false;
    x->x_transp = 60.0;
    x->x_n = 0;
    x->x_nchans = 1;
    x->x_ch_trig  = 1;
    x->x_ch_level = 1;
    x->x_ch_fmod  = 1;
    x->x_ch_tmod  = 1;
    x->x_ch_hmod  = 1;
    x->x_ch_mmod  = 1;
    x->x_ch_freq  = 1;
    x->x_nalloc = 0;
    x->x_voice = NULL;
    x->x_buffer = NULL;
    plaits_grow(x, 1);
    while(ac){
        if(av->a_type == A_SYMBOL){
            t_symbol *sym = atom_getsymbol(av);
            ac--, av++;
            if(sym == gensym("-midi"))
                x->x_pitch_mode = 1;
            else if(sym == gensym("-cv"))
                x->x_pitch_mode = 2;
            else if(sym == gensym("-model") && ac && av->a_type == A_FLOAT){
                t_float m = atom_getint(av);
                x->x_model = m < 0 ? 0 : m > 23 ? 23 : m;
                ac--, av++;
            }
            else if(sym == gensym("-tr_active"))
                x->x_mod.trigger_patched = true;
            else if(sym == gensym("-lvl_active"))
                x->x_mod.level_patched = 1;
            else if(sym == gensym("-timbre_active"))
                x->x_mod.timbre_patched = 1;
            else if(sym == gensym("-freq_active"))
                x->x_mod.frequency_patched = 1;
            else if(sym == gensym("-morph_active"))
                x->x_mod.morph_patched = 1;
            else if(sym == gensym("-midi_active"))
                x->x_midi_mode = 1;
            else
                goto errstate;
        }
        else{
            float f = atom_getfloat(av);
            ac--, av++;
            switch(nfloats++){
                case 0: x->x_f = f; break;
                case 1: x->x_patch.harmonics = f; break;
                case 2: x->x_patch.timbre = f; break;
                case 3: x->x_patch.morph = f; break;
                case 4: x->x_patch.lpg_colour = f; break;
                case 5: x->x_patch.decay = f; break;
                case 6: x->x_patch.timbre_modulation_amount = f; break;
                case 7: x->x_patch.frequency_modulation_amount = f; break;
                case 8: x->x_patch.morph_modulation_amount = f; break;
                default: goto errstate;
            }
        }
    }
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"), gensym ("signal"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"), gensym ("signal"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"), gensym ("signal"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"), gensym ("signal"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"), gensym ("signal"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"), gensym ("signal"));
    outlet_new(&x->x_obj, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    x->x_info_out = outlet_new(&x->x_obj, &s_symbol);
    return(void *)x;
errstate:
    pd_error(x, "[plaits~]: improper args");
    return(NULL);
}

void plaits_tilde_setup(void){
    plaits_class = class_new(gensym("plaits~"), (t_newmethod)plaits_new,
        (t_method)plaits_free, sizeof(t_plaits), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(plaits_class, (t_method)plaits_dsp, gensym("dsp"), A_CANT, 0);
    CLASS_MAINSIGNALIN(plaits_class, t_plaits, x_f);
    class_addlist(plaits_class, plaits_list);
    class_addmethod(plaits_class, (t_method)plaits_model, gensym("model"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_harmonics, gensym("harmonics"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_freq_active, gensym("freq_active"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_fmatt, gensym("freq_mod"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_timbre, gensym("timbre"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_timbre_active, gensym("timbre_active"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_timbreatt, gensym("timbre_mod"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_morph, gensym("morph"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_morph_active, gensym("morph_active"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_morphatt, gensym("morph_mod"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_trigger_mode, gensym("tr_active"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_level_active, gensym("lvl_active"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_midi_active, gensym("midi_active"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_lpg_cutoff, gensym("cutoff"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_decay, gensym("decay"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_transp, gensym("transp"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_cv, gensym("cv"), A_NULL);
    class_addmethod(plaits_class, (t_method)plaits_midi, gensym("midi"), A_NULL);
    class_addmethod(plaits_class, (t_method)plaits_hz, gensym("hz"), A_NULL);
    class_addmethod(plaits_class, (t_method)plaits_dump, gensym("dump"), A_NULL);
    class_addmethod(plaits_class, (t_method)plaits_print, gensym("print"), A_NULL);
}
