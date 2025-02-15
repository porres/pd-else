// Porres 2017-2025

#include <m_pd.h>
#include <math.h>
#include <stdlib.h>
#include "magic.h"
#include "blep.h"

#define MAXLEN 1024

typedef struct _blsaw{
    t_object    x_obj;
    t_elliptic_blep *x_elliptic_blep;
    t_float     *x_phase;
    t_float     *x_last_phase_offset;
    int         x_nchans;
    t_int       x_n;
    t_int       x_sig1;
    t_int       x_ch2;
    t_int       x_ch3;
    t_int       x_midi;
    t_int       x_soft;
    t_int      *x_dir;
    float      *x_freq_list;
    t_int       x_list_size;
    t_symbol   *x_ignore;
    t_inlet    *x_inlet_phase;
    t_inlet    *x_inlet_sync;
    t_outlet   *x_outlet;
    double      x_sr_rec;
// MAGIC:
    t_glist    *x_glist; // object list
}t_blsaw;

static t_class *blsaw_class;

double blsaw_wrap_phase(double phase){
    while(phase >= 1)
        phase -= 1.;
    while(phase < 0)
        phase += 1.;
    return(phase);
}

static t_int *blsaw_perform(t_int *w){
    t_blsaw *x = (t_blsaw *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *in3 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
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
            t_float trig = x->x_ch2 == 1 ? in2[i] : in2[j*n + i];
            double phase_offset = x->x_ch3 == 1 ? in3[i] : in3[j*n + i];
            double phase_dev = phase_offset - x->x_last_phase_offset[j];
            x->x_last_phase_offset[j] = phase_offset;
            double last_phase = phase[j];
            double step = hz * x->x_sr_rec;
            step = step > 0.5 ? 0.5 : step < -0.5 ? -0.5 : step;
            if(dir[j] == 0) // initialize this just once
                dir[j] = 1;
            if(trig > 0 && trig <= 1 && x->x_soft)
                dir[j] = dir[j] == 1 ? -1 : 1;
            step *= dir[j];
            out[j*n + i] = (blsaw_wrap_phase(phase[j]) * -2.0f + 1.0f) + elliptic_blep_get(&blep[j]);
            phase[j] += (step + phase_dev);
            elliptic_blep_step(&blep[j]);
            if(trig > 0 && trig <= 1 && !x->x_soft)
                phase[j] = trig;
            if(phase[j] >= 1 || phase[j] < 0){
                t_float phase_step = blsaw_wrap_phase(phase[j] - last_phase);
                t_float amp_step = (blsaw_wrap_phase(phase[j]) * -2.0f + 1.0f) - (blsaw_wrap_phase(last_phase) * -2.0f + 1.0f);
                phase[j] = blsaw_wrap_phase(phase[j]);
                t_float samples_in_past = phase[j] / phase_step;
                elliptic_blep_add_in_past(&blep[j], amp_step, 1, samples_in_past < 1.0 ? samples_in_past : 0.999999);
            }
        }
    }
    return(w+6);
}

static void blsaw_dsp(t_blsaw *x, t_signal **sp){
    x->x_n = sp[0]->s_n, x->x_sr_rec = 1.0 / (double)sp[0]->s_sr;
    x->x_ch2 = sp[1]->s_nchans, x->x_ch3 = sp[2]->s_nchans;
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
    signal_setmultiout(&sp[3], x->x_nchans);
    if((x->x_ch2 > 1 && x->x_ch2 != x->x_nchans)
    || (x->x_ch3 > 1 && x->x_ch3 != x->x_nchans)){
        dsp_add_zero(sp[3]->s_vec, x->x_nchans * x->x_n);
        pd_error(x, "[blsaw~]: channel sizes mismatch");
        return;
    }
    dsp_add(blsaw_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void blsaw_midi(t_blsaw *x, t_floatarg f){
    x->x_midi = (int)(f != 0);
}

static void blsaw_set(t_blsaw *x, t_symbol *s, int ac, t_atom *av){
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

static void blsaw_list(t_blsaw *x, t_symbol *s, int ac, t_atom * av){
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

static void blsaw_soft(t_blsaw *x, t_floatarg f){
    x->x_soft = (int)(f != 0);
}

static void blsaw_free(t_blsaw *x) {
    inlet_free(x->x_inlet_sync);
    inlet_free(x->x_inlet_phase);
    outlet_free(x->x_outlet);
    freebytes(x->x_phase, x->x_nchans * sizeof(*x->x_phase));
    freebytes(x->x_last_phase_offset, x->x_nchans * sizeof(*x->x_last_phase_offset));
    freebytes(x->x_dir, x->x_nchans * sizeof(*x->x_dir));
    freebytes(x->x_elliptic_blep, x->x_nchans * sizeof(*x->x_elliptic_blep));
    free(x->x_freq_list);
}

static void *blsaw_new(t_symbol *s, int ac, t_atom *av){
    t_blsaw *x = (t_blsaw *)pd_new(blsaw_class);
    x->x_ignore = s;
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
            x->x_phase[0] = av->a_w.w_float;
            ac--, av++;
        }
    }
    x->x_inlet_sync = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_sync, 0);
    x->x_inlet_phase = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_phase, x->x_phase[0]);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    x->x_glist = canvas_getcurrent();
    return(x);
errstate:
    post("[bl.saw~]: improper args");
    return(NULL);
}

void setup_bl0x2esaw_tilde(void){
    blsaw_class = class_new(gensym("bl.saw~"), (t_newmethod)blsaw_new, (t_method)blsaw_free,
        sizeof(t_blsaw), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(blsaw_class, nullfn, gensym("signal"), 0);
    class_addmethod(blsaw_class, (t_method)blsaw_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(blsaw_class, blsaw_list);
    class_addmethod(blsaw_class, (t_method)blsaw_soft, gensym("soft"), A_DEFFLOAT, 0);
    class_addmethod(blsaw_class, (t_method)blsaw_midi, gensym("midi"), A_DEFFLOAT, 0);
    class_addmethod(blsaw_class, (t_method)blsaw_set, gensym("set"), A_GIMME, 0);
}
