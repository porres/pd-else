// Created by Timothy Schoen and Porres
// Based on Elliptic BLEP by signalsmith-audio: https://github.com/Signalsmith-Audio/elliptic-blep

#include <m_pd.h>
#include <blep.h>

#define PI     3.1415926535897931
#define TWO_PI 6.2831853071795862


typedef struct bltri{
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
}t_bltri;

t_class *bl_tri;

static void bltri_midi(t_bltri *x, t_floatarg f){
    x->x_midi = (int)(f != 0);
}

static void bltri_soft(t_bltri *x, t_floatarg f){
    x->x_soft = (int)(f != 0);
}

static t_int* bltri_perform(t_int *w) {
    t_bltri* x =          (t_bltri*)(w[1]);
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
        t_float triangle = 2.0f * fabs(2.0f * ((x->x_phase + 0.25f) - floor(x->x_phase + 0.75f))) - 1.0f;
        *out++ = triangle + elliptic_blep_get(blep);
        
        if(sync > 0 && sync <= 1){ // Phase sync
            if(x->x_soft)
                x->x_soft = x->x_soft == 1 ? -1 : 1;
            else {
                x->x_phase = phasewrap(sync);
            }
        }
        else { // Phase modulation
            double phase_dev = phase_offset - x->x_last_phase_offset;
            if(phase_dev >= 1 || phase_dev <= -1)
                phase_dev = fmod(phase_dev, 1);
            x->x_phase = phasewrap(x->x_phase + phase_dev);
        }
        
        t_float phase_increment = freq / x->x_sr; // Update frequency
        x->x_phase += phase_increment;
        if(x->x_soft)
            phase_increment *= x->x_soft;
        
        elliptic_blep_step(blep);
        
        x->x_phase = phasewrap(x->x_phase);
        if(x->x_phase >= 0.25f && x->x_phase < 0.25f + phase_increment) {
            t_float samples_in_past = (x->x_phase - 0.25f) / phase_increment;
            elliptic_blep_add_in_past(blep, phase_increment * -4.0f, 2, samples_in_past);
        }
        else if (x->x_phase >= 0.75f && x->x_phase < 0.75f + phase_increment) {
            t_float samples_in_past = (x->x_phase - 0.75f) / phase_increment;
            elliptic_blep_add_in_past(blep, phase_increment * 4.0f, 2, samples_in_past);
        }
        
        
        x->x_last_phase_offset = phase_offset;
    }
    return(w+7);
}

static void bltri_dsp(t_bltri *x, t_signal **sp){
    x->x_sr = sp[0]->s_sr;
    elliptic_blep_create(&x->x_elliptic_blep, 0, sp[0]->s_sr);
    dsp_add(bltri_perform, 6, x, sp[0]->s_n, sp[0]->s_vec,
            sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
 }

static void bltri_free(t_bltri *x){
    inlet_free(x->x_inlet_sync);
    inlet_free(x->x_inlet_phase);
}

static void* bltri_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_bltri* x = (t_bltri *)pd_new(bl_tri);
    x->x_phase = 1.0f;
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

void setup_bl0x2etri_tilde(void){
    bl_tri = class_new(gensym("bl.tri~"), (t_newmethod)bltri_new,
        (t_method)bltri_free, sizeof(t_bltri), 0, A_GIMME, A_NULL);
    CLASS_MAINSIGNALIN(bl_tri, t_bltri, x_f);
    class_addmethod(bl_tri, (t_method)bltri_soft, gensym("soft"), A_DEFFLOAT, 0);
    class_addmethod(bl_tri, (t_method)bltri_midi, gensym("midi"), A_DEFFLOAT, 0);
    class_addmethod(bl_tri, (t_method)bltri_dsp, gensym("dsp"), A_NULL);
}
