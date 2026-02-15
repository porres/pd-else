/*
// Created by Timothy Schoen and Porres
// Based on Elliptic BLEP by signalsmith-audio: https://github.com/Signalsmith-Audio/elliptic-blep

#include <m_pd.h>
#include <blep.h>


typedef struct blsquare{
    t_object x_obj;
    t_float x_f;
    t_elliptic_blep x_elliptic_blep;
    t_inlet* x_inlet_sync;
    t_inlet* x_inlet_phase;
    t_inlet* x_inlet_width;
    t_float     x_phase;
    t_float     x_sr;
    t_float     x_last_phase_offset;
    t_float     x_pulse_width;
    t_int       x_midi;
    t_int       x_soft;
}t_blsquare;

t_class *blsquare_class;

static void blsquare_midi(t_blsquare *x, t_floatarg f){
    x->x_midi = (int)(f != 0);
}

static void blsquare_soft(t_blsquare *x, t_floatarg f){
    x->x_soft = (int)(f != 0);
}

static t_int* blsquare_perform(t_int *w) {
    t_blsquare* x      = (t_blsquare*)(w[1]);
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
        if(x->x_midi){
            if(freq > 127)
                freq = 127;
            freq = freq <= 0 ? 0 : pow(2, (freq - 69)/12) * 440;
        }
        // Update pulse width, limit between 0 and 1
        pulse_width = fmax(fmin(0.99, pulse_width), 0.01);
        x->x_pulse_width = pulse_width;
        
        if(sync > 0 && sync <= 1){ // Phase sync
            if(x->x_soft)
                x->x_soft = x->x_soft == 1 ? -1 : 1;
            else{
                x->x_phase = sync;
                x->x_phase = phasewrap(x->x_phase);
            }
        }
        else { // Phase modulation
            double phase_dev = phase_offset - x->x_last_phase_offset;
            if(phase_dev >= 1 || phase_dev <= -1)
                phase_dev = fmod(phase_dev, 1);
            x->x_phase = phasewrap(x->x_phase + phase_dev);
        }
        
        *out++ = (x->x_phase <= x->x_pulse_width ? 1.0f : -1.0f) + elliptic_blep_get(blep);
        
        t_float phase_increment = freq / x->x_sr; // Update frequency
        t_float last_phase = x->x_phase;
        x->x_phase += phase_increment;
        if(x->x_soft)
            phase_increment *= x->x_soft;
        
        elliptic_blep_step(blep);
        
        if(x->x_phase >= 1 || x->x_phase < 0) {
            x->x_phase = x->x_phase < 0 ? x->x_phase+1 : x->x_phase-1;
            t_float samples_in_past = x->x_phase / phase_increment;
            elliptic_blep_add_in_past(blep, 2.0f, 1, samples_in_past);
        }
        else if (x->x_phase >= x->x_pulse_width && x->x_phase < x->x_pulse_width + phase_increment) {
            t_float samples_in_past = (x->x_phase - x->x_pulse_width) / phase_increment;
            elliptic_blep_add_in_past(blep, -2.0f, 1, samples_in_past);
        }
        
        x->x_last_phase_offset = phase_offset;
    }
    return(w+8);
}

static void blsquare_dsp(t_blsquare *x, t_signal **sp){
    x->x_sr = sp[0]->s_sr;
    elliptic_blep_create(&x->x_elliptic_blep, 0, sp[0]->s_sr);
    dsp_add(blsquare_perform, 7, x, sp[0]->s_n, sp[0]->s_vec,
        sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
 }

static void blsquare_free(t_blsquare *x){
    inlet_free(x->x_inlet_sync);
    inlet_free(x->x_inlet_phase);
    inlet_free(x->x_inlet_width);
}

static void* blsquare_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_blsquare* x = (t_blsquare *)pd_new(blsquare_class);
    x->x_phase = 0.0;
    x->x_soft = 0;
    x->x_midi = 0;
    x->x_last_phase_offset = 0;
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

void setup_bl0x2esquare_tilde(void){
    blsquare_class = class_new(gensym("bl.square~"), (t_newmethod)blsquare_new,
        (t_method)blsquare_free, sizeof(t_blsquare), 0, A_GIMME, A_NULL);
    CLASS_MAINSIGNALIN(blsquare_class, t_blsquare, x_f);
    class_addmethod(blsquare_class, (t_method)blsquare_soft, gensym("soft"), A_DEFFLOAT, 0);
    class_addmethod(blsquare_class, (t_method)blsquare_midi, gensym("midi"), A_DEFFLOAT, 0);
    class_addmethod(blsquare_class, (t_method)blsquare_dsp, gensym("dsp"), A_NULL);
}

*/

// Porres 2017-2025

#include <m_pd.h>
#include <math.h>
#include <stdlib.h>
#include <magic.h>
#include <blep.h>

#define MAXLEN 1024

