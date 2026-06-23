// Porres 2017

#include <m_pd.h>
#include <math.h>

static t_class *pulsediv_class;

typedef struct _pulsediv{
    t_object  x_obj;
    int       x_div;
    t_float  *x_count;
    t_float  *x_lastin;
    t_float   x_start;
    t_int     x_n;
    t_int     x_nchans;
    t_int     x_ch2;
    t_inlet  *x_triglet;
    t_outlet *x_outlet_0;
    t_outlet *x_outlet_1;
}t_pulsediv;

static void pulsediv_div(t_pulsediv *x, t_floatarg f){
    x->x_div = f < 1 ? 1 : (int)f;
}

static void pulsediv_start(t_pulsediv *x, t_floatarg f){
    x->x_start = f;
}

static t_int *pulsediv_perform(t_int *w){
    t_pulsediv *x = (t_pulsediv *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *out1 = (t_float *)(w[4]);
    t_float *out2 = (t_float *)(w[5]);
    t_float *lastin = x->x_lastin;
    t_float *count = x->x_count;
    t_float start = x->x_start;
    int div = x->x_div;
    int n = x->x_n;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            t_float in = in1[j*n + i];
            t_float trig = x->x_ch2 == 1 ? in2[i] : in2[j*n + i];
            if(trig > 0)
                count[j] = start;
            t_float pulse = (in > 0 && lastin[j] <= 0);
            count[j] += pulse;
            if(count[j] >= 0)
                count[j] = fmod(count[j], div);
            out1[j*n + i] = pulse && count[j] == 0;
            out2[j*n + i] = pulse && count[j] != 0;
            lastin[j] = in;
        }
    }
    x->x_lastin = lastin;
    x->x_count = count;
    return(w+6);
}

static void pulsediv_dsp(t_pulsediv *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    x->x_ch2 = sp[1]->s_nchans;
    if(x->x_nchans != chs){
       x->x_lastin = (t_float *)resizebytes(x->x_lastin,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_count = (t_float *)resizebytes(x->x_count,
             x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        for(int i = 0; i < chs; i++){
            x->x_lastin[i] = 1;
            x->x_count[i] = x->x_start;
        }
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[2], x->x_nchans);
    signal_setmultiout(&sp[3], x->x_nchans);
    if((x->x_ch2 > 1 && x->x_ch2 != x->x_nchans)){
        dsp_add_zero(sp[2]->s_vec, x->x_nchans*x->x_n);
        dsp_add_zero(sp[3]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[pulsediv~]: channel sizes mismatch");
        return;
    }
    dsp_add(pulsediv_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec);
}

static void *pulsediv_free(t_pulsediv *x){
    freebytes(x->x_lastin, x->x_nchans * sizeof(*x->x_lastin));
    freebytes(x->x_count, x->x_nchans * sizeof(*x->x_count));
    inlet_free(x->x_triglet);
    outlet_free(x->x_outlet_0);
    outlet_free(x->x_outlet_1);
    return (void *)x;
}

static void *pulsediv_new(t_floatarg f1, t_floatarg f2){
    t_pulsediv *x = (t_pulsediv *)pd_new(pulsediv_class);
    x->x_lastin = (t_float *)getbytes(sizeof(*x->x_lastin));
    x->x_lastin[0] = 1;
    x->x_count = (t_float *)getbytes(sizeof(*x->x_count));
    x->x_div = f1 < 1 ? 1 : (int)f1;
    x->x_start = x->x_count[0] = f2 - 1;
    x->x_triglet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_outlet_0 = outlet_new(&x->x_obj, &s_signal);
    x->x_outlet_1 = outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void pulsediv_tilde_setup(void){
    pulsediv_class = class_new(gensym("pulsediv~"),
        (t_newmethod)pulsediv_new, (t_method)pulsediv_free,
        sizeof(t_pulsediv), CLASS_MULTICHANNEL, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(pulsediv_class, nullfn, gensym("signal"), 0);
    class_addmethod(pulsediv_class, (t_method) pulsediv_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(pulsediv_class, (t_method)pulsediv_div, gensym("div"), A_DEFFLOAT, 0);
    class_addmethod(pulsediv_class, (t_method)pulsediv_start, gensym("start"), A_DEFFLOAT, 0);
}
