// plaits ported to Pd, by Porres 2023
// MIT Liscense

#include "m_pd.h"
#include "plaits/dsp/dsp.h"
#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/voice.h"

static t_class *plts_class;

typedef struct _plts{
    t_object x_obj;
    t_sample pitch_in;
    t_int model;
    t_float pitch;
    t_float pitch_correction;
    t_float harmonics;
    t_float timbre;
    t_float morph;
    t_float lpg_cutoff;
    t_int pitch_mode;
    t_float decay;
    bool trigger;
    t_float mod_timbre;
    t_float mod_fm;
    t_float mod_morph;
    bool frequency_active;
    bool timbre_active;
    bool morph_active;
    bool trigger_mode;
    bool level_active;
    t_int block_size;
    t_int block_count;
    t_int last_n;
    t_float last_tigger;
    t_int last_engine;
    t_int last_engine_perform;
    plaits::Voice voice;
    plaits::Patch patch;
    plaits::Modulations modulations;
    char shared_buffer[16384];
    t_outlet *x_out1;
    t_outlet *x_out2;
    t_outlet *x_out_model;
}t_plts;

extern "C"  { // Pure data methods, needed because we are using C++
    t_int *plts_perform(t_int *w);
    void plts_dsp(t_plts *x, t_signal **sp);
    void plts_free(t_plts *x);
    void *plts_new(t_symbol *s, int ac, t_atom *av);
    void plaits_tilde_setup(void);
    void plts_model(t_plts *x, t_floatarg f);
    void plts_harmonics(t_plts *x, t_floatarg f);
    void plts_timbre(t_plts *x, t_floatarg f);
    void plts_morph(t_plts *x, t_floatarg f);
    void plts_lpg_cutoff(t_plts *x, t_floatarg f);
    void plts_decay(t_plts *x, t_floatarg f);
    void plts_bang(t_plts *x);
    void plts_midi(t_plts *x);
    void plts_hz(t_plts *x);
    void plts_cv(t_plts *x);
    void plts_trigger_mode(t_plts *x, t_floatarg f);
}

static const char* modelLabels[16] = {
    "Pair of classic waveforms",
    "Waveshaping oscillator",
    "Two operator FM",
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
};

void plts_model(t_plts *x, t_floatarg f){
    x->model = f < 0 ? 0 : f > 15 ? 15 : (int)f;
    outlet_symbol(x->x_out_model, gensym(modelLabels[x->model]));
}

void plts_harmonics(t_plts *x, t_floatarg f){
    x->harmonics = f;
}

void plts_timbre(t_plts *x, t_floatarg f){
    x->timbre = f;
}

void plts_morph(t_plts *x, t_floatarg f){
    x->morph = f;
}

void plts_bang(t_plts *x){
    x->trigger = 1;
}

void plts_lpg_cutoff(t_plts *x, t_floatarg f){
    x->lpg_cutoff = f;
}

void plts_decay(t_plts *x, t_floatarg f){
    x->decay = f;
}

void plts_hz(t_plts *x){
    x->pitch_mode = 0;
}

void plts_midi(t_plts *x){
    x->pitch_mode = 1;
}

void plts_cv(t_plts *x){
    x->pitch_mode = 2;
}

void plts_trigger_mode(t_plts *x, t_floatarg f){
    x->trigger_mode = (int)(f != 0);
}

static float plts_get_pitch(t_plts *x, t_floatarg f){
    switch(x->pitch_mode){
        case 0: // hz
            return(log2f(f/440) + 0.75);
        break;
        case 1: // midi
            return((f - 60) / 12);
        break;
        case 2: // CV (V/oct)
            return(f);
        break;
    }
}