typedef struct _blsquare{
    t_object    x_obj;
    t_elliptic_blep *x_elliptic_blep;
    t_float     *x_phase;
    t_float     *x_last_phase_offset;
    int         x_nchans;
    t_int       x_n;
    t_int       x_sig1;
    t_int       x_ch2;
    t_int       x_ch3;
    t_int       x_ch4;
    t_int       x_midi;
    t_int       x_soft;
    t_int      *x_dir;
    float      *x_freq_list;
    t_int       x_list_size;
    t_symbol   *x_ignore;
    t_inlet    *x_inlet_phase;
    t_inlet    *x_inlet_sync;
    t_inlet*    x_inlet_width;
    t_outlet   *x_outlet;
    double      x_sr_rec;
// MAGIC:
    t_glist    *x_glist; // object list
}t_blsquare;

static t_class *blsquare_class;

double blsquare_wrap_phase(double phase){
    while(phase >= 1)
        phase -= 1.;
    while(phase < 0)
        phase += 1.;
    return(phase);
}

static t_int *blsquare_perform(t_int *w){
    t_blsquare *x = (t_blsquare *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *in3 = (t_float *)(w[4]);
    t_float *in4 = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);
    t_elliptic_blep *blep = x->x_elliptic_blep; // Now an array
    t_int *dir = x->x_dir;
    t_float *phase = x->x_phase;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0, n = x->x_n; i < n; i++){
            double hz = x->x_sig1 ? in1[j*n + i] : x->x_freq_list[j];
            if(x->x_midi){
                if(hz > 127) hz = 127;
                hz = hz <= 0 ? 0 : pow(2, (hz - 69)/12) * 440;
            }

            t_float pulse_width = x->x_ch2 == 1 ? in2[i] : in2[j*n + i];
            t_float trig = x->x_ch3 == 1 ? in3[i] : in3[j*n + i];
            double phase_offset = x->x_ch4 == 1 ? in4[i] : in4[j*n + i];
            double phase_dev = phase_offset - x->x_last_phase_offset[j];
            x->x_last_phase_offset[j] = phase_offset;
            double last_phase = phase[j];
            double step = hz * x->x_sr_rec;
            step = step > 0.5 ? 0.5 : step < -0.5 ? -0.5 : step;

            if(dir[j] == 0) // initialize this just once
                dir[j] = 1;
            if(trig > 0 && trig <= 1 && x->x_soft){
                dir[j] = dir[j] == 1 ? -1 : 1;
            }
            step *= dir[j];
            
            out[j*n + i] = (phase[j] <= pulse_width ? 1.0f : -1.0f) + elliptic_blep_get(&blep[j]);
            
            phase[j] += (step + phase_dev);
            elliptic_blep_step(&blep[j]);
            
            if(trig > 0 && trig <= 1 && !x->x_soft){
                phase[j] = trig;
            }
            
            if(phase[j] >= 1 || phase[j] < 0) {
                t_float phase_step = blsquare_wrap_phase(x->x_phase[j] - last_phase);
                t_float amp_step = (blsquare_wrap_phase(phase[j]) <= pulse_width ? 1.0f : -1.0f) - (blsquare_wrap_phase(last_phase) <= pulse_width ? 1.0f : -1.0f);
                phase[j] = blsquare_wrap_phase(phase[j]);
                t_float samples_in_past = phase[j] / phase_step;
                elliptic_blep_add_in_past(&blep[j], amp_step, 1, samples_in_past);
            }
            else if (phase[j] >= pulse_width && phase[j] < pulse_width + step) {
                t_float phase_step = blsquare_wrap_phase(x->x_phase[j] - last_phase);
                t_float amp_step = (blsquare_wrap_phase(phase[j]) <= pulse_width ? 1.0f : -1.0f) - (blsquare_wrap_phase(last_phase) <= pulse_width ? 1.0f : -1.0f);
                t_float samples_in_past = (phase[j] - pulse_width) / phase_step;
                elliptic_blep_add_in_past(&blep[j], amp_step, 1, samples_in_past);
            }
        }
    }
    return (w+7);
}

