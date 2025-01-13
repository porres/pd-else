// Created by Timothy Schoen and Porres
// Based on Elliptic BLEP by signalsmith-audio: https://github.com/Signalsmith-Audio/elliptic-blep

#include <m_pd.h>
#include "blep.h"

typedef struct blsaw{
    t_object    x_obj;
    t_float     x_f;
    t_elliptic_blep x_elliptic_blep;
    t_inlet*    x_inlet_sync;
    t_inlet*    x_inlet_phase;
    t_float     x_phase;
    t_float     x_sr;
    t_float     x_last_phase_offset;
    t_int       x_midi;
    t_int       x_soft;
}t_blsaw;

t_class *blsaw_class;

static void blsaw_midi(t_blsaw *x, t_floatarg f){
    x->x_midi = (int)(f != 0);
}

static void blsaw_soft(t_blsaw *x, t_floatarg f){
    x->x_soft = (int)(f != 0);
}


static t_int* blsaw_perform(t_int *w) {
    t_blsaw* x =          (t_blsaw*)(w[1]);
    t_elliptic_blep* blep = &x->x_elliptic_blep;
    t_int n            = (t_int)(w[2]);
    t_float* freq_vec  = (t_float *)(w[3]);
    t_float* sync_vec  = (t_float *)(w[4]);
    t_float* phase_vec = (t_float *)(w[5]);
    t_float* out       = (t_float *)(w[6]);
    while(n--){
        t_float freq = *freq_vec++;
        t_float sync = *sync_vec++;
        t_float phase_offset = *phase_vec++;
        if(x->x_midi){
            if(freq > 127)
                freq = 127;
            freq = freq <= 0 ? 0 : pow(2, (freq - 69)/12) * 440;
        }
        
        if(sync > 0 && sync <= 1){ // Phase sync
            if(x->x_soft)
                x->x_soft = x->x_soft == 1 ? -1 : 1;
            else{
                x->x_phase = phasewrap(sync);
            }
        }
        else { // Phase modulation
            double phase_dev = phase_offset - x->x_last_phase_offset;
            x->x_phase = phasewrap(x->x_phase + phase_dev);
        }
        
        *out++ = (1.0f - 2.0f * x->x_phase) + elliptic_blep_get(blep);
        
        t_float phase_increment = freq / x->x_sr; // Update frequency
        x->x_phase += phase_increment;
        if(x->x_soft)
            phase_increment *= x->x_soft;
        
        elliptic_blep_step(blep);
        
        if(x->x_phase >= 1 || x->x_phase < 0) {
            x->x_phase = x->x_phase < 0 ? x->x_phase+1 : x->x_phase-1;
            t_float samples_in_past = x->x_phase / phase_increment;
            elliptic_blep_add_in_past(blep, 2.0f, 1, samples_in_past);
        }
    }
    return(w+7);
}

static void blsaw_dsp(t_blsaw *x, t_signal **sp){
    x->x_sr = sp[0]->s_sr;
    
    elliptic_blep_create(&x->x_elliptic_blep, 0, sp[0]->s_sr);
    
    dsp_add(blsaw_perform, 6, x, sp[0]->s_n, sp[0]->s_vec,
            sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
 }

static void blsaw_free(t_blsaw *x){
    inlet_free(x->x_inlet_sync);
    inlet_free(x->x_inlet_phase);
}

static void* blsaw_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_blsaw* x = (t_blsaw *)pd_new(blsaw_class);
    x->x_phase = 0.0;
    x->x_soft = 0;
    x->x_midi = 0;
    x->x_last_phase_offset = 0;
    t_float init_freq = 0, init_phase = 0;
    while(ac && av->a_type == A_SYMBOL){
        if(atom_getsymbol(av) == gensym("-midi"))
            x->x_midi = 1;
        else if(atom_getsymbol(av) == gensym("-soft"))
            x->x_soft = 1;
        ac--, av++;
    }
    if(ac && av->a_type == A_FLOAT){
        init_freq = av->a_w.w_float;
        ac--; av++;
        if(ac && av->a_type == A_FLOAT){
            init_phase = av->a_w.w_float;
            ac--; av++;
        }
    }
    x->x_f = init_freq;
    outlet_new(&x->x_obj, &s_signal);
    x->x_inlet_sync = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_sync, 0);
    x->x_inlet_phase = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_phase, init_phase);
    return(void *)x;
}

void setup_bl0x2esaw_tilde(void){
    blsaw_class = class_new(gensym("bl.saw~"), (t_newmethod)blsaw_new,
        (t_method)blsaw_free, sizeof(t_blsaw), 0, A_GIMME, A_NULL);
    CLASS_MAINSIGNALIN(blsaw_class, t_blsaw, x_f);
    class_addmethod(blsaw_class, (t_method)blsaw_midi, gensym("midi"), A_DEFFLOAT, 0);
    class_addmethod(blsaw_class, (t_method)blsaw_soft, gensym("soft"), A_DEFFLOAT, 0);
    class_addmethod(blsaw_class, (t_method)blsaw_dsp, gensym("dsp"), A_NULL);
}
