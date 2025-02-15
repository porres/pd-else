// Porres 2017-2025

#include <m_pd.h>
#include <math.h>
#include <stdlib.h>
#include "magic.h"
#include "blep.h"

#define MAXLEN 1024

typedef struct _blimp{
    t_object    x_obj;
    t_elliptic_blep *x_elliptic_blep;
    t_float     *x_phase;
    int         x_nchans;
    t_int       x_n;
    t_int       x_sig1;
    t_int       x_midi;
    float      *x_freq_list;
    t_int       x_list_size;
    t_symbol   *x_ignore;
    t_outlet   *x_outlet;
    double      x_sr_rec;
// MAGIC:
    t_glist    *x_glist; // object list
}t_blimp;

static t_class *blimp_class;

static t_int *blimp_perform(t_int *w){
    t_blimp *x = (t_blimp *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    t_elliptic_blep *blep = x->x_elliptic_blep; // Now an array
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0, n = x->x_n; i < n; i++){
            double hz = x->x_sig1 ? in1[j*n + i] : x->x_freq_list[j];
            if(x->x_midi){
                if(hz > 127) hz = 127;
                hz = hz <= 0 ? 0 : pow(2, (hz - 69)/12) * 440;
            }
            double step = hz * x->x_sr_rec;
            step = step > 0.5 ? 0.5 : step < -0.5 ? -0.5 : step;
                    
            *out++ = elliptic_blep_get(&blep[j]);
            
            x->x_phase[j] += step;
        
            elliptic_blep_step(&blep[j]);
            
            if(x->x_phase[j] >= 1 || x->x_phase[j] < 0) {
                x->x_phase[j] = x->x_phase[j] < 0 ? x->x_phase[j]+1 : x->x_phase[j]-1;
                t_float samples_in_past = x->x_phase[j] / step;
                elliptic_blep_add_in_past(&blep[j], 1.0f, 0, samples_in_past);
            }
            else if (x->x_phase[j] >= 0.5f && x->x_phase[j] < 0.5f + step) {
                t_float samples_in_past = (x->x_phase[j] - 0.5f) / step;
                elliptic_blep_add_in_past(&blep[j], -1.0f, 0, samples_in_past);
            }
        }
    }
    return (w+4);
}

static void blimp_dsp(t_blimp *x, t_signal **sp){
    x->x_n = sp[0]->s_n, x->x_sr_rec = 1.0 / (double)sp[0]->s_sr;
    x->x_sig1 = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    int chs = x->x_sig1 ? sp[0]->s_nchans : x->x_list_size;
    if(x->x_nchans != chs){
        x->x_phase = (t_float *)resizebytes(x->x_phase,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_elliptic_blep = (t_elliptic_blep *)resizebytes(x->x_elliptic_blep,
            x->x_nchans * sizeof(t_elliptic_blep), chs * sizeof(t_elliptic_blep));
        for(int i = 0; i < chs; i++) {
            x->x_phase[i] = 0;
            elliptic_blep_create(&x->x_elliptic_blep[i], 0, sp[0]->s_sr);
        }
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(blimp_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
}

static void blimp_midi(t_blimp *x, t_floatarg f){
    x->x_midi = (int)(f != 0);
}

static void blimp_set(t_blimp *x, t_symbol *s, int ac, t_atom *av){
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

static void blimp_list(t_blimp *x, t_symbol *s, int ac, t_atom * av){
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

static void blimp_free(t_blimp *x) {
    outlet_free(x->x_outlet);
    freebytes(x->x_phase, x->x_nchans * sizeof(*x->x_phase));
    freebytes(x->x_elliptic_blep, x->x_nchans * sizeof(*x->x_elliptic_blep));
    free(x->x_freq_list);
}

static void *blimp_new(t_symbol *s, int ac, t_atom *av){
    t_blimp *x = (t_blimp *)pd_new(blimp_class);
    x->x_ignore = s;
    x->x_midi = 0;
    x->x_phase = (t_float *)getbytes(sizeof(*x->x_phase));
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
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    x->x_glist = canvas_getcurrent();
    return(x);
errstate:
    post("[blimp~]: improper args");
    return(NULL);
}

void setup_bl0x2eimp2_tilde(void){
    blimp_class = class_new(gensym("bl.imp2~"), (t_newmethod)blimp_new, (t_method)blimp_free,
        sizeof(t_blimp), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(blimp_class, nullfn, gensym("signal"), 0);
    class_addmethod(blimp_class, (t_method)blimp_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(blimp_class, blimp_list);
    class_addmethod(blimp_class, (t_method)blimp_midi, gensym("midi"), A_DEFFLOAT, 0);
    class_addmethod(blimp_class, (t_method)blimp_set, gensym("set"), A_GIMME, 0);
}
