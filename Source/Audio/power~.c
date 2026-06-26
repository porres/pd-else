// Porres 2018-2026

#include <m_pd.h>
#include <math.h>

typedef struct _power{
	t_object    x_obj;
    t_inlet    *x_inlet_f;
    t_int       x_mode;
    int         x_nchans;
    t_int       x_n;
    t_int       x_ch1;
    t_int       x_ch2;
}t_power;

static t_class *power_class;

static void power_mode(t_power *x, t_floatarg f){
    x->x_mode = (int)(f != 0);
}

static t_int *power_perform(t_int *w){
	t_power *x = (t_power *)(w[1]);
    t_sample *in1 = (t_sample *)(w[2]);
    t_sample *in2 = (t_sample *)(w[3]);
    t_sample *out = (t_sample *)(w[4]);
    t_float mode = x->x_mode;
    int n = x->x_n;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            float in = x->x_ch1 == 1 ? in1[i] : in1[j*n + i];
            if(in > 1)
                in = 1;
            else if(in < -1)
                in = -1;
            float p = x->x_ch2 == 1 ? in2[i] : in2[j*n + i];
            if(p == 1)
                out[j*n + i] = in;
            else{
                float output;
                if(in >= 1.)
                    output = 1.;
                else if(in <= -1)
                    output = -1.;
                else if(mode == 0){
                    if(in >= 0.)
                        output = pow(in, 1/p);
                    else
                        output = -pow(-in, 1/p);
                }
                else if(mode == 1){
                    if(in >= 0)
                        output = 1. - powf(1. - in, p);
                    else
                        output = powf(1. + in, p) - 1.;
                }
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
    x->x_mode = 0;
    t_float f = 1;
    int arg = 0;
    while(ac > 0){
        if(av->a_type == A_FLOAT){
            f = atom_getfloat(av);
            ac--, av++;
            arg = 1;
        }
        else if(av->a_type == A_SYMBOL && !arg && ac >= 2){
            if(atom_getsymbol(av) == gensym("-mode")){
                ac--, av++;
                if(av->a_type == A_FLOAT){
                    x->x_mode = atom_getint(av) != 0;
                    ac--, av++;
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    }
    x->x_inlet_f = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_f, f);
    outlet_new((t_object *)x, &s_signal);
    return(void *)x;
errstate:
    pd_error(x, "[power~]: improper args");
    return(NULL);
}

void power_tilde_setup(void){
	power_class = class_new(gensym("power~"), (t_newmethod)power_new, (t_method)power_free, sizeof(t_power), CLASS_MULTICHANNEL, A_GIMME, 0);
	class_addmethod(power_class, nullfn, gensym("signal"), 0);
	class_addmethod(power_class, (t_method)power_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(power_class, (t_method)power_mode, gensym("mode"), A_DEFFLOAT, 0);
}
