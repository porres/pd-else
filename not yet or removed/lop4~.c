// 2026

#include <math.h>
#include "m_pd.h"

#define PI 3.14159265358979323846

typedef struct _lop4{
    t_object    x_obj;
    t_inlet    *x_inlet;
    int         x_n;
    int         x_nchans;
    int         x_ch2;
    float       x_nyq;
    float       x_srcoef;
    double     *x_state;
//    double     *x_last_f;
    double     *x_g;
    double     *x_gi;
}t_lop4;

static t_class *lop4_class;

static void lop4_clear(t_lop4 *x){
    for(int j = 0; j < x->x_nchans; j++)
        x->x_state[j] = 0.;
}

static void lop4_set_coeff(t_lop4 *x, int j, double freq){
    double f = freq / (double)(x->x_nyq * 2.0);
    if(f < 0)
        f = 0;
    if(f > 0.497) // clip a bit below nyquist
        f = 0.497;
    x->x_g[j] = tan(PI * f);
    x->x_gi[j] = 1.0 / (1.0 + x->x_g[j]);
}

static t_int *lop4_perform(t_int *w){
    t_lop4 *x = (t_lop4 *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
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
        // IF LAST IN
            lop4_set_coeff(x, j, kin);
            double lp;

            lp = (x->x_g[j] * in + x->x_state[j]) * x->x_gi[j];

            if(PD_BIGORSMALL(lp))
                lp = 0;
            
            x->x_state[j] = x->x_g[j] * (in - lp) + lp;

            out[n*j+i] = lp;
        }
    }
    return(w+5);
}

static void lop4_dsp(t_lop4 *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    x->x_nyq = sp[0]->s_sr * 0.5;
    int chs = sp[0]->s_nchans;
    x->x_ch2 = sp[1]->s_nchans;
    if(x->x_nchans != chs){
        x->x_state = (double *)resizebytes(x->x_state,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_g = (double *)resizebytes(x->x_g,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_gi = (double *)resizebytes(x->x_gi,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        for(int j = x->x_nchans; j < chs; j++){
            x->x_state[j] = 0.;
            x->x_g[j] = 0.;
            x->x_gi[j] = 1.;
        }
        x->x_nchans = chs;
    }
    float srcoef = PI / x->x_nyq;
    if(x->x_srcoef != srcoef){
        x->x_srcoef = srcoef;
        lop4_clear(x);
        for(int j = 0; j < x->x_nchans; j++)
            lop4_set_coeff(x, j, 1000);
    }
    signal_setmultiout(&sp[2], x->x_nchans);
    if(x->x_ch2 > 1 && x->x_ch2 != x->x_nchans){
        dsp_add_zero(sp[2]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[lop4~]: channel sizes mismatch");
        return;
    }
    dsp_add(lop4_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *lop4_free(t_lop4 *x){
    freebytes(x->x_state, x->x_nchans * sizeof(*x->x_state));
    freebytes(x->x_g, x->x_nchans * sizeof(*x->x_g));
    freebytes(x->x_gi, x->x_nchans * sizeof(*x->x_gi));
    return(void *)x;
}

static void *lop4_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_lop4 *x = (t_lop4 *)pd_new(lop4_class);
    t_float f = 0;
    x->x_nchans = 1;
    if(ac)
        f = atom_getfloat(av);
    x->x_state = (double *)getbytes(sizeof(double));
    x->x_state[0] = 0.0;
    x->x_g = (double *)getbytes(sizeof(double));
    x->x_g[0] = 0.0;
    x->x_gi = (double *)getbytes(sizeof(double));
    x->x_gi[0] = 1.0;
    
    lop4_set_coeff(x, 0, f);
    
    x->x_nyq = sys_getsr() * 0.5;
    x->x_srcoef = PI / x->x_nyq;
    x->x_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet, f);
    outlet_new((t_object *)x, &s_signal);
    return(x);
    errstate:
        pd_error(x, "[lop4~]: improper args");
        return(NULL);
}

void lop4_tilde_setup(void){
    lop4_class = class_new(gensym("lop4~"), (t_newmethod)(void *)lop4_new,
        (t_method)lop4_free, sizeof(t_lop4), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(lop4_class, nullfn, gensym("signal"), 0);
    class_addmethod(lop4_class, (t_method)lop4_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(lop4_class, (t_method)lop4_clear, gensym("clear"), 0);
}
