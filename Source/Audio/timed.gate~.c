// Porres 2016-2025

#include <m_pd.h>

static t_class *timed_gate_class;

typedef struct _timed_gate{
    t_object  x_obj;
    float     x_sr;
    float     x_sr_khz;
    int      *x_sum;
    float    *x_lastin;
    float    *x_gate_value;
    int       x_retrigger;
    int       x_nchans;
    int       x_n;
    t_inlet  *x_inlet_ms;
    t_outlet *x_gate_out;
    t_outlet *x_ignore_out;
}t_timed_gate;

static void timed_gate_float(t_timed_gate *x, t_float f){
    if(f == 0)
        return;
    x->x_sum[0] = 0;
    x->x_gate_value[0] = f;
}

static void timed_gate_ms(t_timed_gate *x, t_float f){
    if(f >= 0)
        pd_float((t_pd *)x->x_inlet_ms, f);
}

static void timed_gate_retrigger(t_timed_gate *x, t_float f){
    x->x_retrigger = f != 0;
}

static void timed_gate_bang(t_timed_gate *x){
    x->x_sum[0] = 0;
    if(x->x_gate_value[0] == 0)
        x->x_gate_value[0] = 1;
}

static t_int *timed_gate_perform(t_int *w){
    t_timed_gate *x = (t_timed_gate *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *out1 = (t_float *)(w[4]);
    t_float *out2 = (t_float *)(w[5]);
    int ch2 = (int)(w[6]);
    int n = x->x_n, chs = x->x_nchans;
    int *sum = x->x_sum;
    t_float sr_khz = x->x_sr_khz;
    t_float *gate_value = x->x_gate_value;
    t_float *lastin = x->x_lastin;
    for(int j = 0; j < chs; j++){
        for(int i = 0; i < n; i++){
            t_float in = in1[j*n + i];
            t_float ms = ch2 == 1 ? in2[i] : in2[j*n + i];
            t_int samps = (int)((ms * sr_khz)+0.5f);
            t_float gate = ((sum[j] += 1) <= samps);
            float output2 = 0;
            if(x->x_retrigger){
                if(in != 0 && lastin[j] == 0){
                    if(gate)
                        output2 = in;
                    sum[j] = 0;
                    gate_value[j] = in;
                }
            }
            else{
                if(in != 0 && lastin[j] == 0){
                    if(gate)
                        output2 = in;
                    else{
                        sum[j] = 0;
                        gate_value[j] = in;
                    }
                }
            }
            out1[j*n + i] =  gate ? gate_value[j] : 0;
            out2[j*n + i] =  output2;
            lastin[j] = in;
        }
    }
    x->x_sum = sum;
    x->x_gate_value = gate_value;
    x->x_lastin = lastin;
    return(w+7);
}

static void timed_gate_dsp(t_timed_gate *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    signal_setmultiout(&sp[2], chs);
    signal_setmultiout(&sp[3], chs);
    if(x->x_nchans != chs){
        x->x_sum = (int *)resizebytes(x->x_sum,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_gate_value = (t_float *)resizebytes(x->x_gate_value,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_lastin = (t_float *)resizebytes(x->x_lastin,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchans = chs;
    }
    int ch2 = sp[1]->s_nchans;
    if(ch2 > 1 && ch2 != chs){
        dsp_add_zero(sp[2]->s_vec, chs*x->x_n);
        dsp_add_zero(sp[3]->s_vec, chs*x->x_n);
        pd_error(x, "[timed.gate~]: channel sizes mismatch");
        return;
    }
    dsp_add(timed_gate_perform, 6, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec, ch2);
}

static void *timed_gate_free(t_timed_gate *x){
    freebytes(x->x_sum, x->x_nchans * sizeof(*x->x_sum));
    freebytes(x->x_gate_value, x->x_nchans * sizeof(*x->x_gate_value));
    freebytes(x->x_lastin, x->x_nchans * sizeof(*x->x_lastin));
    outlet_free(x->x_gate_out);
    outlet_free(x->x_ignore_out);
    return(void *)x;
}

static void *timed_gate_new(t_floatarg f1, t_floatarg f2){
    t_timed_gate *x = (t_timed_gate *)pd_new(timed_gate_class);
    x->x_sum = (int *)getbytes(sizeof(*x->x_sum));
    x->x_lastin = (t_float *)getbytes(sizeof(*x->x_lastin));
    x->x_gate_value = (t_float *)getbytes(sizeof(*x->x_gate_value));
    x->x_lastin[0] = x->x_gate_value[0] = 0.;
    x->x_sr = sys_getsr();
    x->x_sr_khz = x->x_sr * 0.001;
    float ms = f1 < 0 ? 0 : f1;
    int time_in_samps = (int)(ms * x->x_sr_khz + 0.5f);
    x->x_sum[0] = time_in_samps + 1;
    x->x_retrigger = f2 != 0;
    x->x_inlet_ms = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_ms, ms);
    x->x_gate_out = outlet_new(&x->x_obj, &s_signal);
    x->x_ignore_out = outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void setup_timed0x2egate_tilde(void){
    timed_gate_class = class_new(gensym("timed.gate~"),
        (t_newmethod)timed_gate_new, (t_method)timed_gate_free, sizeof(t_timed_gate),
        CLASS_MULTICHANNEL, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(timed_gate_class, nullfn, gensym("signal"), 0);
    class_addmethod(timed_gate_class, (t_method) timed_gate_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(timed_gate_class, (t_method)timed_gate_ms, gensym("ms"), A_FLOAT, 0);
    class_addmethod(timed_gate_class, (t_method)timed_gate_retrigger, gensym("retrigger"), A_FLOAT, 0);
    class_addfloat(timed_gate_class, (t_method)timed_gate_float);
    class_addbang(timed_gate_class, (t_method)timed_gate_bang);
}
