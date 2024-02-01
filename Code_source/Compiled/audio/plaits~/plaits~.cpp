// plaits ported to Pd, by Porres 2023
// MIT Liscense

#include <stdint.h>
#include "plaits/dsp/dsp.h"
#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/voice.h"

#include "m_pd.h"
#include "g_canvas.h"

static t_class *plaits_class;

typedef struct _plaits{
    t_object            x_obj;
    t_glist            *x_glist;
    t_float             x_f;
    t_int               x_n;
    t_int               x_model;
//    t_float             pitch;
    t_float             pitch_correction;
    t_float             harmonics;
    t_float             timbre;
    t_float             morph;
    t_float             lpg_cutoff;
    t_int               pitch_mode;
    t_float             decay;
    t_float             mod_timbre;
    t_float             mod_fm;
    t_float             mod_morph;
    bool                frequency_active;
    bool                timbre_active;
    bool                morph_active;
    bool                trigger_mode;
    bool                level_active;
    t_int               block_size;
    t_int               block_count;
    t_int               last_n;
    t_float             last_tigger;
    t_int               last_engine;
    t_int               last_engine_perform;
    plaits::Voice       voice;
    plaits::Patch       patch;
    plaits::Modulations modulations;
    char                x_shared_buffer[16384];
    t_inlet            *x_trig_in;
    t_inlet            *x_level_in;
    t_outlet           *x_out1;
    t_outlet           *x_out2;
    t_outlet           *x_info_out;
}t_plaits;

extern "C"{ // Pd methods (cause we're using C++)
    t_int  *plaits_perform(t_int *w);
    void    plaits_dsp(t_plaits *x, t_signal **sp);
    void    plaits_free(t_plaits *x);
    void   *plaits_new(t_symbol *s, int ac, t_atom *av);
    void    plaits_tilde_setup(void);
    void    plaits_model(t_plaits *x, t_floatarg f);
    void    plaits_harmonics(t_plaits *x, t_floatarg f);
    void    plaits_timbre(t_plaits *x, t_floatarg f);
    void    plaits_morph(t_plaits *x, t_floatarg f);
    void    plaits_lpg_cutoff(t_plaits *x, t_floatarg f);
    void    plaits_decay(t_plaits *x, t_floatarg f);
    void    plaits_midi(t_plaits *x);
    void    plaits_hz(t_plaits *x);
    void    plaits_cv(t_plaits *x);
    void    plaits_voct(t_plaits *x);
    void    plaits_dump(t_plaits *x);
    void    plaits_print(t_plaits *x);
    void    plaits_trigger_mode(t_plaits *x, t_floatarg f);
    void    plaits_level_active(t_plaits *x, t_floatarg f);
    void    plaits_list(t_plaits *x, t_symbol *s, int ac, t_atom *av);
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
    post("- harmonics: %f", x->harmonics);
    post("- timbre: %f", x->timbre);
    post("- morph: %f", x->morph);
    post("- trigger mode: %d", x->trigger_mode);
    post("- cutoff: %f", x->lpg_cutoff);
    post("- decay: %f", x->decay);
    post("- level active: %d", x->level_active);
}

void plaits_dump(t_plaits *x){
    t_atom at[1];
    SETSYMBOL(at, gensym(modelLabels[x->x_model]));
    outlet_anything(x->x_info_out, gensym("name"), 1, at);
    SETFLOAT(at, x->harmonics);
    outlet_anything(x->x_info_out, gensym("harmonics"), 1, at);
    SETFLOAT(at, x->timbre);
    outlet_anything(x->x_info_out, gensym("timbre"), 1, at);
    SETFLOAT(at, x->morph);
    outlet_anything(x->x_info_out, gensym("morph"), 1, at);
    SETFLOAT(at, x->trigger_mode);
    outlet_anything(x->x_info_out, gensym("trigger mode"), 1, at);
    SETFLOAT(at, x->level_active);
    outlet_anything(x->x_info_out, gensym("level active"), 1, at);
    SETFLOAT(at, x->lpg_cutoff);
    outlet_anything(x->x_info_out, gensym("cutoff"), 1, at);
    SETFLOAT(at, x->decay);
    outlet_anything(x->x_info_out, gensym("decay"), 1, at);
}

void plaits_list(t_plaits *x, t_symbol *s, int argc, t_atom *argv){
    s = NULL;
    if(argc == 0)
        return;
    if(argc != 2)
        obj_list(&x->x_obj, NULL, argc, argv);
    else{
        t_atom at[3];
        SETFLOAT(at, atom_getfloat(argv));
        SETFLOAT(at+1, atom_getfloat(argv+1));
        SETFLOAT(at+2, atom_getfloat(argv+1));
        obj_list(&x->x_obj, NULL, 3, at);
    }
}

void plaits_model(t_plaits *x, t_floatarg f){
    x->x_model = f < 0 ? 0 : f > 23 ? 23 : (int)f;
    t_atom at[1];
    SETSYMBOL(at, gensym(modelLabels[x->x_model]));
    outlet_anything(x->x_info_out, gensym("name"), 1, at);
}

