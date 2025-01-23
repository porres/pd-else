// Porres 2017-2024

#include <m_pd.h>
#include <stdlib.h>
#include "random.h"
#include "magic.h"

#define MAXLEN 1024

static t_class *randpulse2_class;

typedef struct _randpulse2{
    t_object       x_obj;
    t_random_state x_rstate;
    int            x_rand;
    int            x_id;
    int            x_nchans;
    int            x_ch;
    int            x_n;
    t_int          x_sig1;
    float         *x_freq_list;
    t_float        x_sr_rec;
    t_float        x_freq;
    t_int          x_list_size;
    double        *x_phase;
    t_float       *x_lastout;
    t_float       *x_output;
    t_float       *x_ynp1;
    t_float       *x_yn;
    t_float       *x_random;
    t_symbol      *x_ignore;
    t_glist       *x_glist;
}t_randpulse2;

static void randpulse2_rand(t_randpulse2 *x, t_float f){
    x->x_rand = f != 0;
}

static void randpulse2_seed(t_randpulse2 *x, t_symbol *s, int ac, t_atom *av){
    for(int i = 0; i < x->x_nchans; i++)
        x->x_phase[i] = x->x_freq_list[i] >= 0 ? 1.0 : 0.0;
    random_init(&x->x_rstate, get_seed(s, ac, av, x->x_id));
}

static void randpulse2_ch(t_randpulse2 *x, t_floatarg f){
    x->x_ch = f < 1 ? 1 : (int)f;
    canvas_update_dsp();
}

static void randpulse2_list(t_randpulse2 *x, t_symbol *s, int ac, t_atom * av){
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

static void randpulse2_set(t_randpulse2 *x, t_symbol *s, int ac, t_atom *av){
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

static t_int *randpulse2_perform(t_int *w){
    t_randpulse2 *x = (t_randpulse2 *)(w[1]);
    int chs = (t_int)(w[2]); // number of channels in main input signal (density)
    t_float *in = (t_float *)(w[3]);
    t_float *out = (t_sample *)(w[4]);
    uint32_t *s1 = &x->x_rstate.s1;
    uint32_t *s2 = &x->x_rstate.s2;
    uint32_t *s3 = &x->x_rstate.s3;
    t_float *ynp1 = x->x_ynp1, *yn = x->x_yn;
    t_float *lastout = x->x_lastout, *output = x->x_output;
    t_float *random = x->x_random;
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
            float amp;
            double phase_step = hz * x->x_sr_rec;
            phase_step = phase_step > 1 ? 1. : phase_step < -1 ? -1 : phase_step;
            if(hz >= 0){
                if(phase[j] >= 1.){  // update
                    random[j] = (t_float)(random_frand(s1, s2, s3));
                    phase[j] -= 1;
                    yn[j] = ynp1[j];
                    ynp1[j] = random[j]; // next random value
                }
                amp = (yn[j] + (ynp1[j] - yn[j]) * (phase[j]));
            }
            else{
                if(phase[j] <= 0.){ // update
                    random[j] = (t_float)(random_frand(s1, s2, s3));
                    phase[j] += 1;
                    yn[j] = ynp1[j];
                    ynp1[j] = random[j]; // next random value
                }
                amp = (yn[j] + (ynp1[j] - yn[j]) * (1 - phase[j]));
            }
            if(amp > 0 && lastout[j] == 0){
                if(x->x_rand)
                    output[j] =  (t_float)(random_frand(s1, s2, s3));
                else
                    output[j] = 1;
            }
            else if(amp < 0)
                output[j] = 0;
            out[j*x->x_n + i] = output[j];
            phase[j] += phase_step; // next phase
            lastout[j] = output[j];
        }
    }
    x->x_output = output;
    x->x_lastout = lastout;
    x->x_phase = phase;
    x->x_ynp1 = ynp1; // next random value
    x->x_yn = yn; // current output
    x->x_random = random;
    return(w+5);
}

