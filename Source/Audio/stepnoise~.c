// Porres 2017-2024

#include <m_pd.h>
#include "random.h"
#include <stdlib.h>
#include "magic.h"

#define MAXLEN 1024

static t_class *stepnoise_class;

typedef struct _stepnoise{
    t_object       x_obj;
    t_random_state x_rstate;
    int            x_id;
    int            x_nchans;
    int            x_ch;
    int            x_n;
    t_int          x_sig1;
    float         *x_freq_list;
    t_float        x_sr_rec;
    t_int          x_list_size;
    double        *x_phase;
    t_float       *x_val;
    t_symbol      *x_ignore;
    t_glist       *x_glist;
}t_stepnoise;

static void stepnoise_seed(t_stepnoise *x, t_symbol *s, int ac, t_atom *av){
    random_init(&x->x_rstate, get_seed(s, ac, av, x->x_id));
    uint32_t *s1 = &x->x_rstate.s1;
    uint32_t *s2 = &x->x_rstate.s2;
    uint32_t *s3 = &x->x_rstate.s3;
    for(int i = 0; i < x->x_nchans; i++){
        x->x_phase[i] = 0.0f;
        x->x_val[i] = (t_float)(random_frand(s1, s2, s3));
    }
    
}

static void stepnoise_ch(t_stepnoise *x, t_floatarg f){
    x->x_ch = f < 1 ? 1 : (int)f;
    canvas_update_dsp();
}

static void stepnoise_list(t_stepnoise *x, t_symbol *s, int ac, t_atom * av){
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

static void stepnoise_set(t_stepnoise *x, t_symbol *s, int ac, t_atom *av){
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

static t_int *stepnoise_perform(t_int *w){
    t_stepnoise *x = (t_stepnoise *)(w[1]);
    int chs = (t_int)(w[2]); // number of channels in main input signal (density)
    t_float *in = (t_float *)(w[3]);
    t_float *out = (t_sample *)(w[4]);
    uint32_t *s1 = &x->x_rstate.s1;
    uint32_t *s2 = &x->x_rstate.s2;
    uint32_t *s3 = &x->x_rstate.s3;
    t_float *random = x->x_val;
    double *phase = x->x_phase;
    for(int i = 0, n = x->x_n; i < n; i++){
        for(int j = 0; j < x->x_nchans; j++){ // for 'n' out channels
            t_float hz;
            if(x->x_sig1)
                hz = chs == 1 ? in[i] : in[j*n + i];
            else{
                if(chs == 1)
                    hz = x->x_freq_list[0];
                else
                    hz = x->x_freq_list[j];
            }
            double phase_step = hz * x->x_sr_rec;
            phase_step = phase_step > 1 ? 1. : phase_step < -1 ? -1 : phase_step;
            if(hz >= 0){
                if(phase[j] >= 1.){  // update
                    random[j] = (t_float)(random_frand(s1, s2, s3));
                    phase[j] -= 1;
                }
            }
            else{
                if(phase[j] <= 0.){ // update
                    random[j] = (t_float)(random_frand(s1, s2, s3));
                    phase[j] += 1;
                }
            }
            out[j*x->x_n + i] = random[j];
            phase[j] += phase_step; // next phase
        }
    }
    x->x_phase = phase;
    x->x_val = random;
    return(w+5);
}

static void stepnoise_dsp(t_stepnoise *x, t_signal **sp){
    x->x_sr_rec = 1./sp[0]->s_sr;
    x->x_n = sp[0]->s_n;
    x->x_sig1 = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    int chs = x->x_sig1 ? sp[0]->s_nchans : x->x_list_size;
    int nchans = chs;
    if(chs == 1)
        chs = x->x_ch;
    if(x->x_nchans != chs){
        x->x_phase = (double *)resizebytes(x->x_phase,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_val = (t_float *)resizebytes(x->x_val,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(stepnoise_perform, 4, x, nchans, sp[0]->s_vec, sp[1]->s_vec);
}

static void *stepnoise_free(t_stepnoise *x){
    freebytes(x->x_phase, x->x_nchans * sizeof(*x->x_phase));
    freebytes(x->x_val, x->x_nchans * sizeof(*x->x_val));
    free(x->x_freq_list);
    return(void *)x;
}

static void *stepnoise_new(t_symbol *s, int ac, t_atom *av){
    t_stepnoise *x = (t_stepnoise *)pd_new(stepnoise_class);
    x->x_id = random_get_id();
    x->x_nchans = 1;
    x->x_ch = 1;
    x->x_freq_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_phase = (double *)getbytes(sizeof(*x->x_phase));
    x->x_val = (t_float *)getbytes(sizeof(*x->x_val));
    x->x_freq_list[0] = x->x_phase[0] = x->x_val[0] = 0.;
    x->x_list_size = 1;
    stepnoise_seed(x, s, 0, NULL);
    if(ac){
        while(av->a_type == A_SYMBOL){
            if(atom_getsymbol(av) == gensym("-seed")){
                if(ac >= 2){
                    t_atom at[1];
                    SETFLOAT(at, atom_getfloat(av+1));
                    ac-=2, av+=2;
                    stepnoise_seed(x, s, 1, at);
                }
                else{
                    pd_error(x, "[stepnoise~]: -seed needs a seed value");
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
                    stepnoise_ch(x, n < 1 ? 1 : n);
                    ac-=2, av+=2;
                }
                else{
                    pd_error(x, "[stepnoise~]: -ch needs a channel number value");
                    return(NULL);
                }
            }
            else{
                pd_error(x, "[stepnoise~]: improper flag (%s)", atom_getsymbol(av)->s_name);
                return(NULL);
            }
        }
    }
    if(ac && av->a_type == A_FLOAT)
        x->x_freq_list[0] = atom_getfloatarg(0, ac, av);
    x->x_glist = canvas_getcurrent();
    outlet_new(&x->x_obj, &s_signal);
    return(x);
errstate:
    pd_error(x, "[stepnoise~]: improper args");
    return(NULL);
}

void stepnoise_tilde_setup(void){
    stepnoise_class = class_new(gensym("stepnoise~"), (t_newmethod)stepnoise_new,
        (t_method)stepnoise_free, sizeof(t_stepnoise), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(stepnoise_class, nullfn, gensym("signal"), 0);
    class_addmethod(stepnoise_class, (t_method)stepnoise_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(stepnoise_class, stepnoise_list);
    class_addmethod(stepnoise_class, (t_method)stepnoise_seed, gensym("seed"), A_GIMME, 0);
    class_addmethod(stepnoise_class, (t_method)stepnoise_ch, gensym("ch"), A_DEFFLOAT, 0);
    class_addmethod(stepnoise_class, (t_method)stepnoise_set, gensym("set"), A_GIMME, 0);
}