void plaits_harmonics(t_plaits *x, t_floatarg f){
    x->harmonics = f < 0 ? 0 : f > 1 ? 1 : f;
}

void plaits_timbre(t_plaits *x, t_floatarg f){
    x->timbre = f < 0 ? 0 : f > 1 ? 1 : f;
}

void plaits_morph(t_plaits *x, t_floatarg f){
    x->morph = f < 0 ? 0 : f > 1 ? 1 : f;
}

void plaits_lpg_cutoff(t_plaits *x, t_floatarg f){
    x->lpg_cutoff = f < 0 ? 0 : f > 1 ? 1 : f;
}

void plaits_decay(t_plaits *x, t_floatarg f){
    x->decay = f < 0 ? 0 : f > 1 ? 1 : f;
}

void plaits_hz(t_plaits *x){
    x->pitch_mode = 0;
}

void plaits_midi(t_plaits *x){
    x->pitch_mode = 1;
}

void plaits_cv(t_plaits *x){
    x->pitch_mode = 2;
}

void plaits_trigger_mode(t_plaits *x, t_floatarg f){
    x->trigger_mode = (int)(f != 0);
}

void plaits_level_active(t_plaits *x, t_floatarg f){
    x->level_active = (int)(f != 0);
}

static float plaits_get_pitch(t_plaits *x, t_floatarg f){
    if(x->pitch_mode == 0){
        f = log2f((f < 0 ? f * -1 : f)/440) + 0.75;
        return(f);
    }
    else if(x->pitch_mode == 1)
        return((f - 60) / 12);
    else
        return(f*5);
}

t_int *plaits_perform(t_int *w){
    t_plaits *x     = (t_plaits *) (w[1]);
    t_sample *freq  = (t_sample *) (w[2]);
    t_sample *trig  = (t_sample *) (w[3]);
    t_sample *level = (t_sample *) (w[4]);
    t_sample *out   = (t_sample *) (w[5]);
    t_sample *aux   = (t_sample *) (w[6]);
    int n = x->x_n; // block size
    if(n != x->last_n){
        if(n > 24){ // Plaits uses a block size of 24 max
            int block_size = 24;
            while(n > 24 && n % block_size > 0)
                block_size--;
            x->block_size = block_size;
            x->block_count = n / block_size;
        }
        else{
            x->block_size = n;
            x->block_count = 1;
        }
        x->last_n = n;
    }
    x->patch.engine = x->x_model; // Model
    int active_engine = x->voice.active_engine(); // Send current engine
    if(x->last_engine_perform > 128 && x->last_engine != active_engine){
        x->last_engine = active_engine;
        x->last_engine_perform = 0;
    }
    else
        x->last_engine_perform++;
    x->patch.harmonics = x->harmonics;
    x->patch.timbre = x->timbre;
    x->patch.morph = x->morph;
    x->patch.lpg_colour = x->lpg_cutoff;
    x->patch.decay = x->decay;
    x->modulations.trigger_patched = x->trigger_mode;
    x->patch.frequency_modulation_amount = x->mod_fm;
    x->patch.timbre_modulation_amount = x->mod_timbre;
    x->patch.morph_modulation_amount = x->mod_morph;
    x->modulations.frequency_patched = x->frequency_active;
    x->modulations.timbre_patched = x->timbre_active;
    x->modulations.morph_patched = x->morph_active;
    x->modulations.level_patched = x->level_active;
    for(int j = 0; j < x->block_count; j++){ // Render frames
        float pitch = plaits_get_pitch(x, freq[x->block_size * j]); // get pitch
        pitch += x->pitch_correction;
        x->patch.note = 60.f + pitch * 12.f;
        x->modulations.level = level[x->block_size * j];
        if(x->trigger_mode) // trigger mode
            x->modulations.trigger = (trig[x->block_size * j] != 0);
        plaits::Voice::Frame output[x->block_size];
        x->voice.Render(x->patch, x->modulations, output, x->block_size);
        for(int i = 0; i < x->block_size; i++){
            out[i + (x->block_size * j)] = output[i].out / 32768.0f;
            aux[i + (x->block_size * j)] = output[i].aux / 32768.0f;
        }
    }
    return(w+7);
}

