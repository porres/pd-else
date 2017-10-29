// porres 2017

#include "m_pd.h"
#include <math.h>

typedef struct _adsr{
    t_object x_obj;
    t_float  x_in;
    t_inlet  *x_inlet_attack;
    t_inlet  *x_inlet_decay;
    t_inlet  *x_inlet_sustain;
    t_inlet  *x_inlet_release;
    t_float  x_last;
    t_float  x_target;
    t_float  x_sustain_target;
    t_float  x_sr_khz;
    double   x_incr;
    int      x_nleft;
    int      x_gate_status;
} t_adsr;


static t_class *adsr_class;

static t_int *adsr_perform(t_int *w){
    t_adsr *x = (t_adsr *)(w[1]);
    int nblock = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *in3 = (t_float *)(w[5]);
    t_float *in4 = (t_float *)(w[6]);
    t_float *in5 = (t_float *)(w[7]);
    t_float *out = (t_float *)(w[8]);
    t_float last = x->x_last;
    t_float target = x->x_target;
    t_float gate_status = x->x_gate_status;
    double incr = x->x_incr;
    int nleft = x->x_nleft;
    while (nblock--){
        t_float input_gate = *in1++;
        t_float attack = *in2++;
        t_float decay = *in3++;
        t_float sustain_point = *in4++;
        t_float release = *in5++;
        t_int gate = (input_gate != 0);
// clip input
        if (attack < 0)
            attack = 0;
        if (decay < 0)
            decay = 0;
        if (release < 0)
            release = 0;
        t_float n_attack = roundf(attack * x->x_sr_khz);
        t_float n_decay = roundf(decay * x->x_sr_khz);
        t_float n_release = roundf(release * x->x_sr_khz);
// set coefs
        double coef_a;
        if(n_attack == 0)
            coef_a = 0.;
        else
            coef_a = 1. / n_attack;
        double coef_d;
        if(n_decay == 0)
            coef_d = 0.;
        else
            coef_d = 1. / n_decay;
        double coef_r;
        if(n_release == 0)
            coef_r = 0.;
        else
            coef_r = 1. / n_release;
// go for it
        if(gate != gate_status){ // gate status change
            gate_status = gate;
            target = input_gate;
            if (gate_status){ // if gate opened
                if(n_attack > 1){
                    incr = (target - last) * coef_a;
                    nleft = n_attack + n_decay;
                }
                else{
                    incr = (target - last);
                    nleft = 1 + n_decay;
                }
            }
            else{ // if gate closed
                if(n_release > 1){
                    incr =  -(last * coef_r);
                    nleft = n_release;
                }
                else{
                    incr =  -last;
                    nleft = 1;
                }
            }
        }
// "attack + decay + sustain" phase
        if (gate_status){
            if(nleft > 0){ // "attack + decay" not over
                if (nleft < n_decay){ // attack is over
                    incr = ((target * sustain_point) - target) * coef_d;
                    }
                    *out++ = (last += incr);
                nleft--;
            }
            else // "sustain" phase
                *out++ = target * sustain_point;
        }
// "release" phase
        else{
            if(nleft > 0){ // "release" not over
                *out++ = (last += incr);
                nleft--;
            }
            else // "release" over
                *out++ = 0;
        }
    };
    x->x_last = (PD_BIGORSMALL(last) ? 0. : last);
    x->x_target = (PD_BIGORSMALL(target) ? 0. : target);
    x->x_incr = incr;
    x->x_nleft = nleft;
    x->x_gate_status = gate_status;
    return (w + 9);
}

static void adsr_dsp(t_adsr *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    dsp_add(adsr_perform, 8, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
            sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec);
}

static void *adsr_new(t_floatarg a, t_floatarg d, t_floatarg s, t_floatarg r){
    t_adsr *x = (t_adsr *)pd_new(adsr_class);
    x->x_sr_khz = sys_getsr() * 0.001;
    x->x_last = 0.;
    x->x_target = 0.;
    x->x_incr = 0.;
    x->x_nleft = 0;
    x->x_gate_status = 0;
    x->x_inlet_attack = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_attack, a);
    x->x_inlet_decay = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_decay, d);
    x->x_inlet_sustain = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_sustain, s);
    x->x_inlet_release = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_release, r);
    outlet_new((t_object *)x, &s_signal);
    return (x);
}

void adsr_tilde_setup(void){
    adsr_class = class_new(gensym("adsr~"), (t_newmethod)adsr_new, 0,
				 sizeof(t_adsr), 0, A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(adsr_class, t_adsr, x_in);
    class_addmethod(adsr_class, (t_method) adsr_dsp, gensym("dsp"), A_CANT, 0);
}
