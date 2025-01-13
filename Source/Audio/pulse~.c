// Porres 2017-2023

#include <m_pd.h>
#include <stdlib.h>
#include <math.h>
#include "magic.h"

#define MAXLEN 1024

typedef struct _pulse{
    t_object    x_obj;
    double     *x_phase;
    double     *x_lastoffset;
    int         x_nchans;
    t_int       x_n;
    t_int       x_sig1;
    t_int       x_sig2;
    t_int       x_ch2;
    t_int       x_ch3;
    t_int       x_ch4;
    t_int       x_midi;
    t_int       x_soft;
    t_int      *x_dir;
    float      *x_freq_list;
    t_int       x_list_size;
    t_symbol   *x_ignore;
    t_inlet    *x_inlet_width;
    t_inlet    *x_inlet_phase;
    t_inlet    *x_inlet_sync;
    t_outlet   *x_outlet;
    double      x_sr_rec;
// MAGIC:
    t_glist    *x_glist; // object list
    t_float    *x_signalscalar; // right inlet's float field
    t_float     x_phase_sync_float; // float from magic
}t_pulse;

static t_class *pulse_class;

static t_int *pulse_perform(t_int *w){
    t_pulse *x = (t_pulse *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *in3 = (t_float *)(w[4]);
    t_float *in4 = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);
    t_int *dir = x->x_dir;
    double *phase = x->x_phase;
    double *lastoffset = x->x_lastoffset;
// Magic Start
    if(!x->x_sig2){
        t_float *scalar = x->x_signalscalar;
        if(!else_magic_isnan(*x->x_signalscalar)){
            t_float input_phase = fmod(*scalar, 1);
            if(input_phase < 0)
                input_phase += 1;
            for(int j = 0; j < x->x_nchans; j++)
                x->x_phase[j] = input_phase;
            else_magic_setnan(x->x_signalscalar);
        }
    }
// Magic End
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0, n = x->x_n; i < n; i++){
            double hz = x->x_sig1 ? in1[j*n + i] : x->x_freq_list[j];
            if(x->x_midi){
                if(hz > 127)
                    hz = 127;
                hz = hz <= 0 ? 0 : pow(2, (hz - 69)/12) * 440;
            }
            double step = hz * x->x_sr_rec; // phase step
            step = step > 1 ? 1 : step < -1 ? -1 : step; // clipped phase_step
            double width = x->x_ch2 == 1 ? in2[i] : in2[j*n + i];
            double phase_offset = x->x_ch4 == 1 ? in4[i] : in4[j*n + i];
            double phase_dev = phase_offset - lastoffset[j];
            if(phase_dev >= 1 || phase_dev <= -1)
                phase_dev = fmod(phase_dev, 1); // fmod(phase_dev)
            if(x->x_soft){
                if(dir[j] == 0)
                    dir[j] = 1;
                step *= (dir[j]);
            }
            if(x->x_sig2){
                t_float trig = x->x_ch3 == 1 ? in3[i] : in3[j*n + i];
                if(trig > 0 && trig <= 1){ // if sync
                    if(x->x_soft)
                        dir[j] = dir[j] == 1 ? -1 : 1;
                    else
                        phase[j] = trig;
                }
            }
            phase[j] += phase_dev;
            if(hz >= 0){ // POSITIVE freq
                if(phase[j] < 0)
                    phase[j] += 1.;
                if(phase[j] >= 1){ // 1st is always 1
                    out[j*n + i] = 1;
                    phase[j] -= 1;
                }
                else if((phase[j] + step) >= 1)
                    out[j*n + i] = 0; // last sample is always 0
                else
                    out[j*n + i] = phase[j] < width;
            }
            else{ // negative freq
                if(phase[j] >= 1)
                    phase[j] -= 1.;
                if(phase[j] <= 0){
                    out[j*n + i] = 0; // 1st sample is always 0
                    phase[j] += 1;
                }
                else if(phase[j] + step <= 0)
                    out[j*n + i]  = 1; // last sample is always 1
                else
                    out[j*n + i] = phase[j] < width;
            }
            phase[j] += step; // next phase
            lastoffset[j] = phase_offset; // last phase offset
        }
    }
    x->x_phase = phase;
    x->x_lastoffset = lastoffset;
    x->x_dir = dir;
    return(w+7);
}

