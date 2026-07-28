// based on cyclone's which is based on Chamberlin's prototype from "Musical Applications
// of Microprocessors" (csound's svfilter).  Slightly distorted, no upsampling.

#include <math.h>
#include <m_pd.h>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#define TWO_PI             (M_PI * 2.)
#define SVFILTER_MAXOMEGA  (M_PI * .5)
#define SVFILTER_DRIVE     .0001
#define SVFILTER_QSTRETCH  1.2

typedef struct _svfilter{
    t_object    x_obj;
    t_inlet    *x_freq_inlet;
    t_inlet    *x_q_inlet;
    float      *x_lastf;
    float      *x_lastq;
    float      *x_band;
    float      *x_low;
    float      *x_c1;
    float      *x_c2;
    float       x_srcoef;
    int         x_n;
    int         x_nchans;
    int         x_ch2;
    int         x_ch3;
}t_svfilter;

static t_class *svfilter_class;

static void svfilter_clear(t_svfilter *x){
    for(int j = 0; j < x->x_nchans; j++){
        x->x_band[j] = x->x_low[j] = 0.;
        x->x_lastf[j] = x->x_lastq[j] = -1;
    }
}

static t_int *svfilter_perform(t_int *w){
    t_svfilter *x = (t_svfilter *)(w[1]);
    t_float *xin = (t_float *)(w[2]);
    t_float *fin = (t_float *)(w[3]);
    t_float *qin = (t_float *)(w[4]);
    t_float *lout = (t_float *)(w[5]);
    t_float *hout = (t_float *)(w[6]);
    t_float *bout = (t_float *)(w[7]);
    t_float *nout = (t_float *)(w[8]);
    for(int j = 0; j < x->x_nchans; j++){
        float band = x->x_band[j];
        float low = x->x_low[j];
        for(int i = 0; i < x->x_n; i++){
            float xn = xin[j*x->x_n+i];
            float hz = x->x_ch2 == 1 ? fin[i] : fin[j*x->x_n+i];
            float q = x->x_ch3 == 1 ? qin[i] : qin[j*x->x_n+i];
            if(hz < 0)
                hz = 0;
            if(hz != x->x_lastf[j]){
                float omega = hz * x->x_srcoef;
                if(omega > SVFILTER_MAXOMEGA)
                    omega = SVFILTER_MAXOMEGA;
                x->x_c1[j] = sinf(omega);
                x->x_lastf[j] = hz;
            }
            if(q < 0)
                q = 0;
            if(q > 1)
                q = 1;
            if(q != x->x_lastq[j]){
                float r = (1. - q) * SVFILTER_QSTRETCH;
                x->x_c2[j] = r * r;
                x->x_lastq[j] = q;
            }
            float high = xn - low - x->x_c2[j] * band;
            low = low + x->x_c1[j] * band;
            band = x->x_c1[j] * high + band;
            lout[j*x->x_n+i] = low;
            hout[j*x->x_n+i] = high;
            bout[j*x->x_n+i] = band;
            nout[j*x->x_n+i] = low + high;
            band -= band * band * band * SVFILTER_DRIVE;
        }
        x->x_band[j] = PD_BIGORSMALL(band) ? 0. : band;
        x->x_low[j] = PD_BIGORSMALL(low) ? 0. : low;
    }
    return(w+9);
}

static void svfilter_dsp(t_svfilter *x, t_signal **sp){
    float srcoef = TWO_PI / sp[0]->s_sr;
    if(x->x_srcoef != srcoef){
        x->x_srcoef = srcoef;
        svfilter_clear(x);
    }
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    x->x_ch2 = sp[1]->s_nchans;
    x->x_ch3 = sp[2]->s_nchans;
    if(x->x_nchans != chs){
        x->x_lastf = (float *)resizebytes(x->x_lastf,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_lastq = (float *)resizebytes(x->x_lastq,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_band = (float *)resizebytes(x->x_band,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_low = (float *)resizebytes(x->x_low,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_c1 = (float *)resizebytes(x->x_c1,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_c2 = (float *)resizebytes(x->x_c2,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        for(int j = x->x_nchans; j < chs; j++){
            x->x_band[j] = x->x_low[j] = 0.;
            x->x_c1[j] = x->x_c2[j] = 0;
            x->x_lastf[j] = x->x_lastq[j] = -1;
        }
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[3], x->x_nchans);
    signal_setmultiout(&sp[4], x->x_nchans);
    signal_setmultiout(&sp[5], x->x_nchans);
    signal_setmultiout(&sp[6], x->x_nchans);
    if((x->x_ch2 > 1 && x->x_ch2 != x->x_nchans)
    || (x->x_ch3 > 1 && x->x_ch3 != x->x_nchans)){
        dsp_add_zero(sp[3]->s_vec, x->x_nchans*x->x_n);
        dsp_add_zero(sp[4]->s_vec, x->x_nchans*x->x_n);
        dsp_add_zero(sp[5]->s_vec, x->x_nchans*x->x_n);
        dsp_add_zero(sp[6]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[svfilter~]: channel sizes mismatch");
        return;
    }
    dsp_add(svfilter_perform, 8, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec);
}

static void *svfilter_free(t_svfilter *x){
    freebytes(x->x_lastf, x->x_nchans * sizeof(*x->x_lastf));
    freebytes(x->x_lastq, x->x_nchans * sizeof(*x->x_lastq));
    freebytes(x->x_band, x->x_nchans * sizeof(*x->x_band));
    freebytes(x->x_low, x->x_nchans * sizeof(*x->x_low));
    freebytes(x->x_c1, x->x_nchans * sizeof(*x->x_c1));
    freebytes(x->x_c2, x->x_nchans * sizeof(*x->x_c2));
    return(void *)x;
}

static void *svfilter_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_svfilter *x = (t_svfilter *)pd_new(svfilter_class);
    float freq = 0, qcoef = 0.01;
    if(ac && av->a_type == A_FLOAT){
        freq = av->a_w.w_float;
        ac--; av++;
        if(ac && av->a_type == A_FLOAT)
            qcoef = av->a_w.w_float;
    }
    x->x_nchans = 1;
    x->x_srcoef = TWO_PI / sys_getsr();
    x->x_lastf = (float *)getbytes(sizeof(*x->x_lastf));
    x->x_lastq = (float *)getbytes(sizeof(*x->x_lastq));
    x->x_band = (float *)getbytes(sizeof(*x->x_band));
    x->x_low = (float *)getbytes(sizeof(*x->x_low));
    x->x_c1 = (float *)getbytes(sizeof(*x->x_c1));
    x->x_c2 = (float *)getbytes(sizeof(*x->x_c2));
    x->x_lastf[0] = -1;
    x->x_lastq[0] = -1;
    x->x_band[0] = 0;
    x->x_low[0] = 0;
    x->x_c1[0] = 0;
    x->x_c2[0] = 0;
    x->x_freq_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_freq_inlet, freq);
    x->x_q_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_q_inlet, qcoef);
    outlet_new((t_object *)x, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    svfilter_clear(x);
    return(x);
}

void svfilter_tilde_setup(void){
    svfilter_class = class_new(gensym("svfilter~"),
        (t_newmethod)svfilter_new, (t_method)svfilter_free, sizeof(t_svfilter),
        CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(svfilter_class, nullfn, gensym("signal"), 0);
    class_addmethod(svfilter_class, (t_method)svfilter_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(svfilter_class, (t_method)svfilter_clear, gensym("clear"), 0);
}
