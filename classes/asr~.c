// porres 2017

#include "m_pd.h"
#include <math.h>

typedef struct _asr{
    t_object x_obj;
    t_float  x_in;
    t_inlet  *x_inlet_attack;
    t_inlet  *x_inlet_release;
    int      x_n_attack;
    int      x_n_release;
    double   x_coef_a;
    double   x_coef_r;
    t_float  x_last;
    t_float  x_target;
    t_float  x_sr_khz;
    double   x_incr;
    int      x_nleft;
    int      x_gate_status;
} t_asr;


static t_class *asr_class;

static t_int *asr_perform(t_int *w){
    t_asr *x = (t_asr *)(w[1]);
    int nblock = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *in3 = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);
    t_float last = x->x_last;
    t_float target = x->x_target;
    t_float gate_status = x->x_gate_status;
    double incr = x->x_incr;
    int nleft = x->x_nleft;
    while (nblock--){
        t_float input = *in1++;
        t_float attack = *in2++;
        t_float release = *in3++;
        t_int gate = (input != 0);
        if (attack < 0)
            attack = 0;
        if (release < 0)
            release = 0;
        x->x_n_attack = roundf(attack * x->x_sr_khz);
        x->x_n_release = roundf(release * x->x_sr_khz);
        double coef_a;
        double coef_r;
        if (x->x_n_attack == 0)
            coef_a = 0.;
        else
            coef_a = 1. / (float)x->x_n_attack;
        if (x->x_n_release == 0)
            coef_r = 0.;
        else
            coef_r = 1. / (float)x->x_n_release;
        
        if (gate != gate_status){ // gate status change
            target = input;
            gate_status = gate;
            if (gate){
                if (x->x_n_attack > 1){
                    incr = (target - last) * x->x_coef_a;
                    nleft = x->x_n_attack;
                    *out++ = (last += incr);
                    continue;
                }
            }
            else {
                if (x->x_n_release > 1){
                    incr = (target - last) * x->x_coef_r;
                    nleft = x->x_n_release;
                    *out++ = (last += incr);
                    continue;
                }
            }
            incr = 0.;
            nleft = 0;
            *out++ = last = target;
        }
        
        else if(coef_a != x->x_coef_a || coef_r != x->x_coef_r){ // changed time
            x->x_coef_a = coef_a;
            x->x_coef_r = coef_r;
            if (target > last){ // if going up
                if (x->x_n_attack > 1){
                    incr = (target - last) * x->x_coef_a;
                    nleft = x->x_n_attack;
                    *out++ = (last += incr);
                    continue;
                    }
                }
            else if (target < last){ // if going down
                if (x->x_n_release > 1){
                    incr = (target - last) * x->x_coef_r;
                    nleft = x->x_n_release;
                    *out++ = (last += incr);
                    continue;
                }
            }
            incr = 0.;
            nleft = 0;
            *out++ = last = target;
            }
        
        else if (nleft > 0){
            *out++ = (last += incr);
            if (--nleft == 1){
                incr = 0.;
                last = target;
                }
            }
        else *out++ = target;
        };
    x->x_last = (PD_BIGORSMALL(last) ? 0. : last);
    x->x_target = (PD_BIGORSMALL(target) ? 0. : target);
    x->x_incr = incr;
    x->x_nleft = nleft;
    x->x_gate_status = gate_status;
    return (w + 7);
}

static void asr_dsp(t_asr *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    dsp_add(asr_perform, 6, x, sp[0]->s_n, sp[0]->s_vec,
        sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *asr_new(t_floatarg attack, t_floatarg release){
    t_asr *x = (t_asr *)pd_new(asr_class);
    x->x_sr_khz = sys_getsr() * 0.001;
    x->x_last = 0.;
    x->x_target = 0.;
    x->x_incr = 0.;
    x->x_nleft = 0;
    x->x_coef_a = 0.;
    x->x_coef_r = 0.;
    x->x_gate_status = 0;
    x->x_inlet_attack = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_attack, attack);
    x->x_inlet_release = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_release, release);
    outlet_new((t_object *)x, &s_signal);
    return (x);
}

void asr_tilde_setup(void){
    asr_class = class_new(gensym("asr~"), (t_newmethod)asr_new, 0,
				 sizeof(t_asr), 0, A_DEFFLOAT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(asr_class, t_asr, x_in);
    class_addmethod(asr_class, (t_method) asr_dsp, gensym("dsp"), A_CANT, 0);
}