static void blsquare_dsp(t_blsquare *x, t_signal **sp){
    x->x_n = sp[0]->s_n, x->x_sr_rec = 1.0 / (double)sp[0]->s_sr;
    x->x_ch2 = sp[1]->s_nchans, x->x_ch3 = sp[2]->s_nchans, x->x_ch4 = sp[3]->s_nchans;
    x->x_sig1 = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    int chs = x->x_sig1 ? sp[0]->s_nchans : x->x_list_size;
    if(x->x_nchans != chs){
        x->x_phase = (t_float *)resizebytes(x->x_phase,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_last_phase_offset = (t_float *)resizebytes(x->x_last_phase_offset,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_dir = (t_int *)resizebytes(x->x_dir,
            x->x_nchans * sizeof(t_int), chs * sizeof(t_int));
        x->x_elliptic_blep = (t_elliptic_blep *)resizebytes(x->x_elliptic_blep,
            x->x_nchans * sizeof(t_elliptic_blep), chs * sizeof(t_elliptic_blep));
        for(int i = 0; i < chs; i++) {
            x->x_phase[i] = 0;
            x->x_last_phase_offset[i] = 0;
            x->x_dir[i] = 0;
            elliptic_blep_create(&x->x_elliptic_blep[i], 0, sp[0]->s_sr);
        }
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[4], x->x_nchans);
    if((x->x_ch2 > 1 && x->x_ch2 != x->x_nchans)
    || (x->x_ch3 > 1 && x->x_ch3 != x->x_nchans)
    || (x->x_ch4 > 1 && x->x_ch4 != x->x_nchans)){
        dsp_add_zero(sp[4]->s_vec, x->x_nchans * x->x_n);
        pd_error(x, "[blsquare~]: channel sizes mismatch");
        return;
    }
    dsp_add(blsquare_perform, 6, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
}

static void blsquare_midi(t_blsquare *x, t_floatarg f){
    x->x_midi = (int)(f != 0);
}

static void blsquare_set(t_blsquare *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac != 2)
        return;
    int i = atom_getint(av);
    float f = atom_getint(av+1);
    if(i >= x->x_list_size)
        i = x->x_list_size;
    if(i <= 0)
        i = 1;
    i--;
    x->x_freq_list[i] = f;
}

static void blsquare_list(t_blsquare *x, t_symbol *s, int ac, t_atom * av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    if(x->x_list_size != ac){
        x->x_list_size = ac;
        canvas_update_dsp();
    }
    for(int i = 0; i < ac; i++)
        x->x_freq_list[i] = atom_getfloat(av+i);
}

static void blsquare_soft(t_blsquare *x, t_floatarg f){
    x->x_soft = (int)(f != 0);
}

static void blsquare_free(t_blsquare *x) {
    inlet_free(x->x_inlet_sync);
    inlet_free(x->x_inlet_phase);
    outlet_free(x->x_outlet);
    freebytes(x->x_phase, x->x_nchans * sizeof(*x->x_phase));
    freebytes(x->x_last_phase_offset, x->x_nchans * sizeof(*x->x_last_phase_offset));
    freebytes(x->x_dir, x->x_nchans * sizeof(*x->x_dir));
    freebytes(x->x_elliptic_blep, x->x_nchans * sizeof(*x->x_elliptic_blep));
    free(x->x_freq_list);
}

static void *blsquare_new(t_symbol *s, int ac, t_atom *av){
    t_blsquare *x = (t_blsquare *)pd_new(blsquare_class);
    x->x_ignore = s;
    
    t_float width = 0.5f;
    x->x_midi = x->x_soft = 0;
    x->x_dir = (t_int *)getbytes(sizeof(*x->x_dir));
    x->x_phase = (t_float *)getbytes(sizeof(*x->x_phase));
    x->x_last_phase_offset = (t_float *)getbytes(sizeof(*x->x_last_phase_offset));
    x->x_elliptic_blep = (t_elliptic_blep *)getbytes(sizeof(*x->x_elliptic_blep));
    x->x_freq_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_freq_list[0] = 0;
    x->x_phase[0] = 0;
    x->x_list_size = 1;
    while(ac && av->a_type == A_SYMBOL){
        if(atom_getsymbol(av) == gensym("-midi")){
            x->x_midi = 1;
            ac--, av++;
        }
        else if(atom_getsymbol(av) == gensym("-soft")){
            x->x_soft = 1;
            ac--, av++;
        }
        else if(atom_getsymbol(av) == gensym("-mc")){
            ac--, av++;
            if(!ac || av->a_type != A_FLOAT)
                goto errstate;
            int n = 0;
            while(ac && av->a_type == A_FLOAT){
                x->x_freq_list[n] = atom_getfloat(av);
                ac--, av++, n++;
            }
            x->x_list_size = n;
        }
        else
            goto errstate;
    }
    if(ac && av->a_type == A_FLOAT){
        x->x_freq_list[0] = av->a_w.w_float;
        ac--, av++;
        if(ac && av->a_type == A_FLOAT){
            width = av->a_w.w_float;
            ac--, av++;
            if(ac && av->a_type == A_FLOAT){
                x->x_phase[0] = av->a_w.w_float;
                ac--, av++;
            }
        }
    }
    x->x_inlet_width = inlet_new(&x->x_obj, &x->x_obj.ob_pd,  &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_width, width);
    x->x_inlet_sync = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_sync, 0);
    x->x_inlet_phase = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_phase, x->x_phase[0]);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    x->x_glist = canvas_getcurrent();
    return(x);
errstate:
    post("[blsquare~]: improper args");
    return(NULL);
}

void setup_bl0x2esquare_tilde(void){
    blsquare_class = class_new(gensym("bl.square~"), (t_newmethod)blsquare_new, (t_method)blsquare_free,
        sizeof(t_blsquare), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(blsquare_class, nullfn, gensym("signal"), 0);
    class_addmethod(blsquare_class, (t_method)blsquare_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(blsquare_class, blsquare_list);
    class_addmethod(blsquare_class, (t_method)blsquare_soft, gensym("soft"), A_DEFFLOAT, 0);
    class_addmethod(blsquare_class, (t_method)blsquare_midi, gensym("midi"), A_DEFFLOAT, 0);
    class_addmethod(blsquare_class, (t_method)blsquare_set, gensym("set"), A_GIMME, 0);
}


