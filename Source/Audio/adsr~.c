 // porres 2017-2025

#include <m_pd.h>
#include <math.h>

#define LOG001 log(0.001)

typedef struct _adsr{
    t_object x_obj;
    int         x_kretrig; // control retrigger
    int         x_bang;    // control impulse
    int         x_log;
    int         x_rel;
    int         x_nchans;
    int         x_n;
    t_inlet    *x_inlet_attack;
    t_inlet    *x_inlet_decay;
    t_inlet    *x_inlet_sustain;
    t_inlet    *x_inlet_release;
    t_outlet   *x_out2;
    t_float     x_f_gate;
    t_float     x_last_kgate;
    t_float     x_last_gate;
    t_float     x_sustain_target;
    t_float     x_sr_khz;
    double     *x_incr;
    double     *x_delta; // new
    double     *x_phase; // new, for knowing where the ouput should be in log mode
    int        *x_gate_status;
    int        *x_attacked;
    int        *x_decayed;    // new
    int        *x_sustained;  // new
    int        *x_released;   // new
    int        x_status;
    t_float    *x_last;
    t_float    *x_target;
}t_adsr;

static t_class *adsr_class;

static void adsr_lin(t_adsr *x, t_floatarg f){
    x->x_log = (int)(f == 0);
}

static void adsr_rel(t_adsr *x, t_floatarg f){
    x->x_rel = (int)(f != 0);
}

static void adsr_bang(t_adsr *x){ // control impulse
    if(!x->x_gate_status[0]){
        x->x_f_gate = x->x_last_kgate;
        x->x_bang = 1;
    }
}

static void adsr_retrigger(t_adsr *x){
    if(x->x_attacked[0]){ // needs to be attacked
        x->x_f_gate = x->x_last_kgate;
        x->x_kretrig = 1;
    }
}

static void adsr_gate(t_adsr *x, t_floatarg f){
    x->x_f_gate = f;
    if(x->x_f_gate != 0){
        x->x_last_kgate = x->x_f_gate;
        if(!x->x_status) // it's off, so trigger it on
            outlet_float(x->x_out2, x->x_status = 1);
        else // retrigger
            x->x_kretrig = 1;
    }
}

static void adsr_float(t_adsr *x, t_floatarg f){
    adsr_gate(x, f / 127);
}

