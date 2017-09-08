// porres 2017

#include "m_pd.h"

typedef struct _adr{
    t_object x_obj;
    t_float  x_in;
    t_inlet  *x_inlet_attack;
    t_inlet  *x_inlet_decay;
    t_inlet  *x_inlet_sustain;
    t_inlet  *x_inlet_release;
    int      x_n_attack;
    int      x_n_decay;
    int      x_n_release;
    t_float  x_last;
    t_float  x_target;
    t_float  x_sustain_target;
    t_float  x_sr_khz;
    double   x_incr;
    int      x_nleft;
    int      x_gate_status;
} t_adr;


static t_class *adr_class;

static t_int *adr_perform(t_int *w){
    t_adr *x = (t_adr *)(w[1]);
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
        t_float input = *in1++;
        t_float attack = *in2++;
        t_float decay = *in3++;
        t_float sustain_point = *in4++;
        t_float release = *in5++;
        t_int gate = (input != 0);
        if (attack < 0)
            attack = 0;
        if (decay < 0)
            decay = 0;
        if (release < 0)
            release = 0;
        x->x_n_attack = roundf(attack * x->x_sr_khz);
        x->x_n_decay = roundf(decay * x->x_sr_khz);
        x->x_n_release = roundf(release * x->x_sr_khz);
        double coef_a;
        double coef_d;
        double coef_r;
        if (x->x_n_attack == 0)
            coef_a = 0.;
        else
            coef_a = 1. / (float)x->x_n_attack;
        if (x->x_n_decay == 0)
            coef_d = 0.;
        else
            coef_d = 1. / (float)x->x_n_decay;
        if (x->x_n_release == 0)
            coef_r = 0.;
        else
            coef_r = 1. / (float)x->x_n_release;
        
        if (gate != gate_status){ // gate status change
            target = input;
            gate_status = gate;
            if (gate){
                if (x->x_n_attack > 1){
                    incr = (target - last) * coef_a;
                    nleft = x->x_n_attack;
                    *out++ = (last += incr);
                    continue;
                }
            }
            else {
                if (x->x_n_release > 1){
                    incr = (target - last) * coef_r;
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
    return (w + 9);
}

static void adr_dsp(t_adr *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    dsp_add(adr_perform, 8, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
            sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec);
}

static void *adr_new(t_floatarg attack, t_floatarg release){
    t_adr *x = (t_adr *)pd_new(adr_class);
    x->x_sr_khz = sys_getsr() * 0.001;
    x->x_last = 0.;
    x->x_target = 0.;
    x->x_incr = 0.;
    x->x_nleft = 0;
    x->x_gate_status = 0;
    x->x_inlet_attack = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_attack, attack);
    x->x_inlet_decay = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_decay, 0);
    x->x_inlet_sustain = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_sustain, 0);
    x->x_inlet_release = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_release, release);
    outlet_new((t_object *)x, &s_signal);
    return (x);
}

void adr_tilde_setup(void){
    adr_class = class_new(gensym("adr~"), (t_newmethod)adr_new, 0,
				 sizeof(t_adr), 0, A_DEFFLOAT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(adr_class, t_adr, x_in);
    class_addmethod(adr_class, (t_method) adr_dsp, gensym("dsp"), A_CANT, 0);
}
