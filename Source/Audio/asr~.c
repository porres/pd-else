 // porres 2017-2025

#include <m_pd.h>
#include <math.h>

#define LOG001 log(0.001)

typedef struct _asr{
    t_object x_obj;
    int         x_bang;    // control impulse
    int         x_lag;
    int         x_nchans;
    int         x_n;
    t_inlet    *x_inlet_attack;
    t_inlet    *x_inlet_release;
    t_inlet    *x_inlet_mul;
    t_outlet   *x_out2;
    t_float     x_f_gate;
    t_float     x_last_kgate;
    t_float     x_last_gate;
    t_float     x_sr_khz;
    t_float     x_minsus;
    t_float     x_maxsus;
    int        *x_n_minsus;
    int        *x_n_maxsus;
    int        *x_gate_status;
    int        *x_attacked;
    int        *x_sustained;
    int        *x_released;
    double     *x_a;
    double     *x_b;
    double     *x_incr;
    double     *x_delta; // new
    double     *x_phase; // new, for knowing where the ouput should be in log mode
    float       x_curve;
    int        x_status;
    t_float    *x_last;
    t_float    *x_target;
}t_asr;

static t_class *asr_class;

static void asr_lin(t_asr *x){
    x->x_lag = x->x_curve = 0;
}

static void asr_lag(t_asr *x){
    x->x_lag = 1;
}

static void asr_curve(t_asr *x, t_floatarg f){
    x->x_curve = f * -4;
    x->x_lag = 0;
}

static void asr_rel(t_asr *x, t_floatarg f){
    pd_error(x, "[asr~]: please stop useing 'rel' message, use 'minsus' now");
    x->x_minsus = f > 0 ? -1 : 0;
}

static void asr_minsus(t_asr *x, t_floatarg f){
    x->x_minsus = f;
}

static void asr_maxsus(t_asr *x, t_floatarg f){
    x->x_maxsus = f;
}

static void asr_bang(t_asr *x){ // control impulse
    if(!x->x_gate_status[0]){
        x->x_f_gate = x->x_last_kgate;
        x->x_bang = 1;
    }
}

static void asr_gate(t_asr *x, t_floatarg f){
    x->x_f_gate = f;
    if(x->x_f_gate != 0){
        x->x_last_kgate = x->x_f_gate;
        if(!x->x_status) // it's off, so trigger it on
            outlet_float(x->x_out2, x->x_status = 1);
    }
}

static void asr_float(t_asr *x, t_floatarg f){
    asr_gate(x, f / 127);
}

