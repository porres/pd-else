// based on [bob~] and VCV's VCF

#include "m_pd.h"
#include "random.h"
#include "magic.h"
#include <stdlib.h>
#include <math.h>

#define POLES 4
#define MAXLEN 1024

typedef struct _params{
    double      p_input;
    double      p_cutoff;
    double      p_resonance;
}t_params;

typedef struct _moog{
    t_object        x_obj;
    t_inlet        *x_inlet_hz;
    t_inlet        *x_inlet_reson;
    t_inlet        *x_inlet_drive;
    t_params        x_params;
    double          x_sr;
    int             x_oversample;
    int             x_hip;
    float          *x_freq_list;
    float          *x_reson_list;
    float          *x_drive_list;
    t_int           x_f_list_size;
    t_int           x_reson_list_size;
    t_int           x_drive_list_size;
    double         *x_state;
    int             x_nchs;
    int             x_bypass;
    t_int           x_n;
    t_int           x_ch1;
    t_int           x_ch2;
    t_int           x_ch3;
    t_int           x_ch4;
    t_int           x_sig2;
    t_int           x_sig3;
    t_int           x_sig4;
    t_glist        *x_glist;
    t_float        *x_sigscalar1;
    t_float        *x_sigscalar2;
    t_float        *x_sigscalar3;
    t_random_state  x_rstate;
    t_symbol       *x_ignore;
}t_moog;

static t_class *moog_class;

static double clip(double value){
    double x = value > 3.0 ? 3.0 : (value < -3.0 ? -3.0 : value);
    return x * (27.0 + x*x) / (27.0 + 9.0*x*x);
}

static void calc_derivatives(double *dstate,
double *state, t_params *params, int offset){
    double k = 2.0 * M_PI * params->p_cutoff;
    double x0 = clip(state[offset+0]);
    double x1 = clip(state[offset+1]);
    double x2 = clip(state[offset+2]);
    double x3 = clip(state[offset+3]);
    double inputc = clip(params->p_input - params->p_resonance * x3);
    dstate[0] = k * (inputc - x0);
    dstate[1] = k * (x0 - x1);
    dstate[2] = k * (x1 - x2);
    dstate[3] = k * (x2 - x3);
}

static void solver_rungekutte(double *state, double stepsize, t_params *params, int offset){
    int i;
    double deriv1[POLES], deriv2[POLES], deriv3[POLES], deriv4[POLES], tempstate[POLES];
    calc_derivatives(deriv1, state, params, offset);
    for(i = 0; i < POLES; i++)
        tempstate[i] = state[offset+i] + 0.5 * stepsize * deriv1[i];
    calc_derivatives(deriv2, tempstate, params, 0);
    for(i = 0; i < POLES; i++)
        tempstate[i] = state[offset+i] + 0.5 * stepsize * deriv2[i];
    calc_derivatives(deriv3, tempstate, params, 0);
    for(i = 0; i < POLES; i++)
        tempstate[i] = state[offset+i] + stepsize * deriv3[i];
    calc_derivatives(deriv4, tempstate, params, 0);
    for(i = 0; i < POLES; i++)
        state[offset+i] += (1./6.) * stepsize * (deriv1[i] + 2 * deriv2[i] + 2 * deriv3[i] + deriv4[i]);
}

static void moog_freq(t_moog *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    for(int i = 0; i < ac; i++)
        x->x_freq_list[i] = atom_getfloat(av+i);
    if(x->x_f_list_size != ac){
        x->x_f_list_size = ac;
        canvas_update_dsp();
    }
    else_magic_setnan(x->x_sigscalar1);
}

static void moog_reson(t_moog *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    for(int i = 0; i < ac; i++)
        x->x_reson_list[i] = atom_getfloat(av+i);
    if(x->x_reson_list_size != ac){
        x->x_reson_list_size = ac;
        canvas_update_dsp();
    }
    else_magic_setnan(x->x_sigscalar2);
}

static void moog_drive(t_moog *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    for(int i = 0; i < ac; i++)
        x->x_drive_list[i] = atom_getfloat(av+i);
    if(x->x_drive_list_size != ac){
        x->x_drive_list_size = ac;
        canvas_update_dsp();
    }
    else_magic_setnan(x->x_sigscalar3);
}

static void moog_oversample(t_moog *x, t_float oversample){
    x->x_oversample = oversample < 1 ? 1 : (int)oversample;
}

static void moog_clear(t_moog *x){
    for(int i = 0; i < (x->x_nchs * POLES); i++)
        x->x_state[i] = 0;
}

static void moog_hip(t_moog *x){
    moog_clear(x);
    x->x_hip = 1;
}

static void moog_lop(t_moog *x){
    moog_clear(x);
    x->x_hip = 0;
}

static void moog_bypass(t_moog *x, t_floatarg f){
    x->x_bypass = (int)(f != 0);
}

