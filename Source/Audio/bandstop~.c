// Porres 2017

#include <m_pd.h>
#include <stdlib.h>
#include <math.h>
#include "magic.h"

#define PI 3.14159265358979323846
#define HALF_LOG2 log(2)/2
#define MAXLEN 1024

typedef struct _bandstop{
    t_object    x_obj;
    t_int       x_n;
    int         x_nchans;
    t_int       x_sig2;
    t_int       x_sig3;
    t_int       x_ch1;
    t_int       x_ch2;
    t_int       x_ch3;
    double     *x_xnm1;
    double     *x_xnm2;
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
    int         x_bw;
    double      x_radcoeff;
    double      x_f;
    double      x_reson;
    double      x_a0;
    double      x_a1;
    double      x_a2;
    double      x_b1;
    double      x_b2;
    t_glist    *x_glist;
    t_float    *x_sigscalar1;
    t_float    *x_sigscalar2;
    t_symbol   *x_ignore;
}t_bandstop;

static t_class *bandstop_class;

static void bandstop_freq(t_bandstop *x, t_symbol *s, int ac, t_atom *av){
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

static void bandstop_reson(t_bandstop *x, t_symbol *s, int ac, t_atom *av){
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

static void update_coeffs(t_bandstop *x, double f, double reson){
    x->x_f = f;
    x->x_reson = reson;
    double q;
    double omega = f * x->x_radcoeff;
    if(x->x_bw){ // reson is bw in octaves
         if(reson < 0.000001)
             reson = 0.000001;
         q = 1 / (2 * sinh(HALF_LOG2 * reson * omega/sin(omega)));
    }
    else
        q = reson;
    if(q < 0.000001) // force bypass
        x->x_a0 = 1, x->x_a2 = x->x_b1 = x->x_b2 = 0;
    else{
        double alphaQ = sin(omega) / (2*q);
        double b0 = alphaQ + 1;
        x->x_a0 = 1 / b0;
        x->x_a1 = -2*cos(omega) / b0;
        x->x_a2 = x->x_a0;
        x->x_b1 = -x->x_a1;
        x->x_b2 = (alphaQ - 1) / b0;
    }
}

static t_int *bandstop_perform(t_int *w){
    t_bandstop *x = (t_bandstop *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *in3 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    double *xnm1 = x->x_xnm1, *xnm2 = x->x_xnm2;
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
                yn = x->x_a0 * xn + x->x_a1 * xnm1[j] + x->x_a2 * xnm2[j] + x->x_b1 * ynm1[j] + x->x_b2 * ynm2[j];
                out[j*n + i] = yn;
                xnm2[j] = xnm1[j];
                xnm1[j] = xn;
                ynm2[j] = ynm1[j];
                ynm1[j] = yn;
            }
        }
    }
    x->x_xnm1 = xnm1, x->x_xnm2 = xnm2;
    x->x_ynm1 = ynm1, x->x_ynm2 = ynm2;
    return(w+6);
}

static void bandstop_dsp(t_bandstop *x, t_signal **sp){
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
        x->x_xnm1 = (double *)resizebytes(x->x_xnm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_xnm2 = (double *)resizebytes(x->x_xnm2,
            x->x_nchans * sizeof(double), chs * sizeof(double));
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
        pd_error(x, "[bandstop~]: channel sizes mismatch");
        return;
    }
    dsp_add(bandstop_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec);
}
static void bandstop_clear(t_bandstop *x){
    for(int i = 0; i < x->x_nchans; i++)
        x->x_xnm1[i] = x->x_xnm2[i] = x->x_ynm1[i] = x->x_ynm2[i] = 0.;
}

static void bandstop_bypass(t_bandstop *x, t_floatarg f){
    x->x_bypass = (int)(f != 0);
}

static void bandstop_bw(t_bandstop *x){
    x->x_bw = 1;
    update_coeffs(x, x->x_f, x->x_reson);
}

static void bandstop_q(t_bandstop *x){
    x->x_bw = 0;
    update_coeffs(x, x->x_f, x->x_reson);
}

static void *bandstop_free(t_bandstop *x){
    inlet_free(x->x_inlet_freq);
    inlet_free(x->x_inlet_q);
    outlet_free(x->x_out);
    freebytes(x->x_xnm1, x->x_nchans * sizeof(*x->x_xnm1));
    freebytes(x->x_xnm2, x->x_nchans * sizeof(*x->x_xnm2));
    freebytes(x->x_ynm1, x->x_nchans * sizeof(*x->x_ynm1));
    freebytes(x->x_ynm2, x->x_nchans * sizeof(*x->x_ynm2));
    free(x->x_freq_list);
    free(x->x_reson_list);
    return(void *)x;
}

static void *bandstop_new(t_symbol *s, int ac, t_atom *av){
    t_bandstop *x = (t_bandstop *)pd_new(bandstop_class);
    x->x_ignore = s;
    float freq = 0.000001;
    float reson = 0;
    int bw = 0;
    int argnum = 0;
    x->x_xnm1 = (double *)getbytes(sizeof(*x->x_xnm1));
    x->x_xnm2 = (double *)getbytes(sizeof(*x->x_xnm2));
    x->x_ynm1 = (double *)getbytes(sizeof(*x->x_ynm1));
    x->x_ynm2 = (double *)getbytes(sizeof(*x->x_ynm2));
    x->x_freq_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_reson_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_freq_list[0] = x->x_reson_list[0] = 0;
    x->x_xnm1[0] = x->x_xnm2[0] = 0;
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
                bw = 1;
                ac--, av++;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    };
    x->x_bw = bw;
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
    pd_error(x, "[bandstop~]: improper args");
    return(NULL);
}

void bandstop_tilde_setup(void){
    bandstop_class = class_new(gensym("bandstop~"), (t_newmethod)bandstop_new,
        (t_method)bandstop_free, sizeof(t_bandstop), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(bandstop_class, (t_method)bandstop_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(bandstop_class, nullfn, gensym("signal"), 0);
    class_addmethod(bandstop_class, (t_method)bandstop_clear, gensym("clear"), 0);
    class_addmethod(bandstop_class, (t_method)bandstop_bypass, gensym("bypass"), A_DEFFLOAT, 0);
    class_addmethod(bandstop_class, (t_method)bandstop_q, gensym("q"), 0);
    class_addmethod(bandstop_class, (t_method)bandstop_bw, gensym("bw"), 0);
    class_addmethod(bandstop_class, (t_method)bandstop_freq, gensym("freq"), A_GIMME, 0);
    class_addmethod(bandstop_class, (t_method)bandstop_reson, gensym("reson"), A_GIMME, 0);
}
