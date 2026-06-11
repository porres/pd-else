// Porres 2026

#include <m_pd.h>
#include <math.h>

static t_class *atan2_class;

typedef struct _atan2{
	t_object    x_obj;
    t_inlet    *x_inlet_f;
    int         x_nchans;
    t_int       x_n;
    t_int       x_ch1;
    t_int       x_ch2;
}t_atan2;

static t_int *atan2_perform(t_int *w){
	t_atan2 *x = (t_atan2 *)(w[1]);
    t_sample *in1 = (t_sample *)(w[2]);
    t_sample *in2 = (t_sample *)(w[3]);
    t_sample *out = (t_sample *)(w[4]);
    for(int j = 0; j < x->x_nchans; j++){
        int ch = j*x->x_n;
        for(int i = 0; i < x->x_n; i++){
            float a = x->x_ch1 == 1 ? in1[i] : in1[ch + i];
            float b = x->x_ch2 == 1 ? in2[i] : in2[ch + i];
            out[ch + i] = atan2f(a, b);
        }
    };
	return(w+5);
}

static void atan2_dsp(t_atan2 *x, t_signal **sp){
    x->x_n = sp[0]->s_n, x->x_ch1 = sp[0]->s_nchans, x->x_ch2 = sp[1]->s_nchans;
    x->x_nchans = x->x_ch1 > x->x_ch2 ? x->x_ch1 : x->x_ch2;
    signal_setmultiout(&sp[2], x->x_nchans);
    if(x->x_ch1 > 1 && x->x_ch1 != x->x_nchans ||
    x->x_ch2 > 1 && x->x_ch2 != x->x_nchans){
        dsp_add_zero(sp[2]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[atan2~]: channel sizes mismatch");
        return;
    }
    dsp_add(atan2_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *atan2_free(t_atan2 *x){
    inlet_free(x->x_inlet_f);
    return(void *)x;
}

static void *atan2_new(t_floatarg f){
    t_atan2 *x = (t_atan2 *)pd_new(atan2_class);
    x->x_inlet_f = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_f, f);
    outlet_new(&x->x_obj, gensym("signal"));
    return(x);
}

void atan2_tilde_setup(void){
	atan2_class = class_new(gensym("atan2~"), (t_newmethod)atan2_new, (t_method)atan2_free, sizeof(t_atan2), CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
	class_addmethod(atan2_class, nullfn, gensym("signal"), 0);
	class_addmethod(atan2_class, (t_method)atan2_dsp, gensym("dsp"), A_CANT, 0);
}