void plaits_dsp(t_plaits *x, t_signal **sp){
    x->pitch_correction = log2f(48000.f / sys_getsr());
    x->x_n = sp[0]->s_n;
    dsp_add(plaits_perform, 6, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
}

void plaits_free(t_plaits *x){
    x->voice.FreeEngines();
    inlet_free(x->x_trig_in);
    inlet_free(x->x_level_in);
    outlet_free(x->x_out1);
    outlet_free(x->x_out2);
    outlet_free(x->x_info_out);
}

void *plaits_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_plaits *x = (t_plaits *)pd_new(plaits_class);
    stmlib::BufferAllocator allocator(x->x_shared_buffer, sizeof(x->x_shared_buffer));
    x->voice.Init(&allocator);
    x->x_glist = (t_glist *)canvas_getcurrent();
    int floatarg = 0;
    x->patch.engine = 0;
    x->patch.lpg_colour = 0.5f;
    x->patch.decay = 0.5f;
    x->x_model = 0;
//    x->pitch = 0;
    x->pitch_correction = log2f(48000.f / sys_getsr());
    x->harmonics = 0.5f;
    x->timbre = 0.5f;
    x->morph = 0.5f;
    x->lpg_cutoff = 0.5f;
    x->pitch_mode = 0;
    x->decay = 0.5f;
    x->mod_timbre = 0;
    x->mod_fm = 0;
    x->mod_morph = 0;
    x->frequency_active = false;
    x->timbre_active = false;
    x->morph_active = false;
    x->trigger_mode = false;
    x->level_active = false;
    x->last_engine = 0;
    x->last_engine_perform = 0;
    while(ac){
        if((av)->a_type == A_SYMBOL){
            if(floatarg)
                goto errstate;
            t_symbol *sym = atom_getsymbol(av);
            ac--, av++;
            if(sym == gensym("-midi"))
                x->pitch_mode = 1;
            else if(sym == gensym("-cv"))
                x->pitch_mode = 2;
            else if(sym == gensym("-model")){
                if((av)->a_type == A_FLOAT){
                    t_float m = atom_getfloat(av);
                    x->x_model = m < 0 ? 0 : m > 23 ? 23 : (int)m;
                    ac--, av++;
                }
            }
            else if(sym == gensym("-trigger"))
                x->trigger_mode = 1;
            else if(sym == gensym("-level"))
                x->level_active = 1;
            else
                goto errstate;
        }
        else{
            floatarg = 1;
            x->x_f = atom_getfloat(av); // pitch
            ac--, av++;
            if(ac && (av)->a_type == A_FLOAT){ // harmonics
                x->harmonics = atom_getfloat(av);
                ac--, av++;
                if(ac && (av)->a_type == A_FLOAT){ // timbre
                    x->timbre = atom_getfloat(av);
                    ac--, av++;
                    if(ac && (av)->a_type == A_FLOAT){ // morph
                        x->morph = atom_getfloat(av);
                        ac--, av++;
                        if(ac && (av)->a_type == A_FLOAT){ // cutoff
                            x->lpg_cutoff = atom_getfloat(av);
                            ac--, av++;
                            if(ac && (av)->a_type == A_FLOAT){ // decay
                                x->decay = atom_getfloat(av);
                                ac--, av++;
                            }
                        }
                    }
                }
            }
        }
    }
    x->x_trig_in = inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"), gensym ("signal"));
    x->x_level_in = inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"), gensym ("signal"));
    x->x_out1 = outlet_new(&x->x_obj, &s_signal);
    x->x_out2 = outlet_new(&x->x_obj, &s_signal);
    x->x_info_out = outlet_new(&x->x_obj, &s_symbol);
    return(void *)x;
errstate:
    pd_error(x, "[plaits~]: improper args");
    return(NULL);
}

void plaits_tilde_setup(void){
    plaits_class = class_new(gensym("plaits~"), (t_newmethod)plaits_new,
        (t_method)plaits_free, sizeof(t_plaits), 0, A_GIMME, 0);
    class_addmethod(plaits_class, (t_method)plaits_dsp, gensym("dsp"), A_NULL);
    CLASS_MAINSIGNALIN(plaits_class, t_plaits, x_f);
    class_addlist(plaits_class, plaits_list);
    class_addmethod(plaits_class, (t_method)plaits_model, gensym("model"), A_DEFFLOAT, A_NULL);
    class_addmethod(plaits_class, (t_method)plaits_harmonics, gensym("harmonics"), A_DEFFLOAT, A_NULL);
    class_addmethod(plaits_class, (t_method)plaits_timbre, gensym("timbre"), A_DEFFLOAT, A_NULL);
    class_addmethod(plaits_class, (t_method)plaits_morph, gensym("morph"), A_DEFFLOAT, A_NULL);
    class_addmethod(plaits_class, (t_method)plaits_trigger_mode, gensym("trigger"), A_DEFFLOAT, A_NULL);
    class_addmethod(plaits_class, (t_method)plaits_level_active, gensym("level"), A_DEFFLOAT, A_NULL);
    class_addmethod(plaits_class, (t_method)plaits_lpg_cutoff, gensym("cutoff"), A_DEFFLOAT, A_NULL);
    class_addmethod(plaits_class, (t_method)plaits_decay, gensym("decay"), A_DEFFLOAT, A_NULL);
    class_addmethod(plaits_class, (t_method)plaits_cv, gensym("cv"), A_NULL);
    class_addmethod(plaits_class, (t_method)plaits_midi, gensym("midi"), A_NULL);
    class_addmethod(plaits_class, (t_method)plaits_hz, gensym("hz"), A_NULL);
    class_addmethod(plaits_class, (t_method)plaits_dump, gensym("dump"), A_NULL);
    class_addmethod(plaits_class, (t_method)plaits_print, gensym("print"), A_NULL);
}
