// based on the plaits engine (mode 13) by Mutable instruments

#include <stdint.h>

#include "plaits/dsp/dsp.h"
#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/voice.h"

#include <m_pd.h>

static t_class *plaits_class;

typedef struct _plaits{
    t_object            x_obj;
    t_float             x_f;
    t_int               x_n;
    t_float             x_pitch_correction;
    t_float             x_harmonics;
    t_float             x_timbre;
    t_float             x_morph;
    t_float             x_midi_pitch;
    t_float             x_midi_tr;
    t_float             x_midi_lvl;
    bool                x_midi_mode;
    t_int               x_block_size;
    t_int               x_block_count;
    t_int               x_last_n;
    t_int               x_last_engine;
    t_int               x_last_engine_perform;
    plaits::Voice       x_voice;
    plaits::Patch       x_patch;
    plaits::Modulations x_modulations;
    char                x_shared_buffer[16384];
}t_plaits;

extern "C"{
    t_int *plaits_perform(t_int *w);
    t_int *plaits_perform_midi(t_int *w);
    void  *plaits_new(t_symbol *s, int ac, t_atom *av);
    void   plaits_dsp(t_plaits *x, t_signal **sp);
    void   plaits_free(t_plaits *x);
    void   drum_tilde_setup(void);
    void   plaits_harmonics(t_plaits *x, t_floatarg f);
    void   plaits_timbre(t_plaits *x, t_floatarg f);
    void   plaits_morph(t_plaits *x, t_floatarg f);
    void   plaits_list(t_plaits *x, t_symbol *s, int ac, t_atom *av);
}

void plaits_list(t_plaits *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
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
    x->x_midi_tr = x->x_midi_lvl = 0;
    x->x_midi_pitch = atom_getfloat(av);
    ac--, av++;
    if(ac){
        float vel = atom_getfloat(av);
        x->x_midi_tr = vel != 0;
        x->x_midi_lvl = vel / 127.;
    }
}

void plaits_harmonics(t_plaits *x, t_floatarg f){
    x->x_harmonics = f < 0 ? 0 : f > 1 ? 1 : f;
}

void plaits_timbre(t_plaits *x, t_floatarg f){
    x->x_timbre = f < 0 ? 0 : f > 1 ? 1 : f;
}

void plaits_morph(t_plaits *x, t_floatarg f){
    x->x_morph = f < 0 ? 0 : f > 1 ? 1 : f;
}

static float plaits_get_pitch(t_plaits *x, t_floatarg f){
    return(log2f((f < 0 ? f * -1 : f)/440) + 0.75);
}

t_int *plaits_perform(t_int *w){
    t_plaits *x     = (t_plaits *)(w[1]);
    t_sample *freq  = (t_sample *)(w[2]);  // frequency input
    t_sample *trig  = (t_sample *)(w[3]);  // trigger input
    t_sample *level = (t_sample *)(w[4]);  // level input
    t_sample *out   = (t_sample *)(w[5]);  // out
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
    x->x_patch.engine = 0; // Bass Drum Model
/*    int active_engine = x->x_voice.active_engine(); // Send current engine
    if(x->x_last_engine_perform > 128 && x->x_last_engine != active_engine){
        x->x_last_engine = active_engine;
        x->x_last_engine_perform = 0;
    }
    else
        x->x_last_engine_perform++;*/
    x->x_patch.harmonics = x->x_harmonics;
    x->x_patch.timbre = x->x_timbre;
    x->x_patch.morph = x->x_morph;
    x->x_patch.timbre_modulation_amount = 0.0f;
    x->x_patch.frequency_modulation_amount = 0.0f;
    x->x_patch.morph_modulation_amount = 0.0f;
    x->x_modulations.trigger_patched = 1;
    x->x_modulations.level_patched = 1;
    x->x_modulations.frequency_patched = 0;
    x->x_modulations.timbre_patched = 0;
    x->x_modulations.morph_patched = 0;
    for(int j = 0; j < x->x_block_count; j++){
        float pitch = plaits_get_pitch(x, freq[x->x_block_size * j]);
        x->x_modulations.trigger = (trig[x->x_block_size * j] != 0);
        x->x_modulations.level = level[x->x_block_size * j];
        x->x_patch.note = 60 + (pitch + x->x_pitch_correction) * 12.f;
        x->x_modulations.timbre = 0.f;
        x->x_modulations.frequency = 0.f;
        x->x_modulations.morph = 0.f;
        x->x_modulations.harmonics = 0.f;
        plaits::Voice::Frame output[x->x_block_size];
        x->x_voice.Render(x->x_patch, x->x_modulations, output, x->x_block_size);
        for(int i = 0; i < x->x_block_size; i++){
            if(1) // mode
                out[i + (x->x_block_size * j)] = output[i].out / 32768.0f;
            else
                out[i + (x->x_block_size * j)] = output[i].aux / 32768.0f;
        }
    }
    return(w+6);
}

void plaits_dsp(t_plaits *x, t_signal **sp){
    x->x_pitch_correction = log2f(48000.f / sys_getsr());
    x->x_n = sp[0]->s_n;
    dsp_add(plaits_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

void plaits_free(t_plaits *x){
    x->x_voice.FreeEngines();
}

void *plaits_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_plaits *x = (t_plaits *)pd_new(plaits_class);
    stmlib::BufferAllocator allocator(x->x_shared_buffer, sizeof(x->x_shared_buffer));
    x->x_voice.Init(&allocator);
    int floatarg = 0;
    x->x_pitch_correction = log2f(48000.f / sys_getsr());
    x->x_harmonics = x->x_timbre = x->x_morph = 0.5f;
    x->x_last_engine = x->x_last_engine_perform = 0;
    x->x_last_n = 0;
    while(ac){
        if((av)->a_type == A_SYMBOL)
            goto errstate;
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
                    }
                }
            }
        }
    }
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"), gensym ("signal"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"), gensym ("signal"));
    outlet_new(&x->x_obj, &s_signal);
    return(void *)x;
errstate:
    pd_error(x, "[drum~]: improper args");
    return(NULL);
}

void drum_tilde_setup(void){
    plaits_class = class_new(gensym("drum~"), (t_newmethod)plaits_new,
        (t_method)plaits_free, sizeof(t_plaits), 0, A_GIMME, 0);
    class_addmethod(plaits_class, (t_method)plaits_dsp, gensym("dsp"), A_CANT, 0);
    CLASS_MAINSIGNALIN(plaits_class, t_plaits, x_f);
    class_addlist(plaits_class, plaits_list);
    class_addmethod(plaits_class, (t_method)plaits_harmonics, gensym("drive"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_timbre, gensym("tone"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_morph, gensym("decay"), A_FLOAT, 0);
}