t_int *plts_perform(t_int *w){
    t_plts *x = (t_plts *) (w[1]);
    t_sample *pitch_inlet = (t_sample *) (w[2]);
    t_sample *out = (t_sample *) (w[3]);
    t_sample *aux = (t_sample *) (w[4]);
    int n = (int)(w[5]); // Determine block size
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
    x->patch.engine = x->model; // Model
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
        float pitch = plts_get_pitch(x, pitch_inlet[x->block_size * j]); // get pitch
        pitch += x->pitch_correction;
        x->patch.note = 60.f + pitch * 12.f;
        if(x->trigger){ // Message trigger
            x->modulations.trigger = 1.0f;
            x->trigger = false;
        }
        else
            x->modulations.trigger = 0.0f;
        plaits::Voice::Frame output[x->block_size];
        x->voice.Render(x->patch, x->modulations, output, x->block_size);
        for(int i = 0; i < x->block_size; i++){
            out[i + (x->block_size * j)] = output[i].out / 32768.0f;
            aux[i + (x->block_size * j)] = output[i].aux / 32768.0f;
        }
    }
    return(w+6);
}

void plts_dsp(t_plts *x, t_signal **sp){
    x->pitch_correction = log2f(48000.f / sys_getsr());
    dsp_add(plts_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
}

void plts_free(t_plts *x){
    x->voice.FreeEngines();
    outlet_free(x->x_out1);
    outlet_free(x->x_out2);
    outlet_free(x->x_out_model);
}

void *plts_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_plts *x = (t_plts *)pd_new(plts_class);
    stmlib::BufferAllocator allocator(x->shared_buffer, sizeof(x->shared_buffer));
    x->voice.Init(&allocator);
    int floatarg = 0;
    x->patch.engine = 0;
    x->patch.lpg_colour = 0.5f;
    x->patch.decay = 0.5f;
    x->model = 0;
    x->pitch = 0;
    x->pitch_correction = log2f(48000.f / sys_getsr());
    x->harmonics = 0;
    x->timbre = 0;
    x->morph = 0;
    x->lpg_cutoff = 0.5f;
    x->pitch_mode = 0;
    x->decay = 0.5f;
    x->trigger = false;
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
                    x->model = m < 0 ? 0 : m > 15 ? 15 : (int)m;
                    ac--, av++;
                }
            }
            else if(sym == gensym("-trigger"))
                x->trigger_mode = 1;
            else
                goto errstate;
        }
        else{
            floatarg = 1;
            x->pitch_in = atom_getfloat(av); // pitch
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
    x->x_out1 = outlet_new(&x->x_obj, &s_signal);
    x->x_out2 = outlet_new(&x->x_obj, &s_signal);
    x->x_out_model = outlet_new(&x->x_obj, &s_symbol);
    return(void *)x;
errstate:
    pd_error(x, "[plaits~]: improper args");
    return(NULL);
}

void plaits_tilde_setup(void){
    plts_class = class_new(gensym("plaits~"), (t_newmethod)plts_new,
        (t_method)plts_free, sizeof(t_plts), 0, A_GIMME, 0);
    class_addmethod(plts_class, (t_method)plts_dsp, gensym("dsp"), A_NULL);
    CLASS_MAINSIGNALIN(plts_class, t_plts, pitch_in);
    class_addbang(plts_class, plts_bang);
    class_addmethod(plts_class, (t_method)plts_model, gensym("model"), A_DEFFLOAT, A_NULL);
    class_addmethod(plts_class, (t_method)plts_harmonics, gensym("harmonics"), A_DEFFLOAT, A_NULL);
    class_addmethod(plts_class, (t_method)plts_timbre, gensym("timbre"), A_DEFFLOAT, A_NULL);
    class_addmethod(plts_class, (t_method)plts_morph, gensym("morph"), A_DEFFLOAT, A_NULL);
    class_addmethod(plts_class, (t_method)plts_trigger_mode, gensym("trigger"), A_DEFFLOAT, A_NULL);
    class_addmethod(plts_class, (t_method)plts_lpg_cutoff, gensym("cutoff"), A_DEFFLOAT, A_NULL);
    class_addmethod(plts_class, (t_method)plts_decay, gensym("decay"), A_DEFFLOAT, A_NULL);
    class_addmethod(plts_class, (t_method)plts_cv, gensym("cv"), A_NULL);
    class_addmethod(plts_class, (t_method)plts_midi, gensym("midi"), A_NULL);
    class_addmethod(plts_class, (t_method)plts_hz, gensym("hz"), A_NULL);
}