static t_int *moog_perform(t_int *w){
    t_moog *x = (t_moog *)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *cutoff = (t_float *)(w[3]);
    t_float *reson = (t_float *)(w[4]);
    t_float *drive = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);
    uint32_t *s1 = &x->x_rstate.s1;
    uint32_t *s2 = &x->x_rstate.s2;
    uint32_t *s3 = &x->x_rstate.s3;
    double stepsize = 1./(x->x_oversample * x->x_sr);
    if(!x->x_sig2){
        t_float *scalar = x->x_sigscalar1;
        if(!else_magic_isnan(*x->x_sigscalar1)){
            t_float freq = *scalar;
            x->x_ch2 = x->x_f_list_size = 1;
            x->x_freq_list[0] = freq;
            else_magic_setnan(x->x_sigscalar1);
        }
    }
    if(!x->x_sig3){
        t_float *scalar = x->x_sigscalar2;
        if(!else_magic_isnan(*x->x_sigscalar2)){
            t_float q = *scalar;
            x->x_ch3 = x->x_reson_list_size = 1;
            x->x_reson_list[0] = q;
            else_magic_setnan(x->x_sigscalar2);
        }
    }
    if(!x->x_sig4){
        t_float *scalar = x->x_sigscalar3;
        if(!else_magic_isnan(*x->x_sigscalar3)){
            t_float gain = *scalar;
            x->x_ch4 = x->x_drive_list_size = 1;
            x->x_drive_list[0] = gain;
            else_magic_setnan(x->x_sigscalar3);
        }
    }
    for(int j = 0; j < x->x_nchs; j++){
        int offset = j * POLES;
        for(int i = 0, n = x->x_n; i < n; i++){
            double xn, yn, f, q, drive_gain;
            if(x->x_ch1 == 1)
                xn = in[i];
            else
                xn = in[j*n + i];
            if(x->x_bypass)
                out[j*n + i] = xn;
            else{
                if(x->x_ch2 == 1)
                    f = x->x_sig2 ? cutoff[i] : x->x_freq_list[0];
                else
                    f = x->x_sig2 ? cutoff[j*n + i] : x->x_freq_list[j];
                if(x->x_ch3 == 1)
                    q = x->x_sig3 ? reson[i] : x->x_reson_list[0];
                else
                    q = x->x_sig3 ? reson[j*n + i] : x->x_reson_list[j];
                if(q < 0)
                    q = 0;
                if(q > 1)
                    q = 1;
                if(x->x_ch4 == 1)
                    drive_gain = x->x_sig4 ? drive[i] : x->x_drive_list[0];
                else
                    drive_gain = x->x_sig4 ? drive[j*n + i] : x->x_drive_list[j];
                if(drive_gain < -1)
                    drive_gain = -1;
                if(drive_gain > 1)
                    drive_gain = 1;
                drive_gain = pow(1. + drive_gain, 5);
                t_float noise = (t_float)(random_frand(s1, s2, s3)) * 1e-06;
                x->x_params.p_input = (xn * drive_gain) + noise; // add noise for self osc
                x->x_params.p_cutoff = f;
                x->x_params.p_resonance = pow(q, 2) * 10.f;
                for(int o = 0; o < x->x_oversample; o++)
                    solver_rungekutte(x->x_state, stepsize, &x->x_params, offset);
                if(x->x_hip){
                    yn = (x->x_params.p_input - x->x_params.p_resonance * x->x_state[offset+3])
                    - 4*x->x_state[offset+0] + 6*x->x_state[offset+1] - 4*x->x_state[offset+2]
                    + x->x_state[offset+3];
                }
                else
                    yn = x->x_state[offset+3];
                out[j*n + i] = yn;
            }
        }
    }
    return(w+7);
}

static void moog_dsp(t_moog *x, t_signal **sp){
    x->x_sr = sp[0]->s_sr;
    x->x_n = sp[0]->s_n;
    x->x_sig2 = else_magic_inlet_connection((t_object *)x, x->x_glist, 1, &s_signal);
    x->x_sig3 = else_magic_inlet_connection((t_object *)x, x->x_glist, 2, &s_signal);
    x->x_sig4 = else_magic_inlet_connection((t_object *)x, x->x_glist, 3, &s_signal);
    x->x_ch2 = x->x_sig2 ? sp[1]->s_nchans : x->x_f_list_size;
    x->x_ch3 = x->x_sig3 ? sp[2]->s_nchans : x->x_reson_list_size;
    x->x_ch4 = x->x_sig4 ? sp[3]->s_nchans : x->x_drive_list_size;
    int chs = x->x_ch1 = sp[0]->s_nchans;
    if(x->x_ch2 > chs)
        chs = x->x_ch2;
    if(x->x_ch3 > chs)
        chs = x->x_ch3;
    if(x->x_nchs != chs){
        x->x_state = (double *)resizebytes(x->x_state,
            x->x_nchs * POLES * sizeof(double), chs * POLES * sizeof(double));
        x->x_nchs = chs;
        moog_clear(x);
    }
    signal_setmultiout(&sp[4], x->x_nchs);
    if(x->x_ch1 > 1 && x->x_ch1 != x->x_nchs ||
       x->x_ch2 > 1 && x->x_ch2 != x->x_nchs ||
       x->x_ch3 > 1 && x->x_ch3 != x->x_nchs ||
       x->x_ch4 > 1 && x->x_ch4 != x->x_nchs){
        dsp_add_zero(sp[4]->s_vec, x->x_nchs*x->x_n);
        pd_error(x, "[moog~]: channel sizes mismatch");
        return;
    }
    dsp_add(moog_perform, 6, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
        sp[3]->s_vec, sp[4]->s_vec);
}

