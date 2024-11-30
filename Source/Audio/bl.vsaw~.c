// Created by Timothy Schoen and Porres
// Based on Elliptic BLEP by signalsmith-audio: https://github.com/Signalsmith-Audio/elliptic-blep

#include <m_pd.h>
#include <blep.h>

typedef struct blvsaw{
    t_object    x_obj;
    t_float     x_f;
    t_elliptic_blep x_elliptic_blep;
    t_inlet*    x_inlet_sync;
    t_inlet*    x_inlet_phase;
    t_inlet*    x_inlet_width;
    t_float     x_phase;
    t_float     x_sr;
    t_float     x_last_phase_offset;
    t_float     x_pulse_width;
    t_int       x_midi;
    t_int       x_soft;
}t_blvsaw;

t_class *blvsaw_class;

static void blvsaw_midi(t_blvsaw *x, t_floatarg f){
    x->x_midi = (int)(f != 0);
}

static void blvsaw_soft(t_blvsaw *x, t_floatarg f){
    x->x_soft = (int)(f != 0);
}


static t_int* blvsaw_perform(t_int *w) {
    t_blvsaw* x      = (t_blvsaw*)(w[1]);
    t_elliptic_blep* blep = &x->x_elliptic_blep;
    t_int n            = (t_int)(w[2]);
    t_float* freq_vec  = (t_float *)(w[3]);
    t_float* width_vec = (t_float *)(w[4]);
    t_float* sync_vec  = (t_float *)(w[5]);
    t_float* phase_vec = (t_float *)(w[6]);
    t_float* out       = (t_float *)(w[7]);
    while(n--){
        t_float freq = *freq_vec++;
        t_float sync = *sync_vec++;
        t_float phase_offset = *phase_vec++;
        t_float pulse_width = *width_vec++;
        if(x->x_midi && freq < 255)
            freq = pow(2, (freq - 69)/12) * 440;
        // Update pulse width, limit between 0 and 1
        x->x_pulse_width = fmax(fmin(0.9999, pulse_width), 0.0001);
        t_float phase_increment = freq / x->x_sr; // Update frequency
        if(x->x_soft)
            phase_increment *= x->x_soft;
        if(sync > 0 && sync <= 1){ // Phase sync
            if(x->x_soft)
                x->x_soft = x->x_soft == 1 ? -1 : 1;
            else{
                x->x_phase = sync;
                x->x_phase = phasewrap(x->x_phase);
            }
        }
        else{ // Phase modulation
            double phase_dev = phase_offset - x->x_last_phase_offset;
            if(phase_dev >= 1 || phase_dev <= -1)
                phase_dev = fmod(phase_dev, 1);
            x->x_phase = phasewrap(x->x_phase + phase_dev);
        }
        
        t_float output;
        if (pulse_width == 0)
            output = x->x_phase * -2 + 1;
        else if (pulse_width == 1)
            output = x->x_phase * 2 - 1;
        else {
            t_float inc = x->x_phase * pulse_width;                   // phase * 0.5
            t_float dec = (x->x_phase - 1) * (pulse_width - 1);       //
            t_float gain = pow(pulse_width * (pulse_width - 1), -1);
            t_float min = (inc < dec ? inc : dec);
            output = (min * gain) * 2 + 1;
        }
        
        *out++ = output + elliptic_blep_get(blep);
        
        x->x_phase += phase_increment;
        if(x->x_soft)
            phase_increment *= x->x_soft;
        
        elliptic_blep_step(blep);
        
        if(pulse_width != 0.0f && pulse_width != 1.0f)
        {
            if(x->x_phase >= 1 || x->x_phase < 0) {
                x->x_phase = x->x_phase < 0 ? x->x_phase+1 : x->x_phase-1;
                t_float samples_in_past = x->x_phase / phase_increment;
                elliptic_blep_add_in_past(blep, phase_increment * -8.0, 2, samples_in_past);
            }
            else if (x->x_phase >= x->x_pulse_width && x->x_phase < x->x_pulse_width + phase_increment) {
                t_float samples_in_past = (x->x_phase - x->x_pulse_width) / phase_increment;
                elliptic_blep_add_in_past(blep, phase_increment * 8.0, 2, samples_in_past);
            }
        }
        else {
            if(x->x_phase >= 1 || x->x_phase < 0) {
                x->x_phase = x->x_phase < 0 ? x->x_phase+1 : x->x_phase-1;
                t_float samples_in_past = x->x_phase / phase_increment;
                elliptic_blep_add_in_past(blep, pulse_width == 0.0f ? 2.0f : -2.0f, 1, samples_in_past);
            }
        }
       
        x->x_last_phase_offset = phase_offset;
    }
    return(w+8);
}

static void blvsaw_dsp(t_blvsaw *x, t_signal **sp){
    x->x_sr = sp[0]->s_sr;
    elliptic_blep_create(&x->x_elliptic_blep, 0, sp[0]->s_sr);
    dsp_add(blvsaw_perform, 7, x, sp[0]->s_n, sp[0]->s_vec,
        sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
 }

static void blvsaw_free(t_blvsaw *x){
    inlet_free(x->x_inlet_sync);
    inlet_free(x->x_inlet_phase);
    inlet_free(x->x_inlet_width);
}

static void* blvsaw_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_blvsaw* x = (t_blvsaw *)pd_new(blvsaw_class);
    x->x_pulse_width = 0;
    x->x_last_phase_offset = 0;
    x->x_phase = 0.0;
    x->x_midi = 0;
    x->x_soft = 0;
    x->x_pulse_width = 0.5f;
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
            x->x_pulse_width = av->a_w.w_float;
            ac--; av++;
        }
        if(ac && av->a_type == A_FLOAT){
            init_phase = av->a_w.w_float;
            ac--; av++;
        }
    }
    x->x_f = init_freq;
    outlet_new(&x->x_obj, &s_signal);
    x->x_inlet_width = inlet_new(&x->x_obj, &x->x_obj.ob_pd,  &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_width, x->x_pulse_width);
    x->x_inlet_sync = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_sync, 0);
    x->x_inlet_phase = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_phase, init_phase);
    return(void *)x;
}

void setup_bl0x2evsaw_tilde(void){
    blvsaw_class = class_new(gensym("bl.vsaw~"), (t_newmethod)blvsaw_new,
        (t_method)blvsaw_free, sizeof(t_blvsaw), 0, A_GIMME, A_NULL);
    CLASS_MAINSIGNALIN(blvsaw_class, t_blvsaw, x_f);
    class_addmethod(blvsaw_class, (t_method)blvsaw_soft, gensym("soft"), A_DEFFLOAT, 0);
    class_addmethod(blvsaw_class, (t_method)blvsaw_midi, gensym("midi"), A_DEFFLOAT, 0);
    class_addmethod(blvsaw_class, (t_method)blvsaw_dsp, gensym("dsp"), A_NULL);
}
