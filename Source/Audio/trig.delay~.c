// Porres 2017

#include <m_pd.h>

static t_class *trig_delay_class;

typedef struct _trig_delay{
    t_object  x_obj;
    t_inlet  *x_del_let;
    t_float   x_sr_khz;
    int       x_nchans;
    int       x_n;
    int       x_ignore;
    int      *x_on;
    int      *x_count;
    t_float  *x_value;
    t_float  *x_last_in;
}t_trig_delay;

static t_int *trig_delay_perform(t_int *w){
    t_trig_delay *x = (t_trig_delay *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    int ch2 = (int)(w[5]);
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
            t_float output = 0;
            int flag = x->x_ignore ? !on[j] : 1;
            if(in != 0 && last_in[j] == 0 && flag){
                if(samps >= 0){
                    on[j] = 1;
                    count[j] = 0;
                    value[j] = in;
                }
                else
                    output = in;
            }
            else if(on[j]){
                if(count[j] == samps){
                    output = value[j];
                    on[j] = 0;
                }
                count[j]++;
            }
            out[j*n + i] = output;
            last_in[j] = in;
        }
    }
    x->x_count = count;
    x->x_value = value;
    x->x_last_in = last_in;
    x->x_on = on;
    return(w+6);
}

static void trig_delay_ignore(t_trig_delay *x, t_floatarg f){
    x->x_ignore = (int)(f > 0);
}

static void trig_delay_dsp(t_trig_delay *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    signal_setmultiout(&sp[2], chs);
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
        pd_error(x, "[trig.delay~]: channel sizes mismatch");
        return;
    }
    dsp_add(trig_delay_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, ch2);
}

static void * trig_delay_free(t_trig_delay *x){
    inlet_free(x->x_del_let);
    freebytes(x->x_on, x->x_nchans * sizeof(*x->x_on));
    freebytes(x->x_value, x->x_nchans * sizeof(*x->x_value));
    freebytes(x->x_last_in, x->x_nchans * sizeof(*x->x_last_in));
    freebytes(x->x_count, x->x_nchans * sizeof(*x->x_count));
    return(void *)x;
}

static void *trig_delay_new(t_floatarg f1, t_floatarg f2){
    t_trig_delay *x = (t_trig_delay *)pd_new(trig_delay_class);
    x->x_on = (int *)getbytes(sizeof(*x->x_on));
    x->x_count = (int *)getbytes(sizeof(*x->x_count));
    x->x_value = (t_float *)getbytes(sizeof(*x->x_value));
    x->x_last_in = (t_float *)getbytes(sizeof(*x->x_last_in));
    x->x_count[0] = x->x_on[0] = 0;
    x->x_value[0] = x->x_last_in[0] = 0.;
    x->x_sr_khz = sys_getsr() * 0.001;
    x->x_del_let = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_del_let, f1);
    outlet_new(&x->x_obj, &s_signal);
    x->x_ignore = (int)(f2 > 0);
    return(x);
}

void setup_trig0x2edelay_tilde(void){
    trig_delay_class = class_new(gensym("trig.delay~"), (t_newmethod)trig_delay_new,
     (t_method)(trig_delay_free), sizeof(t_trig_delay), CLASS_MULTICHANNEL,
        A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(trig_delay_class, nullfn, gensym("signal"), 0);
    class_addmethod(trig_delay_class, (t_method) trig_delay_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(trig_delay_class, (t_method)trig_delay_ignore,
        gensym("ignore"), A_FLOAT, 0);
}
