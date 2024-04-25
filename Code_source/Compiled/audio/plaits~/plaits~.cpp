// based on the plaits engine by Mutable instruments
// also based on the pd port from github.com/jnonis/pd-plaits
// redesigned and rewritten by Porres 2023-2024
// Liscense: MIT Liscense (which is the original liscense of plaits)

#include <stdint.h>
#include "m_pd.h"
#include "plaits/dsp/dsp.h"
#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/voice.h"

static t_class *plaits_class;

typedef struct _plaits{
    t_object            x_obj;
    t_float             x_f;
    t_int               x_n;
    t_int               x_model;
    t_int               x_pitch_mode;
    t_float             x_pitch_correction;
    t_float             x_harmonics;
    t_float             x_timbre;
    t_float             x_morph;
    t_float             x_lpg_cutoff;
    t_float             x_decay;
    t_float             x_transp;
    t_float             x_mod_timbre;
    t_float             x_mod_fm;
    t_float             x_mod_morph;
    t_float             x_midi_pitch;
    t_float             x_midi_tr;
    t_float             x_midi_lvl;
    bool                x_frequency_active;
    bool                x_midi_mode;
    bool                x_timbre_active;
    bool                x_morph_active;    
    bool                x_trigger_mode;
    bool                x_level_active;
    t_int               x_block_size;
    t_int               x_block_count;
    t_int               x_last_n;
    t_int               x_last_engine;
    t_int               x_last_engine_perform;
    plaits::Voice       x_voice;
    plaits::Patch       x_patch;
    plaits::Modulations x_modulations;
    char                x_shared_buffer[16384];
    t_outlet           *x_info_out;
}t_plaits;

