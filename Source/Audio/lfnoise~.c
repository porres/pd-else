// Porres 2017-2026

#include <m_pd.h>
#include <random.h>
#include <stdlib.h>
#include "magic.h"

#define MAXLEN 1024

static t_class *lfnoise_class;

typedef struct _lfnoise{
    t_object        x_obj;
    t_random_state  x_rstate;
    t_int           x_interp;
    t_inlet        *x_inlet_sync;
    double          x_i_sr;
    int             x_id;
    double         *x_phase;
    t_float        *x_ynp1;
    t_float        *x_yn;
    float          *x_freq_list;
    t_int           x_list_size;
    t_int           x_sig1;
    int             x_nchans;
    int             x_ch;
    int             x_ch2;
    int             x_n;
    t_glist        *x_glist;
}t_lfnoise;

static void lfnoise_interp(t_lfnoise *x, t_floatarg f){
    x->x_interp = (int)f != 0;
}

static void lfnoise_seed(t_lfnoise *x, t_symbol *s, int ac, t_atom *av){
    random_init(&x->x_rstate, get_seed(s, ac, av, x->x_id));
    uint32_t *s1 = &x->x_rstate.s1;
    uint32_t *s2 = &x->x_rstate.s2;
    uint32_t *s3 = &x->x_rstate.s3;
    for(int i = 0; i < x->x_nchans; i++){
        x->x_phase[i] = 0.0f;
        x->x_yn[i] = (t_float)(random_frand(s1, s2, s3));
        x->x_ynp1[i] = (t_float)(random_frand(s1, s2, s3));
    }
}

static void lfnoise_ch(t_lfnoise *x, t_floatarg f){
    x->x_ch = f < 1 ? 1 : (int)f;
    canvas_update_dsp();
}

static void lfnoise_list(t_lfnoise *x, t_symbol *s, int ac, t_atom * av){
    (void)s;
    if(ac == 0)
        return;
    if(x->x_list_size != ac){
        x->x_list_size = ac;
        canvas_update_dsp();
    }
    for(int i = 0; i < ac; i++)
        x->x_freq_list[i] = atom_getfloat(av+i);
}

static void lfnoise_set(t_lfnoise *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
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

static t_int *lfnoise_perform(t_int *w){
    t_lfnoise *x = (t_lfnoise *)(w[1]);
    int chs = (t_int)(w[2]); // number of channels in main input signal
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out = (t_sample *)(w[5]);
    uint32_t *s1 = &x->x_rstate.s1;
    uint32_t *s2 = &x->x_rstate.s2;
    uint32_t *s3 = &x->x_rstate.s3;
    double *phase = x->x_phase;
    t_float *ynp1 = x->x_ynp1;
    t_float *yn = x->x_yn;
    t_int interp = x->x_interp;
    double isr = x->x_i_sr;
    int n = x->x_n, ch2 = x->x_ch2;
    for(int i = 0; i < n; i++){
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
            float sync = ch2 == 1 ? in2[i] : in2[j*n + i];
            double phase_step = hz * isr;
    // clipped phase_step
            phase_step = phase_step > 1 ? 1. : phase_step < -1 ? -1 : phase_step;
            t_float random;
            if(hz >= 0){
                if(sync == 1)
                    phase[j] = 1;
                if(phase[j] >= 1.){ // update
                    random = (t_float)(random_frand(s1, s2, s3));
                    phase[j] = phase[j] - 1;
                    yn[j] = ynp1[j];
                    ynp1[j] = random; // next random value
                }
            }
            else{
                if(sync == 1)
                    phase[j] = 0;
                if(phase[j] <= 0.){ // update
                    random = (t_float)(random_frand(s1, s2, s3));
                    phase[j] = phase[j] + 1;
                    yn[j] = ynp1[j];
                    ynp1[j] = random; // next random value
                }
            }
            if(interp){
                if(hz >= 0)
                    out[j*n + i] = yn[j] + (ynp1[j] - yn[j]) * (phase[j]);
                else
                    out[j*n + i] = yn[j] + (ynp1[j] - yn[j]) * (1 - phase[j]);
            }
            else
                out[j*n + i] = yn[j];
            phase[j] += phase_step;
        }
    }
    x->x_phase = phase;
    x->x_ynp1 = ynp1; // next random value
    x->x_yn = yn; // current output
    return(w+6);
}

