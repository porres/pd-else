// Porres 2017-2024

#include <m_pd.h>
#include <stdlib.h>
#include "random.h"
#include "magic.h"

#define MAXLEN 1024

static t_class *randpulse_class;

typedef struct _randpulse{
    t_object       x_obj;
    t_random_state x_rstate;
    int            x_id;
    int            x_nchans;
    int            x_ch;
    int            x_n;
    t_int          x_ch2;
    t_int          x_ch3;
    t_int          x_sig1;
    float         *x_freq_list;
    t_float        x_sr_rec;
    t_float        x_freq;
    t_int          x_list_size;
    double        *x_phase;
    t_float       *x_random;
    t_symbol      *x_ignore;
    t_inlet       *x_inlet_width;
    t_inlet       *x_inlet_sync;
    // MAGIC:
    t_glist    *x_glist; // object list
    /*    t_float    *x_signalscalar; // right inlet's float field
    t_float     x_phase_sync_float; // float from magic*/
}t_randpulse;

static void randpulse_seed(t_randpulse *x, t_symbol *s, int ac, t_atom *av){
    for(int i = 0; i < x->x_nchans; i++)
        x->x_phase[i] = x->x_freq_list[i] >= 0 ? 1.0 : 0.0;
    random_init(&x->x_rstate, get_seed(s, ac, av, x->x_id));
}

static void randpulse_ch(t_randpulse *x, t_floatarg f){
    x->x_ch = f < 1 ? 1 : (int)f;
    canvas_update_dsp();
}