static void randpulse2_dsp(t_randpulse2 *x, t_signal **sp){
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
        x->x_random = (t_float *)resizebytes(x->x_random,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_lastout = (t_float *)resizebytes(x->x_lastout,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_output = (t_float *)resizebytes(x->x_output,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_ynp1 = (t_float *)resizebytes(x->x_ynp1,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_yn = (t_float *)resizebytes(x->x_yn,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(randpulse2_perform, 4, x, nchans, sp[0]->s_vec, sp[1]->s_vec);
}

static void *randpulse2_free(t_randpulse2 *x){
    freebytes(x->x_phase, x->x_nchans * sizeof(*x->x_phase));
    freebytes(x->x_random, x->x_nchans * sizeof(*x->x_random));
    freebytes(x->x_lastout, x->x_nchans * sizeof(*x->x_lastout));
    freebytes(x->x_output, x->x_nchans * sizeof(*x->x_output));
    freebytes(x->x_ynp1, x->x_nchans * sizeof(*x->x_ynp1));
    freebytes(x->x_yn, x->x_nchans * sizeof(*x->x_yn));
    free(x->x_freq_list);
    return(void *)x;
}

static void *randpulse2_new(t_symbol *s, int ac, t_atom *av){
    t_randpulse2 *x = (t_randpulse2 *)pd_new(randpulse2_class);
    x->x_id = random_get_id();
    x->x_nchans = 1;
    x->x_ch = 1;
    x->x_freq_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_phase = (double *)getbytes(sizeof(*x->x_phase));
    x->x_random = (t_float *)getbytes(sizeof(*x->x_random));
    x->x_lastout = (t_float *)getbytes(sizeof(*x->x_lastout));
    x->x_output = (t_float *)getbytes(sizeof(*x->x_output));
    x->x_ynp1 = (t_float *)getbytes(sizeof(*x->x_ynp1));
    x->x_yn = (t_float *)getbytes(sizeof(*x->x_yn));
    x->x_freq_list[0] = x->x_phase[0] = x->x_random[0] = 0;
    x->x_lastout[0] = x->x_output[0] = 0.;
    x->x_list_size = 1;
    randpulse2_seed(x, s, 0, NULL);
    if(ac){
        while(av->a_type == A_SYMBOL){
            if(atom_getsymbol(av) == gensym("-seed")){
                if(ac >= 2){
                    t_atom at[1];
                    SETFLOAT(at, atom_getfloat(av+1));
                    ac-=2, av+=2;
                    randpulse2_seed(x, s, 1, at);
                }
                else{
                    pd_error(x, "[randpulse2~]: -seed needs a seed value");
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
                    pd_error(x, "[randpulse2~]: -ch needs a channel number value");
                    return(NULL);
                }
            }
            else{
                pd_error(x, "[randpulse2~]: improper flag (%s)", atom_getsymbol(av)->s_name);
                return(NULL);
            }
        }
    }
    if(ac <= 2){
        int numargs = 0;
        while(ac > 0){
            if(av->a_type == A_FLOAT){
                switch(numargs){
                    case 0: x->x_freq_list[0] = atom_getfloatarg(0, ac, av);
                        numargs++;
                        ac--;
                        av++;
                        break;
                    case 1: x->x_rand  = atom_getfloatarg(0, ac, av) != 0;
                        numargs++;
                        ac--;
                        av++;
                        break;
                    default:
                        ac--;
                        av++;
                        break;
                };
            }
            else
                goto errstate;
        };
    }
    else if(ac > 3)
        goto errstate;
    x->x_glist = canvas_getcurrent();
    outlet_new(&x->x_obj, &s_signal);
    return(x);
errstate:
    pd_error(x, "[randpulse2~]: improper args");
    return(NULL);
}

void randpulse2_tilde_setup(void){
    randpulse2_class = class_new(gensym("randpulse2~"), (t_newmethod)randpulse2_new,
        (t_method)randpulse2_free, sizeof(t_randpulse2), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(randpulse2_class, nullfn, gensym("signal"), 0);
    class_addmethod(randpulse2_class, (t_method)randpulse2_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(randpulse2_class, randpulse2_list);
    class_addmethod(randpulse2_class, (t_method)randpulse2_seed, gensym("seed"), A_GIMME, 0);
    class_addmethod(randpulse2_class, (t_method)randpulse2_ch, gensym("ch"), A_DEFFLOAT, 0);
    class_addmethod(randpulse2_class, (t_method)randpulse2_set, gensym("set"), A_GIMME, 0);
    class_addmethod(randpulse2_class, (t_method)randpulse2_rand, gensym("rand"), A_DEFFLOAT, 0);
}