extern "C"{
    t_int *plaits_perform(t_int *w);
    t_int *plaits_perform_midi(t_int *w);
    void  *plaits_new(t_symbol *s, int ac, t_atom *av);
    void   plaits_dsp(t_plaits *x, t_signal **sp);
    void   plaits_free(t_plaits *x);
    void   plaits_tilde_setup(void);
    void   plaits_model(t_plaits *x, t_floatarg f);
    void   plaits_harmonics(t_plaits *x, t_floatarg f);
    void   plaits_timbre(t_plaits *x, t_floatarg f);
    void   plaits_timbreatt(t_plaits *x, t_floatarg f);
    void   plaits_morph(t_plaits *x, t_floatarg f);
    void   plaits_morphatt(t_plaits *x, t_floatarg f);
    void   plaits_fmatt(t_plaits *x, t_floatarg f);
    void   plaits_lpg_cutoff(t_plaits *x, t_floatarg f);
    void   plaits_decay(t_plaits *x, t_floatarg f);
    void   plaits_transp(t_plaits *x, t_floatarg f);
    void   plaits_midi(t_plaits *x);
    void   plaits_hz(t_plaits *x);
    void   plaits_cv(t_plaits *x);
    void   plaits_voct(t_plaits *x);
    void   plaits_dump(t_plaits *x);
    void   plaits_print(t_plaits *x);
    void   plaits_trigger_mode(t_plaits *x, t_floatarg f);
    void   plaits_level_active(t_plaits *x, t_floatarg f);
    void   plaits_morph_active(t_plaits *x, t_floatarg f);
    void   plaits_freq_active(t_plaits *x, t_floatarg f);
    void   plaits_timbre_active(t_plaits *x, t_floatarg f);
    void   plaits_midi_active(t_plaits *x, t_floatarg f);
    void   plaits_list(t_plaits *x, t_symbol *s, int ac, t_atom *av);
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

void plaits_print(t_plaits *x){
    post("[plaits~] settings:");
    post("- name: %s", modelLabels[x->x_model]);
    post("- harmonics: %f", x->x_harmonics);
    post("- timbre: %f", x->x_timbre);
    post("- morph: %f", x->x_morph);
    post("- trigger mode: %d", x->x_trigger_mode);
    post("- cutoff: %f", x->x_lpg_cutoff);
    post("- decay: %f", x->x_decay);
    post("- level active: %d", x->x_level_active);
    post("- morph active: %d", x->x_morph_active);
    post("- freq active: %d", x->x_frequency_active);
    post("- timbre active: %d", x->x_timbre_active);
    post("- midi active: %d", x->x_midi_mode);
}

void plaits_dump(t_plaits *x){
    t_atom at[1];
    SETSYMBOL(at, gensym(modelLabels[x->x_model]));
    outlet_anything(x->x_info_out, gensym("name"), 1, at);
    SETFLOAT(at, x->x_harmonics);
    outlet_anything(x->x_info_out, gensym("harmonics"), 1, at);
    SETFLOAT(at, x->x_timbre);
    outlet_anything(x->x_info_out, gensym("timbre"), 1, at);
    SETFLOAT(at, x->x_morph);
    outlet_anything(x->x_info_out, gensym("morph"), 1, at);
    SETFLOAT(at, x->x_lpg_cutoff);
    outlet_anything(x->x_info_out, gensym("cutoff"), 1, at);
    SETFLOAT(at, x->x_decay);
    outlet_anything(x->x_info_out, gensym("decay"), 1, at);
    SETFLOAT(at, x->x_trigger_mode);
    outlet_anything(x->x_info_out, gensym("trigger mode"), 1, at);
    SETFLOAT(at, x->x_level_active);
    outlet_anything(x->x_info_out, gensym("level active"), 1, at);
    SETFLOAT(at, x->x_morph_active);
    outlet_anything(x->x_info_out, gensym("morph active"), 1, at);
    SETFLOAT(at, x->x_frequency_active);
    outlet_anything(x->x_info_out, gensym("freq active"), 1, at);
    SETFLOAT(at, x->x_timbre_active);
    outlet_anything(x->x_info_out, gensym("timbre active"), 1, at);
    SETFLOAT(at, x->x_midi_mode);
    outlet_anything(x->x_info_out, gensym("midi active"), 1, at);
}

void plaits_list(t_plaits *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(ac == 0)
        return;
    if(ac == 1)
        obj_list(&x->x_obj, NULL, ac, av);
    else if(ac == 2){
        t_atom at[3];
        SETFLOAT(at, atom_getfloat(av));
        SETFLOAT(at+1, atom_getfloat(av+1) / 127.);
        SETFLOAT(at+2, atom_getfloat(av+1) / 127.);
        obj_list(&x->x_obj, NULL, 3, at);
    }
    else if(ac == 3){
        t_atom at[3];
        SETFLOAT(at, atom_getfloat(av));
        SETFLOAT(at+1, atom_getfloat(av+1) / 127.);
        SETFLOAT(at+2, atom_getfloat(av+2) / 127.);
        obj_list(&x->x_obj, NULL, 3, at);
    }
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

void plaits_model(t_plaits *x, t_floatarg f){
    x->x_model = f < 0 ? 0 : f > 23 ? 23 : (int)f;
    t_atom at[1];
    SETSYMBOL(at, gensym(modelLabels[x->x_model]));
    outlet_anything(x->x_info_out, gensym("name"), 1, at);
}

void plaits_harmonics(t_plaits *x, t_floatarg f){
    x->x_harmonics = f < 0 ? 0 : f > 1 ? 1 : f;
}

void plaits_timbre(t_plaits *x, t_floatarg f){
    x->x_timbre = f < 0 ? 0 : f > 1 ? 1 : f;
}

void plaits_timbreatt(t_plaits *x, t_floatarg f){
    x->x_mod_timbre = f < -1 ? -1 : f > 1 ? 1 : f;
}

void plaits_morph(t_plaits *x, t_floatarg f){
    x->x_morph = f < 0 ? 0 : f > 1 ? 1 : f;
}

void plaits_morphatt(t_plaits *x, t_floatarg f){
    x->x_mod_morph = f < -1 ? -1 : f > 1 ? 1 : f;
}

void plaits_fmatt(t_plaits *x, t_floatarg f){
    x->x_mod_fm = f < -1 ? -1 : f > 1 ? 1 : f;
}

void plaits_lpg_cutoff(t_plaits *x, t_floatarg f){
    x->x_lpg_cutoff = f < 0 ? 0 : f > 1 ? 1 : f;
}

void plaits_decay(t_plaits *x, t_floatarg f){
    x->x_decay = f < 0 ? 0 : f > 1 ? 1 : f;
}

void plaits_transp(t_plaits *x, t_floatarg f){
    x->x_transp = 60 + f;
}

void plaits_hz(t_plaits *x){
    x->x_pitch_mode = 0;
}

void plaits_midi(t_plaits *x){
    x->x_pitch_mode = 1;
}

void plaits_cv(t_plaits *x){
    x->x_pitch_mode = 2;
}

void plaits_trigger_mode(t_plaits *x, t_floatarg f){
    x->x_trigger_mode = (int)(f != 0);
}

void plaits_level_active(t_plaits *x, t_floatarg f){
    x->x_level_active = (int)(f != 0);
}

void plaits_morph_active(t_plaits *x, t_floatarg f){
    x->x_morph_active = (int)(f != 0);
}

void plaits_freq_active(t_plaits *x, t_floatarg f){
    x->x_frequency_active = (int)(f != 0);
}

void plaits_timbre_active(t_plaits *x, t_floatarg f){
    x->x_timbre_active = (int)(f != 0);
}

void plaits_midi_active(t_plaits *x, t_floatarg f){
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

t_int *plaits_perform(t_int *w){
    t_plaits *x     = (t_plaits *) (w[1]);
    t_sample *freq  = (t_sample *) (w[2]);  // frequency input
    t_sample *trig  = (t_sample *) (w[3]);  // trigger input
    t_sample *level = (t_sample *) (w[4]);  // level input
    t_sample *fmod  = (t_sample *) (w[5]);  // frequency modulation input
    t_sample *tmod  = (t_sample *) (w[6]);  // timbre modulation input
    t_sample *hmod  = (t_sample *) (w[7]);  // harmonics modulation input
    t_sample *mmod  = (t_sample *) (w[8]);  // morph modulation input
    t_sample *out   = (t_sample *) (w[9]);  // out
    t_sample *aux   = (t_sample *) (w[10]); // aux out
    int n = x->x_n; // block size
    if(n != x->x_last_n){
        if(n > 24){ // Plaits uses a block size of 24 max
            int block_size = 24;
            while(n > 24 && n % block_size > 0)
                block_size--;
            x->x_block_size = block_size;
            x->x_block_count = n / block_size;
        }
        else{
            x->x_block_size = n;
            x->x_block_count = 1;
        }
        x->x_last_n = n;
    }
    x->x_patch.engine = x->x_model; // Model
    int active_engine = x->x_voice.active_engine(); // Send current engine
    if(x->x_last_engine_perform > 128 && x->x_last_engine != active_engine){
        x->x_last_engine = active_engine;
        x->x_last_engine_perform = 0;
    }
    else
        x->x_last_engine_perform++;
    x->x_patch.harmonics = x->x_harmonics;
    x->x_patch.timbre = x->x_timbre;
    x->x_patch.morph = x->x_morph;
    x->x_patch.lpg_colour = x->x_lpg_cutoff;
    x->x_patch.decay = x->x_decay;
    x->x_patch.timbre_modulation_amount = x->x_mod_timbre;
    x->x_patch.frequency_modulation_amount = x->x_mod_fm;
    x->x_patch.morph_modulation_amount = x->x_mod_morph;
    x->x_modulations.trigger_patched = x->x_trigger_mode;
    x->x_modulations.frequency_patched = x->x_frequency_active;
    x->x_modulations.timbre_patched = x->x_timbre_active;
    x->x_modulations.morph_patched = x->x_morph_active;
    x->x_modulations.level_patched = x->x_level_active;
    for(int j = 0; j < x->x_block_count; j++){
        float pitch;
        if(x->x_midi_mode){
            pitch = plaits_get_pitch(x, x->x_midi_pitch);
            if(x->x_trigger_mode) // trigger mode
                x->x_modulations.trigger = x->x_midi_tr;
            x->x_modulations.level = x->x_midi_lvl;
        }
        else{
            pitch = plaits_get_pitch(x, freq[x->x_block_size * j]);
            if(x->x_trigger_mode) // trigger mode
                x->x_modulations.trigger = (trig[x->x_block_size * j] != 0);
            x->x_modulations.level = level[x->x_block_size * j];
        }
        x->x_patch.note = x->x_transp + (pitch + x->x_pitch_correction) * 12.f;
        x->x_modulations.timbre = tmod[x->x_block_size * j] * 0.5;
        x->x_modulations.frequency = fmod[x->x_block_size * j] * 60.f;
        x->x_modulations.morph = mmod[x->x_block_size * j] * 0.5;
        x->x_modulations.harmonics = hmod[x->x_block_size * j] * 0.5;
        plaits::Voice::Frame output[x->x_block_size];
        x->x_voice.Render(x->x_patch, x->x_modulations, output, x->x_block_size);
        for(int i = 0; i < x->x_block_size; i++){
            out[i + (x->x_block_size * j)] = output[i].out / 32768.0f;
            aux[i + (x->x_block_size * j)] = output[i].aux / 32768.0f;
        }
    }
    return(w+11);
}

void plaits_dsp(t_plaits *x, t_signal **sp){
    x->x_pitch_correction = log2f(48000.f / sys_getsr());
    x->x_n = sp[0]->s_n;
    dsp_add(plaits_perform, 10, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec,
        sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec, sp[7]->s_vec, sp[8]->s_vec);
}

void plaits_free(t_plaits *x){
    x->x_voice.FreeEngines();
    outlet_free(x->x_info_out);
}

void *plaits_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_plaits *x = (t_plaits *)pd_new(plaits_class);
    stmlib::BufferAllocator allocator(x->x_shared_buffer, sizeof(x->x_shared_buffer));
    x->x_voice.Init(&allocator);
    int floatarg = 0;
    x->x_model = x->x_pitch_mode = x->x_midi_mode = 0;
    x->x_pitch_correction = log2f(48000.f / sys_getsr());
    x->x_harmonics = x->x_timbre = x->x_morph = x->x_lpg_cutoff = x->x_decay = 0.5f;
    x->x_mod_timbre = x->x_mod_fm = x->x_mod_morph = 0;
    x->x_frequency_active = x->x_timbre_active = false;
    x->x_morph_active = x->x_trigger_mode = x->x_level_active = false;
    x->x_last_engine = x->x_last_engine_perform = 0;
    x->x_transp = 60.0;
    x->x_last_n = 0;
    while(ac){
        if((av)->a_type == A_SYMBOL){
            if(floatarg)
                goto errstate;
            t_symbol *sym = atom_getsymbol(av);
            ac--, av++;
            if(sym == gensym("-midi"))
                x->x_pitch_mode = 1;
            else if(sym == gensym("-cv"))
                x->x_pitch_mode = 2;
            else if(sym == gensym("-model")){
                if((av)->a_type == A_FLOAT){
                    t_float m = atom_getint(av);
                    x->x_model = m < 0 ? 0 : m > 23 ? 23 : m;
                    ac--, av++;
                }
            }
            else if(sym == gensym("-tr_active"))
                x->x_trigger_mode = 1;
            else if(sym == gensym("-lvl_active"))
                x->x_level_active = 1;
            else if(sym == gensym("-timbre_active"))
                x->x_timbre_active = 1;
            else if(sym == gensym("-freq_active"))
                x->x_frequency_active = 1;
            else if(sym == gensym("-morph_active"))
                x->x_morph_active = 1;
            else if(sym == gensym("-midi_active"))
                x->x_midi_mode = 1;
            else
                goto errstate;
        }
        else{
            floatarg = 1;
            x->x_f = atom_getfloat(av); // pitch
            ac--, av++;
            if(ac && (av)->a_type == A_FLOAT){ // harmonics
                x->x_harmonics = atom_getfloat(av);
                ac--, av++;
                if(ac && (av)->a_type == A_FLOAT){ // timbre
                    x->x_timbre = atom_getfloat(av);
                    ac--, av++;
                    if(ac && (av)->a_type == A_FLOAT){ // morph
                        x->x_morph = atom_getfloat(av);
                        ac--, av++;
                        if(ac && (av)->a_type == A_FLOAT){ // cutoff
                            x->x_lpg_cutoff = atom_getfloat(av);
                            ac--, av++;
                            if(ac && (av)->a_type == A_FLOAT){ // decay
                                x->x_decay = atom_getfloat(av);
                                ac--, av++;
                                if(ac && (av)->a_type == A_FLOAT){ // timbre att
                                    x->x_mod_timbre = atom_getfloat(av);
                                    ac--, av++;
                                    if(ac && (av)->a_type == A_FLOAT){ // freq att
                                        x->x_mod_fm = atom_getfloat(av);
                                        ac--, av++;
                                        if(ac && (av)->a_type == A_FLOAT){ // morph att
                                            x->x_mod_morph = atom_getfloat(av);
                                            ac--, av++;
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
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
        (t_method)plaits_free, sizeof(t_plaits), 0, A_GIMME, 0);
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
