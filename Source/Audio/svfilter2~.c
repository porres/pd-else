// based on plaits' ZDF SVF filter

#include <math.h>
#include <m_pd.h>

#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

#define TWO_PI             (M_PI * 2.)
#define svfilter2_MAXOMEGA  (M_PI * .5)

typedef struct _svfilter2{
    t_object    x_obj;
    t_inlet    *x_freq_inlet;
    t_inlet    *x_q_inlet;
    float      *x_lastf;
    float      *x_lastq;
    float      *x_state1;
    float      *x_state2;
    float      *x_g;
    float      *x_r;
    float      *x_h;
    float       x_srcoef;
    int         x_n;
    int         x_nchans;
    int         x_ch2;
    int         x_ch3;
}t_svfilter2;

static t_class *svfilter2_class;

static void svfilter2_clear(t_svfilter2 *x){
    for(int j = 0; j < x->x_nchans; j++){
        x->x_state1[j] = x->x_state2[j] = 0.;
        x->x_lastf[j] = x->x_lastq[j] = -1;
    }
}

static t_int *svfilter2_perform(t_int *w){
    t_svfilter2 *x = (t_svfilter2 *)(w[1]);
    t_float *xin = (t_float *)(w[2]);
    t_float *fin = (t_float *)(w[3]);
    t_float *qin = (t_float *)(w[4]);
    t_float *lout = (t_float *)(w[5]);
    t_float *hout = (t_float *)(w[6]);
    t_float *bout = (t_float *)(w[7]);
    t_float *nout = (t_float *)(w[8]);
    for(int j = 0; j < x->x_nchans; j++){
        float state1 = x->x_state1[j];
        float state2 = x->x_state2[j];
        for(int i = 0; i < x->x_n; i++){
            float xn = xin[j*x->x_n+i];
            float hz = x->x_ch2 == 1 ? fin[i] : fin[j*x->x_n+i];
            float q = x->x_ch3 == 1 ? qin[i] : qin[j*x->x_n+i];
            if(hz < 0)
                hz = 0;
            if(hz != x->x_lastf[j]){
                float omega = hz * x->x_srcoef;
                if(omega > svfilter2_MAXOMEGA)
                    omega = svfilter2_MAXOMEGA;
                x->x_g[j] = tanf(omega * 0.5);
            }
            if(q < 0)
                q = 0.01;
            if(q != x->x_lastq[j] || hz != x->x_lastf[j]){
                x->x_r[j] = 1.0f / q;
                x->x_h[j] = 1.0f /
                    (1.0f + x->x_r[j] * x->x_g[j] +
                     x->x_g[j] * x->x_g[j]);
            }
            
            x->x_lastf[j] = hz;
            x->x_lastq[j] = q;
            
            float g = x->x_g[j];
            float r = x->x_r[j];
            float h = x->x_h[j];

            float hp = (xn - r*state1 - g*state1 - state2) * h;
            float bp = g * hp + state1;
            state1 = g * hp + bp;
            float lp = g * bp + state2;
            state2 = g * bp + lp;
            lout[j*x->x_n+i] = lp;
            bout[j*x->x_n+i] = bp;
            hout[j*x->x_n+i] = hp;
            nout[j*x->x_n+i] = lp + hp;
        }
        x->x_state1[j] = PD_BIGORSMALL(state1) ? 0. : state1;
        x->x_state2[j] = PD_BIGORSMALL(state2) ? 0. : state2;
    }
    return(w+9);
}

static void svfilter2_dsp(t_svfilter2 *x, t_signal **sp){
    float srcoef = TWO_PI / sp[0]->s_sr;
    if(x->x_srcoef != srcoef){
        x->x_srcoef = srcoef;
        svfilter2_clear(x);
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
        x->x_state1 = (float *)resizebytes(x->x_state1,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_state2 = (float *)resizebytes(x->x_state2,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_g = (float *)resizebytes(x->x_g,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_r = (float *)resizebytes(x->x_r,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_h = (float *)resizebytes(x->x_h,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        for(int j = x->x_nchans; j < chs; j++){
            x->x_state1[j] = x->x_state2[j] = 0.;
            x->x_g[j] = x->x_r[j] = 0;
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
        pd_error(x, "[svfilter2~]: channel sizes mismatch");
        return;
    }
    dsp_add(svfilter2_perform, 8, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec);
}

static void *svfilter2_free(t_svfilter2 *x){
    freebytes(x->x_lastf, x->x_nchans * sizeof(*x->x_lastf));
    freebytes(x->x_lastq, x->x_nchans * sizeof(*x->x_lastq));
    freebytes(x->x_state1, x->x_nchans * sizeof(*x->x_state1));
    freebytes(x->x_state2, x->x_nchans * sizeof(*x->x_state2));
    freebytes(x->x_g, x->x_nchans * sizeof(*x->x_g));
    freebytes(x->x_r, x->x_nchans * sizeof(*x->x_r));
    freebytes(x->x_h, x->x_nchans * sizeof(*x->x_h));
    return(void *)x;
}

static void *svfilter2_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_svfilter2 *x = (t_svfilter2 *)pd_new(svfilter2_class);
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
    x->x_state1 = (float *)getbytes(sizeof(*x->x_state1));
    x->x_state2 = (float *)getbytes(sizeof(*x->x_state2));
    x->x_g = (float *)getbytes(sizeof(*x->x_g));
    x->x_r = (float *)getbytes(sizeof(*x->x_r));
    x->x_h = (float *)getbytes(sizeof(*x->x_h));
    x->x_lastf[0] = -1;
    x->x_lastq[0] = -1;
    x->x_state1[0] = 0;
    x->x_state2[0] = 0;
    x->x_g[0] = 0;
    x->x_r[0] = 0;
    x->x_h[0] = 0;
    x->x_freq_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_freq_inlet, freq);
    x->x_q_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_q_inlet, qcoef);
    outlet_new((t_object *)x, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    svfilter2_clear(x);
    return(x);
}

void svfilter2_tilde_setup(void){
    svfilter2_class = class_new(gensym("svfilter2~"),
        (t_newmethod)svfilter2_new, (t_method)svfilter2_free, sizeof(t_svfilter2),
        CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(svfilter2_class, nullfn, gensym("signal"), 0);
    class_addmethod(svfilter2_class, (t_method)svfilter2_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(svfilter2_class, (t_method)svfilter2_clear, gensym("clear"), 0);
}
