// Porres

#include <m_pd.h>
#include <math.h>

#define PI 3.14159265358979323846

typedef struct _hip2{
    t_object    x_obj;
    t_int       x_n;
    int         x_nchans;
    int         x_ch2;
    t_inlet    *x_inlet_freq;
    t_outlet   *x_out;
    t_float     x_nyq;
    float       x_srcoef;
    double     *x_f;
    double     *x_xnm1;
    double     *x_ynm1;
    double     *x_a0;
    double     *x_a1;
    double     *x_b1;
}t_hip2;

static t_class *hip2_class;

static void update_coeffs(t_hip2 *x, int j, double f){
    x->x_f[j] = f;

    if(f <= 0){
        x->x_a0[j] = x->x_a1[j] = x->x_b1[j] = 0;
        return;
    }
    else if(f >= x->x_nyq){
        x->x_a0[j] = 1;
        x->x_a1[j] = x->x_b1[j] = 0;
        return;
    }
    double omega = f * x->x_srcoef;
    double cosw = cos(omega);
    double k;
    if(fabs(cosw) < 1e-12)
        k = 1.0;
    else
        k = (cosw - 1.0 + sqrt(1.0 - cosw * cosw)) / cosw;
    x->x_a0[j] = 1.0 - 0.5 * k;
    x->x_a1[j] = -x->x_a0[j];
    x->x_b1[j] = 1.0 - k;
}

static t_int *hip2_perform(t_int *w){
    t_hip2 *x = (t_hip2 *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    for(int j = 0, n = x->x_n; j < x->x_nchans; j++){
        double xnm1 = x->x_xnm1[j];
        double ynm1 = x->x_ynm1[j];
        for(int i = 0; i < n; i++){
            double xn = in1[j*n+i];
            double f;
            if(x->x_ch2 == 1)
                f = in2[i];
            else
                f = in2[j*n+i];
            if(f < 0)
                f = 0;
            if(f != x->x_f[j])
                update_coeffs(x, j, f);
            double yn = x->x_a0[j] * xn +
                x->x_a1[j] * xnm1 +
                x->x_b1[j] * ynm1;
            out[j*n+i] = yn;
            xnm1 = xn;
            ynm1 = yn;
        }
        x->x_xnm1[j] = xnm1;
        x->x_ynm1[j] = ynm1;
    }
    return(w+5);
}

static void hip2_dsp(t_hip2 *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    x->x_nyq = sp[0]->s_sr * 0.5;
    float srcoef = PI / x->x_nyq;
    if(x->x_srcoef != srcoef){
        x->x_srcoef = srcoef;
        for(int j = 0; j < x->x_nchans; j++)
            update_coeffs(x, j, x->x_f[j]);
    }
    int chs = sp[0]->s_nchans;
    x->x_ch2 = sp[1]->s_nchans;
    if(x->x_nchans != chs){
        x->x_xnm1 = (double *)resizebytes(x->x_xnm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_ynm1 = (double *)resizebytes(x->x_ynm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_f = (double *)resizebytes(x->x_f,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_a0 = (double *)resizebytes(x->x_a0,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_a1 = (double *)resizebytes(x->x_a1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_b1 = (double *)resizebytes(x->x_b1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        for(int j = x->x_nchans; j < chs; j++){
            x->x_xnm1[j] = 0;
            x->x_ynm1[j] = 0;
            x->x_f[j] = -1;
            x->x_a0[j] = 0;
            x->x_a1[j] = 0;
            x->x_b1[j] = 0;
        }
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[2], x->x_nchans);
    if(x->x_ch2 > 1 && x->x_ch2 != x->x_nchans){
        dsp_add_zero(sp[2]->s_vec, x->x_nchans * x->x_n);
        pd_error(x, "[hip2~]: channel sizes mismatch");
        return;
    }
    dsp_add(hip2_perform, 4, x, sp[0]->s_vec,
        sp[1]->s_vec, sp[2]->s_vec);
}

static void hip2_clear(t_hip2 *x){
    for(int i = 0; i < x->x_nchans; i++)
        x->x_xnm1[i] = x->x_ynm1[i] = 0;
}

static void *hip2_free(t_hip2 *x){
    freebytes(x->x_xnm1, x->x_nchans * sizeof(*x->x_xnm1));
    freebytes(x->x_ynm1, x->x_nchans * sizeof(*x->x_ynm1));
    freebytes(x->x_a0, x->x_nchans * sizeof(*x->x_a0));
    freebytes(x->x_a1, x->x_nchans * sizeof(*x->x_a1));
    freebytes(x->x_b1, x->x_nchans * sizeof(*x->x_b1));
    freebytes(x->x_f, x->x_nchans * sizeof(*x->x_f));
    return(void *)x;
}

static void *hip2_new(t_floatarg f){
    t_hip2 *x = (t_hip2 *)pd_new(hip2_class);
    double freq = f < 0 ? 0 : (double)f;
    x->x_nchans = 1;
    x->x_xnm1 = (double *)getbytes(sizeof(*x->x_xnm1));
    x->x_ynm1 = (double *)getbytes(sizeof(*x->x_ynm1));
    x->x_a0 = (double *)getbytes(sizeof(*x->x_a0));
    x->x_a1 = (double *)getbytes(sizeof(*x->x_a1));
    x->x_b1 = (double *)getbytes(sizeof(*x->x_b1));
    x->x_a0[0] = 0;
    x->x_a1[0] = 0;
    x->x_b1[0] = 0;
    x->x_f = (double *)getbytes(sizeof(*x->x_f));
    x->x_f[0] = -1;
    x->x_xnm1[0] = 0;
    x->x_ynm1[0] = 0;
    x->x_nyq = sys_getsr() * 0.5;
    x->x_srcoef = PI / x->x_nyq;
    x->x_f[0] = freq;
    update_coeffs(x, 0, freq);
    
    x->x_inlet_freq = inlet_new((t_object *)x,
        (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_freq, freq);

    x->x_out = outlet_new((t_object *)x, &s_signal);
    return(x);
}

void hip2_tilde_setup(void){
    hip2_class = class_new(gensym("hip2~"), (t_newmethod)hip2_new,
        (t_method)hip2_free, sizeof(t_hip2), CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
    class_addmethod(hip2_class, (t_method)hip2_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(hip2_class, nullfn, gensym("signal"), 0);
    class_addmethod(hip2_class, (t_method)hip2_clear, gensym("clear"), 0);
}