static t_int *asr_perform(t_int *w){
    t_asr *x = (t_asr *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *in3 = (t_float *)(w[4]);
    t_float *in4 = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);
    int ch2 = (int)(w[7]);
    int ch3 = (int)(w[8]);
    int ch4 = (int)(w[9]);
    int n = x->x_n, chs = x->x_nchans;
    t_float *last = x->x_last;
    t_float *target = x->x_target;
    int *gate_status = x->x_gate_status;
    int *attacked = x->x_attacked;
    int *sustained = x->x_sustained;
    int *released = x->x_released;
    int *n_minsus = x->x_n_minsus;
    int *n_maxsus = x->x_n_maxsus;
    double *delta = x->x_delta;
    double *phase = x->x_phase;
    double *incr = x->x_incr;
    for(int j = 0; j < chs; j++){
        for(int i = 0; i < n; i++){
            t_float input_gate = in1[j*n + i];
            t_float mul = ch4 == 1 ? in4[i] : in4[j*n + i];
            t_float attack = (ch2 == 1 ? in2[i] : in2[j*n + i]) * mul;
            t_float release = (ch3 == 1 ? in3[i] : in3[j*n + i]) * mul;
// Gate status
            t_int audio_gate = (input_gate != 0);
            t_int control_gate = (x->x_f_gate != 0);
// get 'n' samples for each stage
            // number of samples in the attack
            t_float n_attack = roundf(attack * x->x_sr_khz);
            if(n_attack < 1)
                n_attack = 1;
            // number of samples in the release
            t_float n_release = roundf(release * x->x_sr_khz);
            if(n_release < 1)
                n_release = 1;
            // number of samples in min/max sustain
            int n_minsustain = (int)roundf(x->x_minsus * mul * x->x_sr_khz);
            int n_maxsustain = (int)roundf(x->x_maxsus * mul * x->x_sr_khz);
// trigger
            if((audio_gate || control_gate) != gate_status[j]){ // status changed
                gate_status[j] = audio_gate || x->x_f_gate; // update status
                if(gate_status[j]){ // if gate opened
                    n_minsus[j] = n_maxsus[j] = 0; // zero sustain count
                    // get target, control or audio
                    x->x_last_gate = x->x_f_gate != 0 ? x->x_f_gate : input_gate;
                    target[j] = x->x_last_gate;
                    attacked[j] = 1; // tag envelope as "attacked"
                    sustained[j] = 0;
                    delta[j] =  target[j] - last[j];
                    
                    x->x_b[j] = x->x_delta[j] / (1 - exp(x->x_curve));
                    x->x_a[j] = last[j] + x->x_b[j];
                    
                    if(!x->x_status)
                        outlet_float(x->x_out2, x->x_status = 1);
                    phase[j] = last[j];
                    incr[j] = delta[j] / n_attack;
                    if(x->x_bang){
                        x->x_bang = 0;
                        x->x_f_gate = 0;
                    }
                }
                else{ // gate closed
                    if(x->x_minsus < 0){ // if immediate release mode
                        target[j] = 0;
                        phase[j] = last[j];
                        delta[j] = -last[j];
                        attacked[j] = sustained[j] = 0;
                        
                        x->x_b[j] = x->x_delta[j] / (1 - exp(x->x_curve));
                        x->x_a[j] = last[j] + x->x_b[j];
                    }
                }
            }
// "attack + sustain" phase
            if(attacked[j]){
                if(!sustained[j]){ // attack stage
                    float output;
                    phase[j] += incr[j];
                    if(x->x_lag){
                        double fcoeff = exp(LOG001 / n_attack);
                        output = target[j] + (last[j] - target[j])*fcoeff;
                    }
                    else{
                        if(fabs(x->x_curve) > 0.001){
                            x->x_b[j] *= exp(x->x_curve / n_attack);
                            output = last[j] = x->x_a[j] - x->x_b[j];
                        }
                        else // linear
                            output = phase[j];
                    }
                    if(!sustained[j]){ // attack phase
                        int finished = 0;
                        if(delta[j] < 0){ // positive gate
                            if(phase[j] <= target[j])
                                finished = 1;
                        }
                        else{
                            if(phase[j] >= target[j])
                                finished = 1;
                        }
                        if(finished){ // reached target, change to sustain stage
                            phase[j] = target[j];
                            output = target[j];
                            sustained[j] = 1;
                            delta[j] = 0;
                        }
                    }
                    out[j*n + i] = last[j] = output;
                }
                else{ // "sustain" phase
                    out[j*n + i] = last[j] = phase[j] = target[j];
                    n_minsus[j]++;
                    n_maxsus[j]++;
                    int status = ((!gate_status[j]
                        && (n_minsus[j] > n_minsustain))
                        || (x->x_maxsus >= 0
                        && (n_maxsus[j] > n_maxsustain)));
                    if(status){ // gate off, set to release
                        delta[j] =  -last[j];
                        target[j] = 0;
                        attacked[j] = sustained[j] = 0;
                        
                        x->x_b[j] = x->x_delta[j] / (1 - exp(x->x_curve));
                        x->x_a[j] = last[j] + x->x_b[j];
                    }
                }
            }
// "release" phase
            else{
                incr[j] = delta[j] / n_release;
                phase[j] += incr[j];
                float output;
                if(x->x_lag){
                    float r_coeff = exp(LOG001 / n_release);
                    output = last[j] * r_coeff;
                }
                else{
                    if(fabs(x->x_curve) > 0.001){
                        x->x_b[j] *= exp(x->x_curve / n_release); // inc
                        output = last[j] = x->x_a[j] - x->x_b[j];
                    }
                    else // linear
                        output = phase[j];
                }
                int finished = 0;
                if(incr[j] < 0){ // positive gate
                    if(phase[j] <= 0)
                        finished = 1;
                }
                else{ // negative gate
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
    x->x_sustained = sustained;
    x->x_released = released;
    return(w+10);
}

static void asr_dsp(t_asr *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    signal_setmultiout(&sp[4], chs);
    if(x->x_nchans != chs){
        x->x_a = (double *)resizebytes(x->x_a,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_b = (double *)resizebytes(x->x_b,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_incr = (double *)resizebytes(x->x_incr,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_delta = (double *)resizebytes(x->x_delta,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_phase = (double *)resizebytes(x->x_phase,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_n_minsus = (int *)resizebytes(x->x_n_minsus,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_n_maxsus = (int *)resizebytes(x->x_n_maxsus,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_gate_status = (int *)resizebytes(x->x_gate_status,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_attacked = (int *)resizebytes(x->x_attacked,
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
    if((ch2 > 1 && ch2 != chs) || (ch3 > 1 && ch3 != chs)
    || (ch4 > 1 && ch4 != chs)){
        dsp_add_zero(sp[4]->s_vec, chs*x->x_n);
        pd_error(x, "[asr~]: channel sizes mismatch");
        return;
    }
    dsp_add(asr_perform, 9, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
        sp[3]->s_vec, sp[4]->s_vec, ch2, ch3, ch4);
}

static void *asr_free(t_asr *x){
    freebytes(x->x_a, x->x_nchans * sizeof(*x->x_a));
    freebytes(x->x_b, x->x_nchans * sizeof(*x->x_b));
    freebytes(x->x_incr, x->x_nchans * sizeof(*x->x_incr));
    freebytes(x->x_delta, x->x_nchans * sizeof(*x->x_delta));
    freebytes(x->x_phase, x->x_nchans * sizeof(*x->x_phase));
    freebytes(x->x_n_minsus, x->x_nchans * sizeof(*x->x_n_minsus));
    freebytes(x->x_n_maxsus, x->x_nchans * sizeof(*x->x_n_maxsus));
    freebytes(x->x_gate_status, x->x_nchans * sizeof(*x->x_gate_status));
    freebytes(x->x_attacked, x->x_nchans * sizeof(*x->x_attacked));
    freebytes(x->x_sustained, x->x_nchans * sizeof(*x->x_sustained));
    freebytes(x->x_released, x->x_nchans * sizeof(*x->x_released));
    freebytes(x->x_last, x->x_nchans * sizeof(*x->x_last));
    freebytes(x->x_target, x->x_nchans * sizeof(*x->x_target));
    return(void *)x;
}

static void *asr_new(t_symbol *sym, int ac, t_atom *av){
    t_asr *x = (t_asr *)pd_new(asr_class);
    t_symbol *cursym = sym; // avoid warning
    x->x_sr_khz = sys_getsr() * 0.001;
    float a = 10, r = 10, mul = 1;
    x->x_a = (double *)getbytes(sizeof(*x->x_a));
    x->x_b = (double *)getbytes(sizeof(*x->x_b));
    x->x_incr = (double *)getbytes(sizeof(*x->x_incr));
    x->x_delta = (double *)getbytes(sizeof(*x->x_delta));
    x->x_phase = (double *)getbytes(sizeof(*x->x_phase));
    x->x_n_minsus = (int *)getbytes(sizeof(*x->x_n_minsus));
    x->x_n_maxsus = (int *)getbytes(sizeof(*x->x_n_maxsus));
    x->x_gate_status = (int *)getbytes(sizeof(*x->x_gate_status));
    x->x_attacked = (int *)getbytes(sizeof(*x->x_attacked));
    x->x_sustained = (int *)getbytes(sizeof(*x->x_sustained));
    x->x_released = (int *)getbytes(sizeof(*x->x_released));
    x->x_last = (t_float *)getbytes(sizeof(*x->x_last));
    x->x_target = (t_float *)getbytes(sizeof(*x->x_target));
    x->x_a[0] = x->x_b[0] = x->x_incr[0] = x->x_delta[0] = x->x_phase[0] = 0.;
    x->x_gate_status[0] = 0;
    x->x_attacked[0] = 0;
    x->x_sustained[0] = x->x_released[0] = 0;
    x->x_last[0] = x->x_target[0] = 0.;
    x->x_last_kgate = 1;
    x->x_last_gate = 0;
    x->x_status = 0;
    x->x_curve = -4;
    x->x_bang = 0;
    x->x_lag = 1;
    x->x_minsus = -1;
    x->x_maxsus = -1;
    int argnum = 0;
    x->x_lag = 0;
    while(ac > 0){
        if(av->a_type == A_FLOAT){
            float argval = atom_getfloat(av);
            switch(argnum){
                case 0:
                    a = argval;
                    break;
                case 1:
                    r = argval;
                    break;
                case 2:
                    x->x_curve = argval * -4;
                    break;
                case 3:
                    mul = argval;
                    break;
                default:
                    break;
            };
            argnum++;
            ac--;
            av++;
        }
        else if(av->a_type == A_SYMBOL && !argnum){
            cursym = atom_getsymbolarg(0, ac, av);
            if(cursym == gensym("-lin")){
                ac--, av++;
                x->x_curve = 0;
            }
            else if(cursym == gensym("-lag")){
                ac--, av++;
                x->x_lag = 1;
            }
            else if(cursym == gensym("-rel")){
                pd_error(x, "[asr~]: don't use '-rel' anymore please, it's the default now");
                ac--, av++;
            }
            else if(cursym == gensym("-minsus")){
                if(ac >= 2){
                    ac--, av++;
                    x->x_minsus = atom_getfloat(av);
                    ac--, av++;
                }
                else
                    goto errstate;
            }
            else if(cursym == gensym("-maxsus")){
                if(ac >= 2){
                    ac--, av++;
                    x->x_maxsus = atom_getfloat(av);
                    ac--, av++;
                }
                else
                    goto errstate;
            }
            else if(cursym == gensym("-curve")){
                if(ac >= 2){
                    ac--, av++;
                    x->x_curve = atom_getfloat(av) * -4;
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
    x->x_inlet_attack = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_attack, a);
    x->x_inlet_release = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_release, r);
    x->x_inlet_mul = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_mul, mul);
    outlet_new((t_object *)x, &s_signal);
    x->x_out2 = outlet_new((t_object *)x, &s_float);
    return(x);
errstate:
    pd_error(x, "[asr~]: improper args");
    return(NULL);
}

void asr_tilde_setup(void){
    asr_class = class_new(gensym("asr~"), (t_newmethod)asr_new, (t_method)asr_free,
        sizeof(t_asr), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(asr_class, nullfn, gensym("signal"), 0);
    class_addmethod(asr_class, (t_method)asr_dsp, gensym("dsp"), A_CANT, 0);
    class_addbang(asr_class, (t_method)asr_bang);
    class_addfloat(asr_class, (t_method)asr_float);
    class_addmethod(asr_class, (t_method)asr_gate, gensym("gate"), A_FLOAT, 0);
    class_addmethod(asr_class, (t_method)asr_lin, gensym("lin"), 0);
    class_addmethod(asr_class, (t_method)asr_lag, gensym("lag"), 0);
    class_addmethod(asr_class, (t_method)asr_rel, gensym("rel"), A_FLOAT, 0);
    class_addmethod(asr_class, (t_method)asr_minsus, gensym("minsus"), A_FLOAT, 0);
    class_addmethod(asr_class, (t_method)asr_maxsus, gensym("maxsus"), A_FLOAT, 0);
    class_addmethod(asr_class, (t_method)asr_curve, gensym("curve"), A_FLOAT, 0);
}
