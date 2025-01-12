// porres 2017

#include <m_pd.h>
#include <math.h>

#define LOG001 log(0.001)

typedef struct _asr{
    t_object x_obj;
    t_inlet  *x_inlet_attack;
    t_inlet  *x_inlet_sustain;
    t_inlet  *x_inlet_release;
    t_outlet *x_out2;
    int       x_nchans;
    int       x_n;
    t_float  x_sr_khz;
    t_float  x_f_gate;
    int      x_log;
    double   *x_incr;
    int      *x_nleft;
    int      *x_gate_status;
    int    *x_status;
    t_float  *x_last;
    t_float  *x_target;
} t_asr;

static t_class *asr_class;

static void asr_lin(t_asr *x, t_floatarg f){
    x->x_log = (int)(f == 0);
}

static void asr_float(t_asr *x, t_floatarg f){
    if(f != 0 && !x->x_status[0]) // on
        outlet_float(x->x_out2, x->x_status[0] = 1);
    x->x_f_gate = f/127;
}

static void asr_gate(t_asr *x, t_floatarg f){
    if(f != 0 && !x->x_status[0]) // on
        outlet_float(x->x_out2, x->x_status[0] = 1);
    x->x_f_gate = f;
}

static t_int *asr_perform(t_int *w){
    t_asr *x = (t_asr *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *in3 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    int ch2 = (int)(w[6]);
    int ch3 = (int)(w[7]);
    int n = x->x_n, chs = x->x_nchans;
    t_float *last = x->x_last;
    t_float *target = x->x_target;
    int *gate_status = x->x_gate_status;
    int *status = x->x_status;
    int *nleft = x->x_nleft;
    double *incr = x->x_incr;
    for(int j = 0; j < chs; j++){
        for(int i = 0; i < n; i++){
            t_float input_gate = in1[j*n + i];
            t_float attack = ch2 == 1 ? in2[i] : in2[j*n + i];
            t_float release = ch3 == 1 ? in3[i] : in3[j*n + i];
// get & clip 'n'; set a/r coefs
            t_float n_attack = roundf(attack * x->x_sr_khz);
            if(n_attack < 1)
                n_attack = 1;
            double coef_a = 1. / n_attack;
            t_float n_release = roundf(release * x->x_sr_khz);
            if(n_release < 1)
                n_release = 1;
            double coef_r = 1. / n_release;
            double a_coef = exp(LOG001 / n_attack);
            double r_coef = exp(LOG001 / n_release);
// Gate status / get incr & nleft values!
            t_int audio_gate = (input_gate != 0);
            t_int control_gate = (x->x_f_gate != 0);
            if((audio_gate || control_gate) != gate_status[j]){ // status changed
                gate_status[j] = audio_gate || x->x_f_gate;
                target[j] = x->x_f_gate != 0 ? x->x_f_gate : input_gate;
                if(gate_status[j]){ // if gate opened
                    if(!status[j])
                    outlet_float(x->x_out2, status[j] = 1);
                    incr[j] = (target[j] - last[j]) * coef_a;
                    nleft[j] = n_attack;
                }
                else{ // gate closed, set release incr
                    incr[j] =  -(last[j] * coef_r);
                    nleft[j] = n_release;
                }
            }
// "attack + decay + sustain" phase
            if(gate_status[j]){
                if(!x->x_log){ // linear
                    if(nleft[j] > 0){ // "attack" not over
                        out[j*n + i] = last[j] += incr[j];
                        nleft[j]--;
                    }
                    else // "sustain" phase
                        out[j*n + i] = last[j] = target[j];
                }
                else
                    out[j*n + i] = last[j] = target[j] + a_coef*(last[j] - target[j]);
            }
// "release" phase
            else{
                if(nleft[j] > 0){ // "release" not over
                    if(x->x_log)
                        out[j*n + i] = last[j] = target[j] + r_coef*(last[j] - target[j]);
                    else
                        out[j*n + i] = last[j] += incr[j];
                    nleft[j]--;
                }
                else{ // "release" over
                    if(status[j])
                        outlet_float(x->x_out2, status[j] = 0);
                    out[j*n + i] = last[j] = 0;
                }
            }
        }
        last[j] = (PD_BIGORSMALL(last[j]) ? 0. : last[j]);
        target[j] = (PD_BIGORSMALL(target[j]) ? 0. : target[j]);
    };
    x->x_last = last;
    x->x_target = target;
    x->x_incr = incr;
    x->x_nleft = nleft;
    x->x_gate_status = gate_status;
    x->x_status = status;
    return(w+8);
}

static void asr_dsp(t_asr *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    signal_setmultiout(&sp[3], chs);
    if(x->x_nchans != chs){
        x->x_incr = (double *)resizebytes(x->x_incr,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_nleft = (int *)resizebytes(x->x_nleft,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_gate_status = (int *)resizebytes(x->x_gate_status,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_status = (int *)resizebytes(x->x_status,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_target = (t_float *)resizebytes(x->x_target,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_last = (t_float *)resizebytes(x->x_last,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchans = chs;
    }
    int ch2 = sp[1]->s_nchans, ch3 = sp[2]->s_nchans;
    if((ch2 > 1 && ch2 != chs) || (ch3 > 1 && ch3 != chs)){
        dsp_add_zero(sp[3]->s_vec, chs*x->x_n);
        pd_error(x, "[asr~]: channel sizes mismatch");
        return;
    }
    dsp_add(asr_perform, 7, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
        sp[3]->s_vec, ch2, ch3);
}

static void *asr_free(t_asr *x){
    freebytes(x->x_incr, x->x_nchans * sizeof(*x->x_incr));
    freebytes(x->x_nleft, x->x_nchans * sizeof(*x->x_nleft));
    freebytes(x->x_status, x->x_nchans * sizeof(*x->x_status));
    freebytes(x->x_gate_status, x->x_nchans * sizeof(*x->x_gate_status));
    freebytes(x->x_last, x->x_nchans * sizeof(*x->x_last));
    freebytes(x->x_target, x->x_nchans * sizeof(*x->x_target));
    return(void *)x;
}

static void *asr_new(t_symbol *s, int ac, t_atom *av){
    t_asr *x = (t_asr *)pd_new(asr_class);
    t_symbol *cursym = s; // avoid warning
    x->x_sr_khz = sys_getsr() * 0.001;
    x->x_incr = (double *)getbytes(sizeof(*x->x_incr));
    x->x_nleft = (int *)getbytes(sizeof(*x->x_nleft));
    x->x_gate_status = (int *)getbytes(sizeof(*x->x_gate_status));
    x->x_status = (int *)getbytes(sizeof(*x->x_status));
    x->x_last = (t_float *)getbytes(sizeof(*x->x_last));
    x->x_target = (t_float *)getbytes(sizeof(*x->x_target));
    x->x_incr[0] = 0.;
    x->x_nleft[0] = x->x_gate_status[0] = x->x_status[0] = 0;
    x->x_last[0] = x->x_target[0] = 0.;
    x->x_log = 1;
    float a = 10, r = 10;
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
    class_addmethod(asr_class, (t_method) asr_dsp, gensym("dsp"), A_CANT, 0);
    class_addfloat(asr_class, (t_method)asr_float);
    class_addmethod(asr_class, (t_method)asr_gate, gensym("gate"), A_DEFFLOAT, 0);
    class_addmethod(asr_class, (t_method)asr_lin, gensym("lin"), A_DEFFLOAT, 0);
}
