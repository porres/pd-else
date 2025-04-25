// Porres 2025

#include <m_pd.h>
#include <math.h>

static t_class *gatedelay_class;

typedef struct _gatedelay{
    t_object  x_obj;
    t_inlet  *x_del_let;
    t_float   x_sr_khz;
    t_float   x_f;
    int       x_nchans;
    int       x_n;
    int      *x_on;
    int      *x_count;
    t_float  *x_value;
    t_float  *x_lastin;
} t_gatedelay;

static t_int *gatedelay_perform(t_int *w){
    t_gatedelay *x = (t_gatedelay *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    int ch2 = (int)(w[5]);
    int n = x->x_n, chs = x->x_nchans;
    int *count = x->x_count;
    t_float sr_khz = x->x_sr_khz;
    t_float *value = x->x_value;
    t_float *lastin = x->x_lastin;
    int *on = x->x_on;
    for(int j = 0; j < chs; j++){
        for(int i = 0; i < n; i++){
            t_float gate = in1[j*n + i];
            t_float ms = ch2 == 1 ? in2[i] : in2[j*n + i];
            t_int samps = (int)roundf(ms * sr_khz);
            t_float output = 0;
            if(lastin[j] == 0 && gate != 0){
                on[j] = 1;
                count[j] = 0;
                value[j] = gate;
            }
            else if(lastin[j] != 0 && gate == 0){
                value[j] = 0.;
                output = count[j] = on[j] = 0;
            }
            if(on[j]){
                if(count[j] >= samps)
                    output = value[j];
                count[j]++;
            }
            out[j*n + i] = output;
            lastin[j] = gate;
        }
    }
    x->x_count = count;
    x->x_value = value;
    x->x_lastin = lastin;
    x->x_on = on;
    return(w+6);
}

static void gatedelay_dsp(t_gatedelay *x, t_signal **sp){
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
        x->x_lastin = (t_float *)resizebytes(x->x_lastin,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchans = chs;
    }
    int ch2 = sp[1]->s_nchans;
    if(ch2 > 1 && ch2 != chs){
        dsp_add_zero(sp[2]->s_vec, chs*x->x_n);
        pd_error(x, "[gatedelay~]: channel sizes mismatch");
        return;
    }
    dsp_add(gatedelay_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, ch2);
}

static void * gatedelay_free(t_gatedelay *x){
    inlet_free(x->x_del_let);
    freebytes(x->x_on, x->x_nchans * sizeof(*x->x_on));
    freebytes(x->x_value, x->x_nchans * sizeof(*x->x_on));
    freebytes(x->x_count, x->x_nchans * sizeof(*x->x_on));
    freebytes(x->x_lastin, x->x_nchans * sizeof(*x->x_lastin));
    return(void *)x;
}

static void *gatedelay_new(t_floatarg f1){
    t_gatedelay *x = (t_gatedelay *)pd_new(gatedelay_class);
    x->x_on = (int *)getbytes(sizeof(*x->x_on));
    x->x_count = (int *)getbytes(sizeof(*x->x_count));
    x->x_value = (t_float *)getbytes(sizeof(*x->x_value));
    x->x_lastin = (t_float *)getbytes(sizeof(*x->x_lastin));
    x->x_count[0] = x->x_on[0] = 0;
    x->x_value[0] = x->x_lastin[0] = 0.;
    x->x_sr_khz = sys_getsr() * 0.001;
    x->x_del_let = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_del_let, f1);
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void gatedelay_tilde_setup(void){
    gatedelay_class = class_new(gensym("gatedelay~"), (t_newmethod)gatedelay_new,
     (t_method)(gatedelay_free), sizeof(t_gatedelay), CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(gatedelay_class, t_gatedelay, x_f);
    class_addmethod(gatedelay_class, (t_method) gatedelay_dsp, gensym("dsp"), A_CANT, 0);
}
