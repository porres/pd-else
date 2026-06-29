// Porres 2017

#include <m_pd.h>
#include <stdlib.h>
#include <math.h>
#include "magic.h"

#define PI 3.14159265358979323846
#define HALF_LOG2 log(2)/2
#define T60_COEFF PI / (log(1000) * 1000)
#define MAXLEN 1024

typedef struct _bp2{
    t_object    x_obj;
    t_int       x_n;
    int         x_nchans;
    t_int       x_sig2;
    t_int       x_sig3;
    t_int       x_ch1;
    t_int       x_ch2;
    t_int       x_ch3;
    double     *x_ynm1;
    double     *x_ynm2;
    float      *x_freq_list;
    float      *x_reson_list;
    t_int       x_f_list_size;
    t_int       x_q_list_size;
    t_inlet    *x_inlet_freq;
    t_inlet    *x_inlet_q;
    t_outlet   *x_out;
    t_float     x_nyq;
    int         x_bypass;
    int         x_resmode; // 0: q / 1: bw / 2: t60
    double      x_radcoeff;
    double      x_f;
    double      x_reson;
    double      x_a0;
    double      x_b1;
    double      x_b2;
    t_glist    *x_glist;
    t_float    *x_sigscalar1;
    t_float    *x_sigscalar2;
    t_symbol   *x_ignore;
}t_bp2;

static t_class *bp2_class;

static void bp2_freq(t_bp2 *x, t_symbol *s, int ac, t_atom *av){
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

static void bp2_reson(t_bp2 *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    for(int i = 0; i < ac; i++)
        x->x_reson_list[i] = atom_getfloat(av+i);
    if(x->x_q_list_size != ac){
        x->x_q_list_size = ac;
        canvas_update_dsp();
    }
    else_magic_setnan(x->x_sigscalar2);
}

static void update_coeffs(t_bp2 *x, double f, double reson){
    x->x_reson = reson;
    x->x_f = f;
    double omega = f * x->x_radcoeff;
    double q;
    switch(x->x_resmode){
    case 0:
        q = reson;
        break;
    case 1:
        if(reson < 1e-6)
            reson = 1e-6;
        q = 1.0 / (2.0 * sinh(HALF_LOG2 * reson * omega / sin(omega)));
        break;
    case 2:
        q = f * reson * T60_COEFF;
        break;
    }
    if(q < 1e-6){
        x->x_a0 = 1.0;
        x->x_b1 = 0.0;
        x->x_b2 = 0.0;
        return;
    }
    double oneminusr = omega / q;
    if(oneminusr > 1.0)
        oneminusr = 1.0;
    double r = 1.0 - oneminusr;
    x->x_b1 = 2.0 * cos(omega) * r;
    x->x_b2 = -r * r;
    x->x_a0 = 2.0 * oneminusr * (oneminusr + r * omega);
}

static t_int *bp2_perform(t_int *w){
    t_bp2 *x = (t_bp2 *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *in3 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    double *ynm1 = x->x_ynm1, *ynm2 = x->x_ynm2;
    t_float nyq = x->x_nyq;
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
            t_float reson = *scalar;
            x->x_ch3 = x->x_q_list_size = 1;
            x->x_reson_list[0] = reson;
            else_magic_setnan(x->x_sigscalar2);
        }
    }
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0, n = x->x_n; i < n; i++){
            double xn, yn, f, reson;
            if(x->x_ch1 == 1)
                xn = in1[i];
            else
                xn = in1[j*n + i];
            if(x->x_ch2 == 1)
                f = x->x_sig2 ? in2[i] : x->x_freq_list[0];
            else
                f = x->x_sig2 ? in2[j*n + i] : x->x_freq_list[j];
            if(f < 0.000001)
                f = 0.000001;
            if(f > nyq - 0.000001)
                f = nyq - 0.000001;
            if(x->x_ch3 == 1)
                reson = x->x_sig3 ? in3[i] : x->x_reson_list[0];
            else
                reson = x->x_sig3 ? in3[j*n + i] : x->x_reson_list[j];
            if(x->x_bypass)
                out[j*n + i] = xn;
            else{
                if(f != x->x_f || reson != x->x_reson)
                    update_coeffs(x, (double)f, (double)reson);
                yn = xn + x->x_b1 * ynm1[j] + x->x_b2 * ynm2[j];
                out[j*n + i] = x->x_a0 * yn;
                ynm2[j] = ynm1[j];
                ynm1[j] = yn;
            }
        }
    }
    x->x_ynm1 = ynm1;
    x->x_ynm2 = ynm2;
    return(w+6);
}

