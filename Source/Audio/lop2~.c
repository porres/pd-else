// 2026

#include <math.h>
#include "m_pd.h"

#define PI 3.14159265358979323846

typedef struct _lop2{
    t_object    x_obj;
    t_inlet    *x_inlet;
    int         x_n;
    int         x_nchans;
    int         x_coeff;
    int         x_ch2;
    float       x_nyq;
    float       x_srcoef;
    double     *x_ynm1;
    double     *x_last_k;
    double     *x_k;
}t_lop2;

static t_class *lop2_class;

static void lop2_clear(t_lop2 *x){
    for(int j = 0; j < x->x_nchans; j++)
        x->x_ynm1[j] = 0.;
}

static void lop2_coeff(t_lop2 *x, t_floatarg f){
    int coeff = (int)f != 0;
    if(x->x_coeff != coeff){
        x->x_coeff = coeff;
        for(int i = 0; i < x->x_nchans; i++)
            x->x_last_k[i] = -1;
    }
}

static double lop2_get_coeff(t_lop2 *x, double kin){
    double k;
    if(x->x_coeff)
        k = kin;
    else{
        double w = kin * x->x_srcoef; // radians
        if(kin < 50)
            k = w;
        else{
            if(kin > x->x_nyq)
                k = 1;
            else{
                double cosw = cos(w);
                double twomincos = (2 - cosw);
                k = sqrtf((twomincos * twomincos) - 1) + cosw - 1;
            }
        }
    }
    return(k < 0 ? 0 : k > 1 ? 1 : k);
}

static t_int *lop2_perform(t_int *w){
    t_lop2 *x = (t_lop2 *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    double *ynm1 = x->x_ynm1;
    double *last_k = x->x_last_k;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0, n = x->x_n; i < n; i++){
            double in = (double)in1[n*j+i];
            double kin;
            if(x->x_ch2 == 1)
                kin = (double)in2[i];
            else
                kin = (double)in2[n*j+i];
            if(kin < 0)
                kin = 0;
            if(kin != last_k[j])
                x->x_k[j] = lop2_get_coeff(x, kin);
            last_k[j] = kin;
            ynm1[j] = ynm1[j] + x->x_k[j] * (in - ynm1[j]);
            if(PD_BIGORSMALL(ynm1[j]))
                ynm1[j] = 0;
            out[n*j+i] = ynm1[j];
        }
    }
    x->x_ynm1 = ynm1;
    x->x_last_k = last_k;
    return(w+5);
}

static void lop2_dsp(t_lop2 *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    x->x_nyq = sp[0]->s_sr * 0.5;
    int chs = sp[0]->s_nchans;
    x->x_ch2 = sp[1]->s_nchans;
    if(x->x_nchans != chs){
        x->x_ynm1 = (double *)resizebytes(x->x_ynm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_last_k = (double *)resizebytes(x->x_last_k,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_k = (double *)resizebytes(x->x_k,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        for(int j = x->x_nchans; j < chs; j++){
            x->x_ynm1[j] = 0;
            x->x_last_k[j] = -1;
            x->x_k[j] = 0;
        }
        x->x_nchans = chs;
    }
    float srcoef = PI / x->x_nyq;
    if(x->x_srcoef != srcoef){
        x->x_srcoef = srcoef;
        lop2_clear(x);
        for(int j = 0; j < x->x_nchans; j++)
            x->x_last_k[j] = -1;
    }
    signal_setmultiout(&sp[2], x->x_nchans);
    if(x->x_ch2 > 1 && x->x_ch2 != x->x_nchans){
        dsp_add_zero(sp[2]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[lop2~]: channel sizes mismatch");
        return;
    }
    dsp_add(lop2_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *lop2_free(t_lop2 *x){
    freebytes(x->x_ynm1, x->x_nchans * sizeof(*x->x_ynm1));
    freebytes(x->x_last_k, x->x_nchans * sizeof(*x->x_last_k));
    freebytes(x->x_k, x->x_nchans * sizeof(*x->x_k));
    return(void *)x;
}

static void *lop2_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_lop2 *x = (t_lop2 *)pd_new(lop2_class);
    t_float f = 0;
    x->x_coeff = 0;
    x->x_nchans = 1;
    if(ac){
        if(av->a_type == A_SYMBOL){
            if(atom_getsymbol(av) == gensym("-coeff")){
                x->x_coeff = 1;
                ac--, av++;
            }
            else
                goto errstate;
        }
        if(ac){
            if(av->a_type == A_FLOAT){
                f = atom_getfloat(av);
            }
            else
                goto errstate;
        }
    }
    
    x->x_ynm1 = (double *)getbytes(sizeof(*x->x_ynm1));
    x->x_ynm1[0] = 0;
    x->x_last_k = (double *)getbytes(sizeof(*x->x_last_k));
    x->x_last_k[0] = -1;
    x->x_k = (double *)getbytes(sizeof(*x->x_k));
    x->x_k[0] = 0;
    
    x->x_nyq = sys_getsr() * 0.5;
    x->x_srcoef = PI / x->x_nyq;
    x->x_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet, f);
    outlet_new((t_object *)x, &s_signal);
    return(x);
    errstate:
        pd_error(x, "[lop2~]: improper args");
        return(NULL);
}

void lop2_tilde_setup(void){
    lop2_class = class_new(gensym("lop2~"), (t_newmethod)(void *)lop2_new,
        (t_method)lop2_free, sizeof(t_lop2), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(lop2_class, nullfn, gensym("signal"), 0);
    class_addmethod(lop2_class, (t_method)lop2_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(lop2_class, (t_method)lop2_clear, gensym("clear"), 0);
    class_addmethod(lop2_class, (t_method)lop2_coeff, gensym("coeff"), A_FLOAT, 0);
}
