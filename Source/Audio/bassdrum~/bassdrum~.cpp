// based on the plaits engine by Mutable instruments

#include <stdint.h>

#include "plaits/dsp/dsp.h"
#include "plaits/dsp/engine/engine.h"
#include "plaits/dsp/voice.h"

#include <m_pd.h>

static t_class *plaits_class;

typedef struct _plaits{
    t_object            x_obj;
    t_int               x_n;
    t_int               x_mode;
    t_int               x_k_trig;
    t_float             x_pitch_correction;
    t_float             x_harmonics;
    t_float             x_timbre;
    t_float             x_morph;
    t_float             x_pitch;
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
    void   bassdrum_tilde_setup(void);
    void   plaits_harmonics(t_plaits *x, t_floatarg f);
    void   plaits_freq(t_plaits *x, t_floatarg f);
    void   plaits_level(t_plaits *x, t_floatarg f);
    void   plaits_mode(t_plaits *x, t_floatarg f);
    void   plaits_timbre(t_plaits *x, t_floatarg f);
    void   plaits_morph(t_plaits *x, t_floatarg f);
    void   plaits_list(t_plaits *x, t_symbol *s, int ac, t_atom *av);
}

void plaits_freq(t_plaits *x, t_floatarg f){
    x->x_pitch = log2f((f < 0 ? f * -1 : f)/440) + 0.75;
}

void plaits_bang(t_plaits *x){
    x->x_k_trig = 1;
}

void plaits_level(t_plaits *x, t_floatarg f){
    x->x_modulations.level = f;
}

void plaits_mode(t_plaits *x, t_floatarg f){
    x->x_mode = f != 0;
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

t_int *plaits_perform(t_int *w){
    t_plaits *x    = (t_plaits *)(w[1]);
    t_sample *trig = (t_sample *)(w[2]);  // trigger
    t_sample *out  = (t_sample *)(w[3]);  // out
    int n = x->x_n; // block size
    float pitch = x->x_pitch;
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
    int nsize = x->x_block_size;
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
    for(int j = 0; j < x->x_block_count; j++){
        float trigger_v = trig[nsize*j];
        int trigger = (trigger_v != 0);
        if(trigger)
            x->x_modulations.level = trigger_v;
        if(x->x_k_trig){
            trigger = 1;
            x->x_k_trig = 0;
        }
        x->x_modulations.trigger = trigger;
        x->x_patch.note = 60 + (pitch + x->x_pitch_correction) * 12.f;
        plaits::Voice::Frame output[nsize];
        x->x_voice.Render(x->x_patch, x->x_modulations, output, nsize);
        for(int i = 0; i < nsize; i++){
            if(!x->x_mode) // TR_808 revised
                out[i + nsize*j] = output[i].out / 32768.0f;
            else // Inadvertedly TR_908-ish
                out[i + nsize*j] = output[i].aux / 32768.0f;
        }
    }
    return(w+4);
}

void plaits_dsp(t_plaits *x, t_signal **sp){
    x->x_pitch_correction = log2f(48000.f / sys_getsr());
    x->x_n = sp[0]->s_n;
    dsp_add(plaits_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
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
    float pitch = 60;
    float lvl = 0.5f;
    while(ac){
        if((av)->a_type == A_SYMBOL)
            goto errstate;
        else{
            floatarg = 1;
            pitch = atom_getfloat(av); // pitch
            ac--, av++;
            if(ac && (av)->a_type == A_FLOAT){ // level
                lvl = atom_getfloat(av);
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
    }
    x->x_patch.timbre_modulation_amount = 0.0f;
    x->x_patch.frequency_modulation_amount = 0.0f;
    x->x_patch.morph_modulation_amount = 0.0f;
    x->x_modulations.trigger_patched = 1;
    x->x_modulations.level_patched = 1;
    x->x_modulations.frequency_patched = 0;
    x->x_modulations.timbre_patched = 0;
    x->x_modulations.morph_patched = 0;
    x->x_modulations.timbre = 0.f;
    x->x_modulations.frequency = 0.f;
    x->x_modulations.morph = 0.f;
    x->x_modulations.harmonics = 0.f;
    x->x_modulations.level = lvl;
    plaits_freq(x, pitch);
    outlet_new(&x->x_obj, &s_signal);
    return(void *)x;
errstate:
    pd_error(x, "[bassdrum~]: improper args");
    return(NULL);
}

void bassdrum_tilde_setup(void){
    plaits_class = class_new(gensym("bassdrum~"), (t_newmethod)plaits_new,
        (t_method)plaits_free, sizeof(t_plaits), 0, A_GIMME, 0);
    class_addmethod(plaits_class, (t_method)plaits_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(plaits_class, nullfn, gensym("signal"), A_NULL);
    class_addbang(plaits_class, plaits_bang);
    class_addmethod(plaits_class, (t_method)plaits_mode, gensym("mode"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_level, gensym("level"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_freq, gensym("freq"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_harmonics, gensym("punch"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_timbre, gensym("tone"), A_FLOAT, 0);
    class_addmethod(plaits_class, (t_method)plaits_morph, gensym("decay"), A_FLOAT, 0);
}