static void bp2_dsp(t_bp2 *x, t_signal **sp){
    t_float nyq = sp[0]->s_sr * 0.5;
    x->x_n = sp[0]->s_n;
    if(nyq != x->x_nyq){
        x->x_nyq = nyq;
        x->x_radcoeff = PI / x->x_nyq;
        update_coeffs(x, x->x_f, x->x_reson);
    }
    x->x_sig2 = else_magic_inlet_connection((t_object *)x, x->x_glist, 1, &s_signal);
    x->x_sig3 = else_magic_inlet_connection((t_object *)x, x->x_glist, 2, &s_signal);
    x->x_ch2 = x->x_sig2 ? sp[1]->s_nchans : x->x_f_list_size;
    x->x_ch3 = x->x_sig3 ? sp[2]->s_nchans : x->x_q_list_size;
    int chs = x->x_ch1 = sp[0]->s_nchans;
    if(x->x_ch2 > chs)
        chs = x->x_ch2;
    if(x->x_ch3 > chs)
        chs = x->x_ch3;
    if(x->x_nchans != chs){
        x->x_ynm1 = (double *)resizebytes(x->x_ynm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_ynm2 = (double *)resizebytes(x->x_ynm2,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[3], x->x_nchans);
    if((x->x_ch1 > 1 && x->x_ch1 != x->x_nchans)
    || (x->x_ch2 > 1 && x->x_ch2 != x->x_nchans)
    || (x->x_ch3 > 1 && x->x_ch3 != x->x_nchans)){
        dsp_add_zero(sp[3]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[bp2~]: channel sizes mismatch");
        return;
    }
    dsp_add(bp2_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec);
}

static void bp2_clear(t_bp2 *x){
    for(int i = 0; i < x->x_nchans; i++)
        x->x_ynm1[i] = x->x_ynm2[i] = 0.;
}

static void bp2_bypass(t_bp2 *x, t_floatarg f){
    x->x_bypass = (int)(f != 0);
}

static void bp2_bw(t_bp2 *x){
    x->x_resmode = 1;
    update_coeffs(x, x->x_f, x->x_reson);
}

static void bp2_q(t_bp2 *x){
    x->x_resmode = 0;
    update_coeffs(x, x->x_f, x->x_reson);
}

static void bp2_t60(t_bp2 *x){
    x->x_resmode = 2;
    update_coeffs(x, x->x_f, x->x_reson);
}

static void *bp2_free(t_bp2 *x){
    inlet_free(x->x_inlet_freq);
    inlet_free(x->x_inlet_q);
    outlet_free(x->x_out);
    freebytes(x->x_ynm1, x->x_nchans * sizeof(*x->x_ynm1));
    freebytes(x->x_ynm2, x->x_nchans * sizeof(*x->x_ynm2));
    free(x->x_freq_list);
    free(x->x_reson_list);
    return(void *)x;
}

static void *bp2_new(t_symbol *s, int ac, t_atom *av){
    t_bp2 *x = (t_bp2 *)pd_new(bp2_class);
    x->x_ignore = s;
    float freq = 0.000001;
    float reson = 0;
    int resmode = 0;
    int argnum = 0;
    x->x_ynm1 = (double *)getbytes(sizeof(*x->x_ynm1));
    x->x_ynm2 = (double *)getbytes(sizeof(*x->x_ynm2));
    x->x_freq_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_reson_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_freq_list[0] = x->x_reson_list[0] = 0;
    x->x_ynm1[0] = x->x_ynm2[0] = 0;
    x->x_f_list_size = x->x_q_list_size = 1;
    while(ac > 0){
        if(av->a_type == A_FLOAT){ //if current argument is a float
            t_float aval = atom_getfloat(av);
            switch(argnum){
                case 0:
                    freq = aval;
                    break;
                case 1:
                    reson = aval;
                    break;
                default:
                    break;
            };
            argnum++;
            ac--, av++;
        }
        else if(av->a_type == A_SYMBOL && !argnum){
            if(atom_getsymbol(av) == gensym("-bw")){
                resmode = 1;
                ac--, av++;
            }
            else if(atom_getsymbol(av) == gensym("-t60")){
                resmode = 2;
                ac--, av++;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    };
    x->x_resmode = resmode;
    x->x_nyq = sys_getsr() * 0.5;
    x->x_radcoeff = PI / x->x_nyq;
    x->x_freq_list[0] = freq, x->x_reson_list[0] = reson;
    update_coeffs(x, (double)freq, (double)reson);
    x->x_inlet_freq = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_freq, freq);
    x->x_inlet_q = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_q, reson);
    x->x_out = outlet_new((t_object *)x, &s_signal);
    x->x_glist = canvas_getcurrent();
    x->x_sigscalar1 = obj_findsignalscalar((t_object *)x, 1);
    else_magic_setnan(x->x_sigscalar1);
    x->x_sigscalar2 = obj_findsignalscalar((t_object *)x, 2);
    else_magic_setnan(x->x_sigscalar2);
    return(x);
errstate:
    pd_error(x, "[bp2~]: improper args");
    return(NULL);
}

void bp2_tilde_setup(void){
    bp2_class = class_new(gensym("bp2~"), (t_newmethod)bp2_new,
        (t_method)bp2_free, sizeof(t_bp2), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(bp2_class, (t_method)bp2_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(bp2_class, nullfn, gensym("signal"), 0);
    class_addmethod(bp2_class, (t_method)bp2_clear, gensym("clear"), 0);
    class_addmethod(bp2_class, (t_method)bp2_bypass, gensym("bypass"), A_DEFFLOAT, 0);
    class_addmethod(bp2_class, (t_method)bp2_q, gensym("q"), 0);
    class_addmethod(bp2_class, (t_method)bp2_bw, gensym("bw"), 0);
    class_addmethod(bp2_class, (t_method)bp2_t60, gensym("t60"), 0);
    class_addmethod(bp2_class, (t_method)bp2_freq, gensym("freq"), A_GIMME, 0);
    class_addmethod(bp2_class, (t_method)bp2_reson, gensym("reson"), A_GIMME, 0);
}
