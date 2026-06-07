// Porres 2018-2026

#include <m_pd.h>
#include <math.h>

static t_class *power_class;

typedef struct _power{
	t_object    x_obj;
    t_inlet    *x_inlet_f;
    int         x_nchans;
    t_int       x_n;
    t_int       x_ch1;
    t_int       x_ch2;
}t_power;

static t_int *power_perform(t_int *w){
	t_power *x = (t_power *)(w[1]);
    t_sample *in1 = (t_sample *)(w[2]);
    t_sample *in2 = (t_sample *)(w[3]);
    t_sample *out = (t_sample *)(w[4]);
    int n = x->x_n;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            float in = x->x_ch1 == 1 ? in1[i] : in1[j*n + i];
            float power = x->x_ch2 == 1 ? in2[i] : in2[j*n + i];
            if(power == 1){
                out[j*n + i] = in;
            }
            else{
                float output;
                if(in >= 0.)
                    output = pow(in, power);
                else
                    output = -pow(-in, power);
                out[j*n + i] = output;
            }
        }
    };
	return(w+5);
}

static void power_dsp(t_power *x, t_signal **sp){
    x->x_n = sp[0]->s_n, x->x_ch1 = sp[0]->s_nchans, x->x_ch2 = sp[1]->s_nchans;
    x->x_nchans = x->x_ch1 > x->x_ch2 ? x->x_ch1 : x->x_ch2;
    signal_setmultiout(&sp[2], x->x_nchans);
    if(x->x_ch1 > 1 && x->x_ch1 != x->x_nchans ||
    x->x_ch2 > 1 && x->x_ch2 != x->x_nchans){
        dsp_add_zero(sp[2]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[power~]: channel sizes mismatch");
        return;
    }
    dsp_add(power_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *power_free(t_power *x){
    inlet_free(x->x_inlet_f);
    return(void *)x;
}

static void *power_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_power *x = (t_power *) pd_new(power_class);
    t_float f = ac ? atom_getfloatarg(0, ac, av) : 1;
    x->x_inlet_f = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_f, f);
    outlet_new((t_object *)x, &s_signal);
    return(void *)x;
}

void power_tilde_setup(void){
	power_class = class_new(gensym("power~"), (t_newmethod)power_new, (t_method)power_free, sizeof(t_power), CLASS_MULTICHANNEL, A_GIMME, 0);
	class_addmethod(power_class, nullfn, gensym("signal"), 0);
	class_addmethod(power_class, (t_method)power_dsp, gensym("dsp"), A_CANT, 0);
}