static void *moog_free(t_moog *x){
    inlet_free(x->x_inlet_hz);
    inlet_free(x->x_inlet_reson);
    inlet_free(x->x_inlet_drive);
    freebytes(x->x_state, x->x_nchs * POLES * sizeof(*x->x_state));
    free(x->x_freq_list);
    free(x->x_reson_list);
    free(x->x_drive_list);
    return(void *)x;
}

static void *moog_new(t_symbol *s, int ac, t_atom *av){
    t_moog *x = (t_moog *)pd_new(moog_class);
    random_init(&x->x_rstate, get_seed(s, 0, NULL, random_get_id()));
    x->x_hip = x->x_bypass = 0;
    float cutoff = 0, reson = 0, drive = 0, oversample = 2;
    int argnum = 0;
    x->x_nchs = 1;
    while(ac > 0){
        if(av->a_type == A_FLOAT){
            float argval = atom_getfloat(av);
            switch(argnum){
                case 0:
                    cutoff = argval;
                    break;
                case 1:
                    reson = argval;
                    break;
                case 2:
                    drive = argval;
                    break;
                default:
                    break;
            };
            argnum++;
            ac--;
            av++;
        }
        else if(av->a_type == A_SYMBOL && !argnum){
            t_symbol *cursym = atom_getsymbol(av);
            if(cursym == gensym("-oversample")){
                if(ac >= 2){
                    ac--, av++;
                    oversample = atom_getfloat(av);
                    ac--, av++;
                }
                else
                    goto errstate;
            }
/*            else if(cursym == gensym("-hip")){
                ac--, av++;
                x->x_hip = 1;
            }*/
            else
                goto errstate;
        }
        else
            goto errstate;
    }
    x->x_freq_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_reson_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_drive_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_freq_list[0] = cutoff;
    x->x_reson_list[0] = reson;
    x->x_drive_list[0] = drive;
    x->x_state = (double *)getbytes(POLES * sizeof(double));
    x->x_inlet_hz = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_hz, cutoff);
    x->x_inlet_reson = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_reson, reson);
    x->x_inlet_drive = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_drive, drive);
    outlet_new(&x->x_obj, gensym("signal"));
    moog_clear(x);
    moog_oversample(x, oversample);
    
    x->x_glist = canvas_getcurrent();
    x->x_sigscalar1 = obj_findsignalscalar((t_object *)x, 1);
    else_magic_setnan(x->x_sigscalar1);
    x->x_sigscalar2 = obj_findsignalscalar((t_object *)x, 2);
    else_magic_setnan(x->x_sigscalar2);
    x->x_sigscalar3 = obj_findsignalscalar((t_object *)x, 3);
    else_magic_setnan(x->x_sigscalar3);
    return(x);
errstate:
    pd_error(x, "[moog~]: improper args");
    return(NULL);
}

void moog_tilde_setup(void){
    moog_class = class_new(gensym("moog~"), (t_newmethod)(void *)moog_new, (t_method)moog_free, sizeof(t_moog), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(moog_class, nullfn, gensym("signal"), 0);
    class_addmethod(moog_class, (t_method)moog_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(moog_class, (t_method)moog_clear, gensym("clear"), 0);
//    class_addmethod(moog_class, (t_method)moog_hip, gensym("hip"), 0);
//    class_addmethod(moog_class, (t_method)moog_lop, gensym("lop"), 0);
    class_addmethod(moog_class, (t_method)moog_freq, gensym("freq"), A_GIMME, 0);
    class_addmethod(moog_class, (t_method)moog_reson, gensym("reson"), A_GIMME, 0);
    class_addmethod(moog_class, (t_method)moog_drive, gensym("drive"), A_GIMME, 0);
    class_addmethod(moog_class, (t_method)moog_bypass, gensym("bypass"), A_FLOAT, 0);
    class_addmethod(moog_class, (t_method)moog_oversample, gensym("oversample"), A_FLOAT, 0);
}
