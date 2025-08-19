// Porres 2017-2025

#include <m_pd.h>

static t_class *timed_gate_class;

typedef struct _timed_gate{
    t_object  x_obj;
    t_float   x_sr_khz;
    int       x_nchans;
    int       x_n;
    int       x_bang;
    int       x_retrigger;
    int      *x_on;
    int      *x_count;
    t_float  *x_value;
    t_float  *x_last_in;
    t_inlet  *x_del_let;
    t_outlet *x_ignore_out;
}t_timed_gate;

static void timed_gate_ms(t_timed_gate *x, t_float f){
    if(f >= 0)
        pd_float((t_pd *)x->x_del_let, f);
}

static void timed_gate_bang(t_timed_gate *x){
    for(int i = 0; i < x->x_nchans; i++){
        if(x->x_value[i] == 0)
            x->x_value[i] = 1;
    }
    x->x_bang = 1;
}

static void timed_gate_float(t_timed_gate *x, t_float f){
    if(f == 0)
        return;
    for(int i = 0; i < x->x_nchans; i++)
        x->x_value[i] = f;
    x->x_bang = 1;
}

static t_int *timed_gate_perform(t_int *w){
    t_timed_gate *x = (t_timed_gate *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *out1 = (t_float *)(w[4]);
    t_float *out2 = (t_float *)(w[5]);
    int ch2 = (int)(w[6]);
    int n = x->x_n, chs = x->x_nchans;
    int *count = x->x_count;
    t_float sr_khz = x->x_sr_khz;
    t_float *value = x->x_value;
    t_float *last_in = x->x_last_in;
    int *on = x->x_on;
    for(int j = 0; j < chs; j++){
        for(int i = 0; i < n; i++){
            t_float in = in1[j*n + i];
            t_float ms = ch2 == 1 ? in2[i] : in2[j*n + i];
            t_int samps = (int)((ms * sr_khz)+0.5f) - 1;
            t_float output1 = 0;
            t_float output2 = 0;
            int flag = x->x_retrigger ? 1 : !on[j];
            if((in != 0 && last_in[j] == 0) || x->x_bang){
                if(flag){
                    if(samps >= 0){
                        on[j] = 1;
                        count[j] = 0;
                        if(!x->x_bang)
                            value[j] = in;
                    }
                    output1 = value[j];
                }
                else
                    output2 = value[j];
            }
            else if(on[j]){
                if(count[j] < samps)
                    output1 = value[j];
                else
                    on[j] = 0;
                count[j]++;
            }
            out1[j*n + i] = output1;
            out2[j*n + i] = output2;
            last_in[j] = in;
        }
    }
    x->x_count = count;
    x->x_value = value;
    x->x_last_in = last_in;
    x->x_on = on;
    x->x_bang = 0;
    return(w+7);
}

static void timed_gate_retrigger(t_timed_gate *x, t_floatarg f){
    x->x_retrigger = (int)(f > 0);
}

static void timed_gate_dsp(t_timed_gate *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    signal_setmultiout(&sp[2], chs);
    signal_setmultiout(&sp[3], chs);
    if(x->x_nchans != chs){
        x->x_on = (int *)resizebytes(x->x_on,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_count = (int *)resizebytes(x->x_count,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_value = (t_float *)resizebytes(x->x_value,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_last_in = (t_float *)resizebytes(x->x_last_in,
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

static void * timed_gate_free(t_timed_gate *x){
    inlet_free(x->x_del_let);
    freebytes(x->x_on, x->x_nchans * sizeof(*x->x_on));
    freebytes(x->x_value, x->x_nchans * sizeof(*x->x_value));
    freebytes(x->x_last_in, x->x_nchans * sizeof(*x->x_last_in));
    freebytes(x->x_count, x->x_nchans * sizeof(*x->x_count));
    outlet_free(x->x_ignore_out);
    return(void *)x;
}

static void *timed_gate_new(t_floatarg f1, t_floatarg f2){
    t_timed_gate *x = (t_timed_gate *)pd_new(timed_gate_class);
    x->x_on = (int *)getbytes(sizeof(*x->x_on));
    x->x_count = (int *)getbytes(sizeof(*x->x_count));
    x->x_value = (t_float *)getbytes(sizeof(*x->x_value));
    x->x_last_in = (t_float *)getbytes(sizeof(*x->x_last_in));
    x->x_count[0] = x->x_on[0] = 0;
    x->x_value[0] = x->x_last_in[0] = 0.;
    x->x_sr_khz = sys_getsr() * 0.001;
    x->x_del_let = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_del_let, f1);
    x->x_retrigger = (int)(f2 != 0);
    outlet_new(&x->x_obj, &s_signal);
    x->x_ignore_out = outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void setup_timed0x2egate_tilde(void){
    timed_gate_class = class_new(gensym("timed.gate~"), (t_newmethod)timed_gate_new,
     (t_method)(timed_gate_free), sizeof(t_timed_gate), CLASS_MULTICHANNEL,
        A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(timed_gate_class, nullfn, gensym("signal"), 0);
    class_addmethod(timed_gate_class, (t_method) timed_gate_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(timed_gate_class, (t_method)timed_gate_ms, gensym("ms"), A_FLOAT, 0);
    class_addmethod(timed_gate_class, (t_method)timed_gate_retrigger,
        gensym("retrigger"), A_FLOAT, 0);
    class_addfloat(timed_gate_class, (t_method)timed_gate_float);
    class_addbang(timed_gate_class, (t_method)timed_gate_bang);
}
