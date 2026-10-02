// based on the plaits engine by Mutable Instruments (MIT)

#include <m_pd.h>
#include <math.h>
#include "dsp.h"
#include "voice.h"

static t_class *bd_class;

typedef struct _bd{
    t_object            x_obj;
    t_int               x_mode;        // 0: 808 (out), 1: 909-ish (aux)
    t_int               x_k_trig;      // bang trigger
    t_int               x_block_size;
    t_int               x_block_count;
    t_int               x_n;
    t_int               x_nchans;
    plaits::Voice       *x_voice;
    plaits::Patch       x_patch;
    plaits::Modulations x_modulations;
}t_bd;

extern "C" void bassdrum_tilde_setup(void);
extern "C" void bd_tilde_setup(void);

static float bd_clip01(float f){
    return f < 0 ? 0 : f > 1 ? 1 : f;
}

static void bd_freq(t_bd *x, t_floatarg f){
    t_floatarg pitch = log2f((f < 0 ? f * -1 : f) / 440) + 0.75;
    x->x_patch.note = 60.0f + pitch * 12.f;
}

static void bd_bang(t_bd *x){
    x->x_k_trig = 1;
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

static void bd_level(t_bd *x, t_floatarg f){
    x->x_modulations.level = bd_clip01(f);
}

static void bd_ptime(t_bd *x, t_floatarg f){ // pitch env time
    x->x_patch.decay = bd_clip01(f);
}

static void bd_pdepth(t_bd *x, t_floatarg f){ // pitch env depth
    x->x_patch.frequency_modulation_amount = bd_clip01(f);
}

static t_int *bd_perform(t_int *w){
    t_bd *x = (t_bd *)(w[1]);
    t_sample *in  = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = x->x_n;
    plaits::Voice::Frame output[plaits::kMaxBlockSize];
    for(int c = 0; c < x->x_nchans; c++){
        t_sample *trig = in + c * n;
        t_sample *outc = out + c * n;
        for(int j = 0; j < x->x_block_count; j++){
            int base = x->x_block_size * j;
            int trigger_at = -1;
            for(int i = 0; i < x->x_block_size; i++){
                if(trig[base + i] != 0){
                    x->x_modulations.level = bd_clip01(fabsf(trig[base + i]));
                    trigger_at = i;
                    break;
                }
            }
            if(x->x_k_trig){ // bang: trigger at the start of the block
                if(trigger_at < 0)
                    trigger_at = 0;
                x->x_k_trig = 0;
            }
            if(trigger_at > 0){
                x->x_modulations.trigger = 0;
                x->x_voice[c].Render(x->x_patch, x->x_modulations, output, trigger_at);
                for(int i = 0; i < trigger_at; i++){
                    short bd = x->x_mode ? output[i].aux : output[i].out;
                    outc[i + base] = (float)bd / 32768.0f;
                }
                x->x_modulations.trigger = 1;
                x->x_voice[c].Render(x->x_patch, x->x_modulations, output, x->x_block_size - trigger_at);
                for(int i = 0; i < x->x_block_size - trigger_at; i++){
                    short bd = x->x_mode ? output[i].aux : output[i].out;
                    outc[i + base + trigger_at] = (float)bd / 32768.0f;
                }
            }
            else{
                x->x_modulations.trigger = (trigger_at == 0);
                x->x_voice[c].Render(x->x_patch, x->x_modulations, output, x->x_block_size);
                for(int i = 0; i < x->x_block_size; i++){
                    short bd = x->x_mode ? output[i].aux : output[i].out;
                    outc[i + base] = (float)bd / 32768.0f;
                }
            }
        }
    }
    return(w+4);
}

static void bd_dsp(t_bd *x, t_signal **sp){
    plaits::kSampleRate = (float)sp[0]->s_sr;
    plaits::a0 = 55.f / plaits::kSampleRate;
    int n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
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
    if(chs != x->x_nchans){
        x->x_voice = (plaits::Voice *)resizebytes(x->x_voice,
            x->x_nchans * sizeof(plaits::Voice), chs * sizeof(plaits::Voice));
        for(int c = x->x_nchans; c < chs; c++)
            x->x_voice[c].Init();
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(bd_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
}

static void *bd_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_bd *x = (t_bd *)pd_new(bd_class);
    float pitch = 50, punch = 0.5f, tone = 0.5f, decay = 0.5f, ptime = 0, pdepth = 0, lvl = 0.5f;
    int nfloats = 0;
    x->x_mode = 0;
    while(ac){
        if(av->a_type == A_SYMBOL){
            t_symbol *sym = atom_getsymbol(av);
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
                case 0: pitch = f; break;   // freq (Hz)
                case 1: lvl = f; break;     // accent
                case 2: punch = f; break;   // harmonics
                case 3: tone = f; break;    // timbre
                case 4: decay = f; break;   // morph
                case 5: ptime = f; break;   // pitch env time
                case 6: pdepth = f; break;  // pitch env depth
                default: goto errstate;
            }
        }
    }
    x->x_k_trig = 0;
    x->x_n = 0;
    x->x_nchans = 1;
    x->x_voice = (plaits::Voice *)getbytes(sizeof(plaits::Voice));
    x->x_modulations.level = bd_clip01(lvl);
    x->x_modulations.level_patched = true;
    bd_freq(x, pitch);
    x->x_patch.harmonics = bd_clip01(punch);
    x->x_patch.timbre = bd_clip01(tone);
    x->x_patch.morph = bd_clip01(decay);
    x->x_patch.timbre_modulation_amount = 0;
    x->x_patch.morph_modulation_amount = 0;
    bd_ptime(x, ptime);
    bd_pdepth(x, pdepth);
    x->x_modulations.trigger = 0;
    x->x_voice[0].Init();
    outlet_new(&x->x_obj, &s_signal);
    return(void *)x;
errstate:
    pd_error(x, "[bassdrum~]: improper args");
    return(NULL);
}

void bassdrum_tilde_setup(void){
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