static t_int *adsr_perform(t_int *w){
    t_adsr *x = (t_adsr *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *in3 = (t_float *)(w[4]);
    t_float *in4 = (t_float *)(w[5]);
    t_float *in5 = (t_float *)(w[6]);
    t_float *in6 = (t_float *)(w[7]);
    t_float *out = (t_float *)(w[8]);
    int ch2 = (int)(w[9]);
    int ch3 = (int)(w[10]);
    int ch4 = (int)(w[11]);
    int ch5 = (int)(w[12]);
    int ch6 = (int)(w[13]);
    int n = x->x_n, chs = x->x_nchans;
    t_float *last = x->x_last;
    t_float *target = x->x_target;
    int *gate_status = x->x_gate_status;
    int *attacked = x->x_attacked;
    int *decayed = x->x_decayed;
    int *sustained = x->x_sustained;
    int *released = x->x_released;
    double *delta = x->x_delta;
    double *phase = x->x_phase;
    double *incr = x->x_incr;
    for(int j = 0; j < chs; j++){
        for(int i = 0; i < n; i++){
            t_float input_gate = in1[j*n + i];
            t_float retrig = ch2 == 1 ? in2[i] : in2[j*n + i];
            t_float attack = ch3 == 1 ? in3[i] : in3[j*n + i];
            t_float decay = ch4 == 1 ? in4[i] : in4[j*n + i];
            t_float sustain_point = ch5 == 1 ? in5[i] : in5[j*n + i];
            t_float release = ch6 == 1 ? in6[i] : in6[j*n + i];
// sustain bound check
            if(sustain_point < 0)
                sustain_point = 0;
            if(sustain_point > 1)
                sustain_point = 1;
// Gate status
            t_int audio_gate = (input_gate != 0);
            t_int control_gate = (x->x_f_gate != 0);
// get 'n' samples for each stage
            // number of samples in the attack
            t_float n_attack = roundf(attack * x->x_sr_khz);
            if(n_attack < 1)
                n_attack = 1;
            // number of samples in the decay
            t_float n_decay = roundf(decay * x->x_sr_khz);
            if(n_decay < 1)
                n_decay = 1;
            // number of samples in the release
            t_float n_release = roundf(release * x->x_sr_khz);
            if(n_release < 1)
                n_release = 1;
// trigger
            if(x->x_kretrig){ // control trigger
                target[j] = x->x_f_gate;
                phase[j] = last[j];
                delta[j] = target[j] - last[j];
                decayed[j] = sustained[j] = 0;
                attacked[j] = 1;
                x->x_kretrig = 0;
            } // else check for audio gate status
            else if((audio_gate || control_gate) != gate_status[j]){ // status changed
                gate_status[j] = audio_gate || x->x_f_gate; // update status
                if(gate_status[j]){ // if gate opened
                    // get target, control or audio
                    x->x_last_gate = x->x_f_gate != 0 ? x->x_f_gate : input_gate;
                    target[j] = x->x_last_gate;
                    attacked[j] = 1; // tag enveloped as "attacked"
                    decayed[j] = sustained[j] = 0;
                    if(!x->x_status)
                        outlet_float(x->x_out2, x->x_status = 1);
                    phase[j] = last[j];
                    delta[j] =  target[j] - last[j];
                    incr[j] = delta[j] / n_attack;
                    if(x->x_bang){
                        x->x_bang = 0;
                        x->x_f_gate = 0;
                    }
                }
                else{ // gate closed
                    if(x->x_rel){ // if immediate release mode
                        delta[j] =  -last[j];
                        attacked[j] = decayed[j] = sustained[j] = 0;
                    }
                }
            }
            else if(attacked[j] && retrig != 0){ // signal retrigger
                target[j] = x->x_last_gate > 0 ? retrig : -retrig;
                phase[j] = last[j];
                delta[j] = target[j] - last[j];
                decayed[j] = sustained[j] = 0;
            }
// "attack + decay + sustain" phase
            if(attacked[j]){
                if(!sustained[j]){ // attack or decay stage
                    double fcoeff;
                    if(!decayed[j]){ // attack stage
                        incr[j] = delta[j] / n_attack;
                        fcoeff = exp(LOG001 / n_attack);
                    }
                    else if(!sustained[j]){ // decay phase
                        incr[j] = delta[j] / n_decay;
                        fcoeff = exp(LOG001 / n_decay);
                    }
                    float output;
                    phase[j] += incr[j];
                    if(!x->x_log) // linear
                        output = phase[j];
                    else
                        output = target[j] + (last[j] - target[j])*fcoeff;
                    if(!decayed[j]){ // attack phase
                        int finished = 0;
                        if(target[j] > 0){ // positive gate
                            if(phase[j] >= target[j])
                                finished = 1;
                        }
                        else{
                            if(phase[j] <= target[j])
                                finished = 1;
                        }
                        if(finished){ // reached target, change stage
                            phase[j] = target[j];
                            if(!x->x_log)
                                output = phase[j];
                            decayed[j] = 1;
                            delta[j] = target[j]*sustain_point - target[j];
                            target[j] = target[j]*sustain_point;
                        }
                    }
                    else if(!sustained[j]){ // decay phase
                        int finished = 0;
                        if(delta[j] < 0){ // positive gate
                            if(phase[j] <= target[j])
                                finished = 1;
                        }
                        else{
                            if(phase[j] >= target[j])
                                finished = 1;
                        }
                        if(finished){ // reached target, change stage
                            phase[j] = target[j];
                            if(!x->x_log)
                                output = target[j];
                            sustained[j] = 1;
                            delta[j] = 0;
                        }
                    }
                    out[j*n + i] = last[j] = output;
                }
                else{ // "sustain" phase
                    out[j*n + i] = last[j] = phase[j] = target[j];
                    if(!gate_status[j]){ // gate off, set to release
                        delta[j] =  -last[j];
                        target[j] = 0;
                        attacked[j] = decayed[j] = sustained[j] = 0;
                    }
                }
            }
// "release" phase
            else{
                incr[j] = delta[j] / n_release;
                phase[j] += incr[j];
                float output;
                if(!x->x_log) // linear
                    output = phase[j];
                else{
                    float r_coeff = exp(LOG001 / n_release);
                    output = last[j] * r_coeff;
                }
                int finished = 0;
                if(incr[j] < 0){ // positive gate
                    if(phase[j] <= 0)
                        finished = 1;
                }
                else{
                    if(phase[j] >= 0)
                        finished = 1;
                }
                if(finished){
                    phase[j] = output = 0;
                    if(x->x_status){
                        int done = 1;
                        for(int ch = 0; ch < x->x_nchans; ch++){
                            if(phase[ch] != 0){
                                done = 0;
                                break;
                            }
                        }
                        if(done) // turn off global status
                            outlet_float(x->x_out2, x->x_status = 0);
                    }
                }
                out[j*n + i] = last[j] = output;
            }
        }
        last[j] = (PD_BIGORSMALL(last[j]) ? 0. : last[j]);
    };
    x->x_last = last;
    x->x_target = target;
    x->x_incr = incr;
    x->x_phase = phase;
    x->x_delta = delta;
    x->x_gate_status = gate_status;
    x->x_attacked = attacked;
    x->x_decayed = decayed;
    x->x_sustained = sustained;
    x->x_released = released;
    return(w+14);
}

static void adsr_dsp(t_adsr *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    signal_setmultiout(&sp[6], chs);
    if(x->x_nchans != chs){
        x->x_incr = (double *)resizebytes(x->x_incr,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_delta = (double *)resizebytes(x->x_delta,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_phase = (double *)resizebytes(x->x_phase,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_gate_status = (int *)resizebytes(x->x_gate_status,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_attacked = (int *)resizebytes(x->x_attacked,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_decayed = (int *)resizebytes(x->x_decayed,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_sustained = (int *)resizebytes(x->x_sustained,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_released = (int *)resizebytes(x->x_released,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_target = (t_float *)resizebytes(x->x_target,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_last = (t_float *)resizebytes(x->x_last,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchans = chs;
    }
    int ch2 = sp[1]->s_nchans, ch3 = sp[2]->s_nchans, ch4 = sp[3]->s_nchans;
    int ch5 = sp[4]->s_nchans, ch6 = sp[5]->s_nchans;
    if((ch2 > 1 && ch2 != chs) || (ch3 > 1 && ch3 != chs) || (ch4 > 1 && ch4 != chs)
    || (ch5 > 1 && ch5 != chs) || (ch6 > 1 && ch6 != chs)){
        dsp_add_zero(sp[6]->s_vec, chs*x->x_n);
        pd_error(x, "[adsr~]: channel sizes mismatch");
        return;
    }
    dsp_add(adsr_perform, 13, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
        sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec, ch2, ch3, ch4, ch5, ch6);
}

static void *adsr_free(t_adsr *x){
    freebytes(x->x_incr, x->x_nchans * sizeof(*x->x_incr));
    freebytes(x->x_delta, x->x_nchans * sizeof(*x->x_delta));
    freebytes(x->x_phase, x->x_nchans * sizeof(*x->x_phase));
    freebytes(x->x_gate_status, x->x_nchans * sizeof(*x->x_gate_status));
    freebytes(x->x_attacked, x->x_nchans * sizeof(*x->x_attacked));
    freebytes(x->x_decayed, x->x_nchans * sizeof(*x->x_decayed));
    freebytes(x->x_sustained, x->x_nchans * sizeof(*x->x_sustained));
    freebytes(x->x_released, x->x_nchans * sizeof(*x->x_released));
    freebytes(x->x_last, x->x_nchans * sizeof(*x->x_last));
    freebytes(x->x_target, x->x_nchans * sizeof(*x->x_target));
    return(void *)x;
}

static void *adsr_new(t_symbol *sym, int ac, t_atom *av){
    t_adsr *x = (t_adsr *)pd_new(adsr_class);
    t_symbol *cursym = sym; // avoid warning
    x->x_sr_khz = sys_getsr() * 0.001;
    float a = 10, d = 10, s = 1, r = 10;
    x->x_incr = (double *)getbytes(sizeof(*x->x_incr));
    x->x_delta = (double *)getbytes(sizeof(*x->x_delta));
    x->x_phase = (double *)getbytes(sizeof(*x->x_phase));
    x->x_gate_status = (int *)getbytes(sizeof(*x->x_gate_status));
    x->x_attacked = (int *)getbytes(sizeof(*x->x_attacked));
    x->x_decayed = (int *)getbytes(sizeof(*x->x_decayed));
    x->x_sustained = (int *)getbytes(sizeof(*x->x_sustained));
    x->x_released = (int *)getbytes(sizeof(*x->x_released));
    x->x_last = (t_float *)getbytes(sizeof(*x->x_last));
    x->x_target = (t_float *)getbytes(sizeof(*x->x_target));
    x->x_incr[0] = x->x_delta[0] = x->x_phase[0] = 0.;
    x->x_gate_status[0] = 0;
    x->x_attacked[0] = x->x_decayed[0] = 0;
    x->x_sustained[0] = x->x_released[0] = 0;
    x->x_last[0] = x->x_target[0] = 0.;
    x->x_last_kgate = 1;
    x->x_last_gate = 0;
    x->x_status = 0;
    x->x_kretrig = 0;
    x->x_bang = 0;
    x->x_log = 1;
    x->x_rel = 0; // release mode
    int symarg = 0;
    int argnum = 0;
    while(ac > 0){
        if(av->a_type == A_FLOAT){
            float argval = atom_getfloatarg(0, ac, av);
            switch(argnum){
                case 0:
                    a = argval;
                    break;
                case 1:
                    d = argval;
                    break;
                case 2:
                    s = argval;
                    break;
                case 3:
                    r = argval;
                    break;
                default:
                    break;
            };
            argnum++;
            ac--;
            av++;
        }
        else if(av->a_type == A_SYMBOL && !symarg && !argnum){
            symarg = 1;
            cursym = atom_getsymbolarg(0, ac, av);
            if(cursym == gensym("-lin")){
                ac--, av++;
                x->x_log = 0;
            }
            else if(cursym == gensym("-rel")){
                ac--, av++;
                x->x_rel = 1;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    }
    inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_inlet_attack = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_attack, a);
    x->x_inlet_decay = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_decay, d);
    x->x_inlet_sustain = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_sustain, s);
    x->x_inlet_release = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_release, r);
    outlet_new((t_object *)x, &s_signal);
    x->x_out2 = outlet_new((t_object *)x, &s_float);
    return(x);
errstate:
    pd_error(x, "[adsr~]: improper args");
    return(NULL);
}

void adsr_tilde_setup(void){
    adsr_class = class_new(gensym("adsr~"), (t_newmethod)adsr_new, (t_method)adsr_free,
        sizeof(t_adsr), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(adsr_class, nullfn, gensym("signal"), 0);
    class_addmethod(adsr_class, (t_method)adsr_dsp, gensym("dsp"), A_CANT, 0);
    class_addbang(adsr_class, (t_method)adsr_bang);
    class_addfloat(adsr_class, (t_method)adsr_float);
    class_addmethod(adsr_class, (t_method)adsr_gate, gensym("gate"), A_DEFFLOAT, 0);
    class_addmethod(adsr_class, (t_method)adsr_retrigger, gensym("retrigger"), A_DEFFLOAT, 0);
    class_addmethod(adsr_class, (t_method)adsr_lin, gensym("lin"), A_DEFFLOAT, 0);
    class_addmethod(adsr_class, (t_method)adsr_rel, gensym("rel"), A_DEFFLOAT, 0);
}