static void pulse_dsp(t_pulse *x, t_signal **sp){
    x->x_n = sp[0]->s_n, x->x_sr_rec = 1.0 / (double)sp[0]->s_sr;
    x->x_ch2 = sp[1]->s_nchans, x->x_ch3 = sp[2]->s_nchans, x->x_ch4 = sp[3]->s_nchans;
    x->x_sig1 = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    x->x_sig2 = else_magic_inlet_connection((t_object *)x, x->x_glist, 2, &s_signal);
    int chs = x->x_sig1 ? sp[0]->s_nchans : x->x_list_size;
    if(x->x_nchans != chs){
        x->x_phase = (double *)resizebytes(x->x_phase,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_lastoffset = (double *)resizebytes(x->x_lastoffset,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_dir = (t_int *)resizebytes(x->x_dir,
            x->x_nchans * sizeof(t_int), chs * sizeof(t_int));
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[4], x->x_nchans);
    if((x->x_ch2 > 1 && x->x_ch2 != x->x_nchans)
    || (x->x_ch3 > 1 && x->x_ch3 != x->x_nchans)
    || (x->x_ch4 > 1 && x->x_ch4 != x->x_nchans)){
        dsp_add_zero(sp[4]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[pulse~]: channel sizes mismatch");
        return;
    }
    dsp_add(pulse_perform, 6, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
}

static void pulse_midi(t_pulse *x, t_floatarg f){
    x->x_midi = (int)(f != 0);
}

static void pulse_soft(t_pulse *x, t_floatarg f){
    x->x_soft = (int)(f != 0);
}

static void pulse_list(t_pulse *x, t_symbol *s, int ac, t_atom * av){
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

static void pulse_set(t_pulse *x, t_symbol *s, int ac, t_atom *av){
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

static void *pulse_free(t_pulse *x){
    inlet_free(x->x_inlet_width);
    inlet_free(x->x_inlet_sync);
    inlet_free(x->x_inlet_phase);
    outlet_free(x->x_outlet);
    freebytes(x->x_phase, x->x_nchans * sizeof(*x->x_phase));
    freebytes(x->x_lastoffset, x->x_nchans * sizeof(*x->x_lastoffset));
    freebytes(x->x_dir, x->x_nchans * sizeof(*x->x_dir));
    free(x->x_freq_list);
    return(void *)x;
}

static void *pulse_new(t_symbol *s, int ac, t_atom *av){
    t_pulse *x = (t_pulse *)pd_new(pulse_class);
    x->x_ignore = s;
    x->x_midi = x->x_soft = 0;
    t_float width = 0.5f;
    x->x_dir = (t_int *)getbytes(sizeof(*x->x_dir));
    x->x_phase = (double *)getbytes(sizeof(*x->x_phase));
    x->x_lastoffset = (double *)getbytes(sizeof(*x->x_lastoffset));
    x->x_freq_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_freq_list[0] = x->x_phase[0] = x->x_lastoffset[0] = 0;
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
    x->x_inlet_width = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_width, width);
    x->x_inlet_sync = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_sync, 0);
    x->x_inlet_phase = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_phase, x->x_phase[0]);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
// Magic
    x->x_glist = canvas_getcurrent();
    x->x_signalscalar = obj_findsignalscalar((t_object *)x, 2);
    return(x);
errstate:
    post("[pulse~]: improper args");
    return(NULL);
}

void pulse_tilde_setup(void){
    pulse_class = class_new(gensym("pulse~"), (t_newmethod)pulse_new, (t_method)pulse_free,
        sizeof(t_pulse), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(pulse_class, nullfn, gensym("signal"), 0);
    class_addmethod(pulse_class, (t_method)pulse_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(pulse_class, pulse_list);
    class_addmethod(pulse_class, (t_method)pulse_soft, gensym("soft"), A_DEFFLOAT, 0);
    class_addmethod(pulse_class, (t_method)pulse_midi, gensym("midi"), A_DEFFLOAT, 0);
    class_addmethod(pulse_class, (t_method)pulse_set, gensym("set"), A_GIMME, 0);
}