static void lfnoise_dsp(t_lfnoise *x, t_signal **sp){
    x->x_i_sr = 1.0/sp[0]->s_sr;
    x->x_n = sp[0]->s_n;
    x->x_sig1 = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    int chs = x->x_sig1 ? sp[0]->s_nchans : x->x_list_size;
    x->x_ch2 = sp[1]->s_nchans;
    int nchans = chs;
    if(chs == 1)
        chs = x->x_ch;
    int old_chs = x->x_nchans;
    if(old_chs != chs){
        x->x_phase = (double *)resizebytes(x->x_phase,
            old_chs * sizeof(double), chs * sizeof(double));
        x->x_ynp1 = (t_float *)resizebytes(x->x_ynp1,
            old_chs * sizeof(t_float), chs * sizeof(t_float));
        x->x_yn = (t_float *)resizebytes(x->x_yn,
            old_chs * sizeof(t_float), chs * sizeof(t_float));
        if(chs > old_chs){
            uint32_t *s1 = &x->x_rstate.s1;
            uint32_t *s2 = &x->x_rstate.s2;
            uint32_t *s3 = &x->x_rstate.s3;
            for(int i = old_chs; i < chs; i++){
                x->x_phase[i] = 0.;
                x->x_yn[i] = (t_float)random_frand(s1, s2, s3);
                x->x_ynp1[i] = (t_float)random_frand(s1, s2, s3);
            }
        }
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[2], x->x_nchans);
    if(x->x_ch2 > 1 && x->x_ch2 != x->x_nchans){
        dsp_add_zero(sp[2]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[lfnoise~]: channel sizes mismatch");
        return;
    }
    dsp_add(lfnoise_perform, 5, x, nchans, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *lfnoise_free(t_lfnoise *x){
    freebytes(x->x_phase, x->x_nchans * sizeof(*x->x_phase));
    freebytes(x->x_yn, x->x_nchans * sizeof(*x->x_yn));
    freebytes(x->x_ynp1, x->x_nchans * sizeof(*x->x_ynp1));
    inlet_free(x->x_inlet_sync);
    return(void *)x;
}


static void *lfnoise_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_lfnoise *x = (t_lfnoise *)pd_new(lfnoise_class);
    x->x_id = random_get_id();
    
    x->x_nchans = 1;
    x->x_ch = 1;
    x->x_freq_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_phase = (double *)getbytes(sizeof(*x->x_phase));
    x->x_yn = (t_float *)getbytes(sizeof(*x->x_yn));
    x->x_ynp1 = (t_float *)getbytes(sizeof(*x->x_ynp1));
    x->x_freq_list[0] = x->x_phase[0] = x->x_ynp1[0] = x->x_yn[0] = 0.;
    x->x_list_size = 1;
    
    lfnoise_seed(x, s, 0, NULL);
// default parameters
    t_int interp = 0;
    if(ac){
        while(av->a_type == A_SYMBOL){
            if(atom_getsymbol(av) == gensym("-seed")){
                if(ac >= 2){
                    t_atom at[1];
                    SETFLOAT(at, atom_getfloat(av+1));
                    ac-=2, av+=2;
                    lfnoise_seed(x, s, 1, at);
                }
                else{
                    pd_error(x, "[lfnoise~]: -seed needs a seed value");
                    return(NULL);
                }
            }
            else if(atom_getsymbol(av) == gensym("-mc")){
                ac--, av++;
                if(!ac || av->a_type != A_FLOAT){
                    pd_error(x, "[lfnoise~]: -mc needs a frequency value");
                    return(NULL);
                }
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
                    lfnoise_ch(x, n < 1 ? 1 : n);
                    ac-=2, av+=2;
                }
                else{
                    pd_error(x, "[lfnoise~]: -ch needs a channel number value");
                    return(NULL);
                }
            }
            else{
                pd_error(x, "[lfnoise~]: improper flag (%s)", atom_getsymbol(av)->s_name);
                return(NULL);
            }
        }
    }
    if(ac){
        x->x_freq_list[0] = atom_getfloat(av);
        ac--, av++;
    }
    if(ac && av->a_type == A_FLOAT)
        interp = atom_getfloat(av) != 0;
    x->x_interp = interp;
// in/out
    x->x_inlet_sync = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_sync, 0);
    outlet_new(&x->x_obj, &s_signal);
    x->x_glist = canvas_getcurrent();
    return(x);
}

void lfnoise_tilde_setup(void){
    lfnoise_class = class_new(gensym("lfnoise~"), (t_newmethod)lfnoise_new,
        (t_method)lfnoise_free, sizeof(t_lfnoise), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(lfnoise_class, nullfn, gensym("signal"), 0);
    class_addmethod(lfnoise_class, (t_method)lfnoise_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(lfnoise_class, lfnoise_list);
    class_addmethod(lfnoise_class, (t_method)lfnoise_seed, gensym("seed"), A_GIMME, 0);
    class_addmethod(lfnoise_class, (t_method)lfnoise_interp, gensym("interp"), A_DEFFLOAT, 0);
    class_addmethod(lfnoise_class, (t_method)lfnoise_ch, gensym("ch"), A_DEFFLOAT, 0);
    class_addmethod(lfnoise_class, (t_method)lfnoise_set, gensym("set"), A_GIMME, 0);
}
