// based on the plaits engine by Mutable instruments

#include <m_pd.h>
#include "bass_drum.h"

static t_class *bd_class;

typedef struct _bd{
    t_object            x_obj;
    t_int               x_mode;
    t_int               x_k_trig;
    t_int               x_block_size;
    t_int               x_block_count;
    t_int               x_last_n;
    plaits::Voice       x_voice;
    plaits::Patch       x_patch;
    plaits::Modulations x_modulations;
}t_bd;

extern "C" void bassdrum_tilde_setup(void);

static float bd_clip01(float f){
    return f < 0 ? 0 : f > 1 ? 1 : f;
}

static void bd_freq(t_bd *x, t_floatarg f){
    x->x_patch.note = fabsf(f);
}

static void bd_bang(t_bd *x){
    x->x_k_trig = 1;
}

static void bd_level(t_bd *x, t_floatarg f){
    x->x_modulations.level = bd_clip01(f);
}

static void bd_mode(t_bd *x, t_floatarg f){
    x->x_mode = f != 0;
}

static void bd_punch(t_bd *x, t_floatarg f){
    x->x_patch.harmonics = bd_clip01(f);
}

static void bd_tone(t_bd *x, t_floatarg f){
    x->x_patch.timbre = bd_clip01(f);
}

static void bd_decay(t_bd *x, t_floatarg f){
    x->x_patch.morph = bd_clip01(f);
}

static void bd_penv(t_bd *x, t_floatarg f){
    x->x_patch.decay = x->x_patch.frequency_modulation_amount = bd_clip01(f);
}

static t_int *bd_perform(t_int *w){
    t_bd *x = (t_bd *)(w[1]);
    t_sample *trig = (t_sample *)(w[2]);    // trigger
    t_sample *out  = (t_sample *)(w[3]);    // out
    t_int n = (t_int)w[4];                  // block size
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
    for(int j = 0; j < x->x_block_count; j++){
        float trigger_v = trig[nsize*j];
        int trigger = (trigger_v != 0);
        if(trigger)
            x->x_modulations.level = bd_clip01(trigger_v);
        if(x->x_k_trig){
            trigger = 1;
            x->x_k_trig = 0;
        }
        x->x_modulations.trigger = trigger;
        plaits::Voice::Frame output[plaits::kMaxBlockSize];
        x->x_voice.Render(x->x_patch, x->x_modulations, output, nsize);
        for(int i = 0; i < nsize; i++){
            if(!x->x_mode) // TR_808 revised
                out[i + nsize*j] = output[i].out;
            else // Inadvertedly TR_908-ish
                out[i + nsize*j] = output[i].aux;
        }
    }
    return(w+5);
}

static void bd_dsp(t_bd *x, t_signal **sp){
    plaits::kSampleRate = (float)sp[0]->s_sr;
    dsp_add(bd_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
}

void *bd_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_bd *x = (t_bd *)pd_new(bd_class);
    int arg = 0;
    float punch = 0.5f, tone = 0.5f, decay = 0.5f, penv = 0.5f, lvl = 0.5f;
    float pitch = 50;
    x->x_mode = 0;
    while(ac){
        if((av)->a_type == A_SYMBOL){
            if(arg)
                goto errstate;
            else if(atom_getsymbol(av) == gensym("-mode")){
                ac--, av++;
                if((av)->a_type == A_FLOAT){
                    x->x_mode = atom_getint(av) != 0;
                    ac--, av++;
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
            arg = 1;
        }
        else{
            arg = 1;
            pitch = atom_getfloat(av); // freq
            ac--, av++;
            if(ac && (av)->a_type == A_FLOAT){ // punch (harmonics)
                punch = atom_getfloat(av);
                ac--, av++;
                if(ac && (av)->a_type == A_FLOAT){ // tone (timbre)
                    tone = atom_getfloat(av);
                    ac--, av++;
                    if(ac && (av)->a_type == A_FLOAT){ // decay (morph)
                        decay = atom_getfloat(av);
                        ac--, av++;
                        if(ac && (av)->a_type == A_FLOAT){ // penv
                            penv = atom_getfloat(av);
                            ac--, av++;
                            if(ac && (av)->a_type == A_FLOAT){ // level
                                lvl = atom_getfloat(av);
                                ac--, av++;
                            }
                        }
                    }
                }
            }
        }
    }
    bd_freq(x, pitch);
    x->x_patch.harmonics = punch;
    x->x_patch.timbre = tone;
    x->x_patch.morph = decay;
    x->x_patch.decay = x->x_patch.frequency_modulation_amount = penv;
    x->x_modulations.trigger = 0;
    x->x_modulations.level = lvl;
    x->x_last_n = 0;
    x->x_voice.Init();
    outlet_new(&x->x_obj, &s_signal);
    return(void *)x;
errstate:
    pd_error(x, "[bassdrum~]: improper args");
    return(NULL);
}
    
void bassdrum_tilde_setup(void){
    bd_class = class_new(gensym("bassdrum~"), (t_newmethod)bd_new,
        0, sizeof(t_bd), 0, A_GIMME, 0);
    class_addmethod(bd_class, (t_method)bd_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(bd_class, nullfn, gensym("signal"), A_NULL);
    class_addbang(bd_class, bd_bang);
    class_addmethod(bd_class, (t_method)bd_mode, gensym("mode"), A_FLOAT, 0);
    class_addmethod(bd_class, (t_method)bd_freq, gensym("freq"), A_FLOAT, 0);
    class_addmethod(bd_class, (t_method)bd_level, gensym("level"), A_FLOAT, 0);
    class_addmethod(bd_class, (t_method)bd_punch, gensym("punch"), A_FLOAT, 0);
    class_addmethod(bd_class, (t_method)bd_tone, gensym("tone"), A_FLOAT, 0);
    class_addmethod(bd_class, (t_method)bd_decay, gensym("decay"), A_FLOAT, 0);
    class_addmethod(bd_class, (t_method)bd_penv, gensym("penv"), A_FLOAT, 0);
}