static void randpulse_list(t_randpulse *x, t_symbol *s, int ac, t_atom * av){
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

static void randpulse_set(t_randpulse *x, t_symbol *s, int ac, t_atom *av){
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

static t_int *randpulse_perform(t_int *w){
    t_randpulse *x = (t_randpulse *)(w[1]);
    int chs = (t_int)(w[2]); // number of channels in main input signal (density)
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *in3 = (t_float *)(w[5]);
    t_float *out = (t_sample *)(w[6]);
    uint32_t *s1 = &x->x_rstate.s1;
    uint32_t *s2 = &x->x_rstate.s2;
    uint32_t *s3 = &x->x_rstate.s3;
    double *phase = x->x_phase;
    t_float *random = x->x_random;
    double output;
    for(int i = 0, n = x->x_n; i < n; i++){
        for(int j = 0; j < x->x_nchans; j++){ // for 'n' out channels
            t_float hz;
            if(x->x_sig1)
                hz = chs == 1 ? in1[i] : in1[j*n + i];
            else{
                if(chs == 1)
                    hz = x->x_freq_list[0];
                else
                    hz = x->x_freq_list[j];
            }
            double width = x->x_ch2 == 1 ? in2[i] : in2[j*n + i];
            width = width > 1. ? 1. : width < 0. ? 0. : width; // clipped
            double sync = x->x_ch3 == 1 ? in3[i] : in3[j*n + i];
            double phase_step = hz * x->x_sr_rec;
            phase_step = phase_step > 0.5 ? 0.5 : phase_step < -0.5 ? -0.5 : phase_step;
            if(hz >= 0){
                if(sync == 1)
                    phase[j] = 1;
                if(phase[j] >= 1.){  // update
                    random[j] = (t_float)(random_frand(s1, s2, s3));
                    phase[j] = phase[j] - 1;
                    output = random[j]; // first is always random
                }
                else if(phase[j] + phase_step >= 1)
                    output = 0; // last sample is always 0
                else
                    output = phase[j] <= width ? random[j] : 0;
            }
            else{
                if(sync == 1)
                    phase[j] = 0;
                if(phase[j] <= 0.){ // update
                    random[j] = (t_float)(random_frand(s1, s2, s3));
                    phase[j] += 1;
                    output = 0;
                }
                else if(phase[j] + phase_step <= 0)
                    output = random[j]; // last sample is always 1
                else
                    output = phase[j] <= width ? random[j] : 0;
            }
            out[j*x->x_n + i] = output;
            phase[j] += phase_step; // next phase
        }
    }
    x->x_phase = phase;
    x->x_random = random;
    return(w+7);
}

static void randpulse_dsp(t_randpulse *x, t_signal **sp){
    x->x_sr_rec = 1./sp[0]->s_sr;
    x->x_n = sp[0]->s_n;
    x->x_sig1 = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    x->x_ch2 = sp[1]->s_nchans, x->x_ch3 = sp[2]->s_nchans;
    int chs = x->x_sig1 ? sp[0]->s_nchans : x->x_list_size;
    int nchans = chs;
    if(chs == 1)
        chs = x->x_ch;
    if(x->x_nchans != chs){
        x->x_phase = (double *)resizebytes(x->x_phase,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_random = (t_float *)resizebytes(x->x_random,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[3], x->x_nchans);
    if((x->x_ch2 > 1 && x->x_ch2 != x->x_nchans)
    || (x->x_ch3 > 1 && x->x_ch3 != x->x_nchans)){
        dsp_add_zero(sp[3]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[randpulse~]: channel sizes mismatch");
        return;
    }
    dsp_add(randpulse_perform, 6, x, nchans, sp[0]->s_vec,
        sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *randpulse_free(t_randpulse *x){
    inlet_free(x->x_inlet_width);
    inlet_free(x->x_inlet_sync);
    freebytes(x->x_phase, x->x_nchans * sizeof(*x->x_phase));
    freebytes(x->x_random, x->x_nchans * sizeof(*x->x_random));
    free(x->x_freq_list);
    return(void *)x;
}

static void *randpulse_new(t_symbol *s, int ac, t_atom *av){
    t_randpulse *x = (t_randpulse *)pd_new(randpulse_class);
    x->x_id = random_get_id();
    x->x_nchans = 1;
    x->x_ch = 1;
    t_float width = 0.5f;
    x->x_phase = (double *)getbytes(sizeof(*x->x_phase));
    x->x_freq_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_random = (t_float *)getbytes(sizeof(*x->x_random));
    x->x_freq_list[0] = x->x_phase[0] = x->x_random[0] = 0;
    x->x_list_size = 1;
    randpulse_seed(x, s, 0, NULL);
    if(ac){
        while(av->a_type == A_SYMBOL){
            if(atom_getsymbol(av) == gensym("-seed")){
                if(ac >= 2){
                    t_atom at[1];
                    SETFLOAT(at, atom_getfloat(av+1));
                    ac-=2, av+=2;
                    randpulse_seed(x, s, 1, at);
                }
                else{
                    pd_error(x, "[randpulse~]: -seed needs a seed value");
                    return(NULL);
                }
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
            else if(atom_getsymbol(av) == gensym("-ch")){
                if(ac >= 2){
                    int n = atom_getint(av+1);
                    x->x_ch = n < 1 ? 1 : n;
                    ac-=2, av+=2;
                }
                else{
                    pd_error(x, "[randpulse~]: -ch needs a channel number value");
                    return(NULL);
                }
            }
            else{
                pd_error(x, "[randpulse~]: improper flag (%s)", atom_getsymbol(av)->s_name);
                return(NULL);
            }
        }
    }
    if(ac && av->a_type == A_FLOAT){
        x->x_freq_list[0] = av->a_w.w_float;
        ac--, av++;
        if(ac && av->a_type == A_FLOAT){
            width = av->a_w.w_float;
            ac--, av++;
        }
    }
    x->x_glist = canvas_getcurrent();
    x->x_inlet_width = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_width, width);
    x->x_inlet_sync = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_sync, 0);
    outlet_new(&x->x_obj, &s_signal);
    return(x);
errstate:
    post("[randpulse~]: improper args");
    return(NULL);
}

void randpulse_tilde_setup(void){
    randpulse_class = class_new(gensym("randpulse~"), (t_newmethod)randpulse_new,
        (t_method)randpulse_free, sizeof(t_randpulse), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(randpulse_class, nullfn, gensym("signal"), 0);
    class_addmethod(randpulse_class, (t_method)randpulse_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(randpulse_class, randpulse_list);
    class_addmethod(randpulse_class, (t_method)randpulse_seed, gensym("seed"), A_GIMME, 0);
    class_addmethod(randpulse_class, (t_method)randpulse_ch, gensym("ch"), A_DEFFLOAT, 0);
    class_addmethod(randpulse_class, (t_method)randpulse_set, gensym("set"), A_GIMME, 0);
}
