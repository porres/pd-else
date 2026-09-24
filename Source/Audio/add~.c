// Porres 2016

#include <m_pd.h>

static t_class *add_class;

typedef struct _add{
    t_object  x_obj;
    t_float  *x_sum;
    t_float  *x_start;
    t_float   x_in;
    t_inlet  *x_triglet;
    t_outlet *x_outlet;
    int       x_nchans;
    int       x_trigchans;
} t_add;

static void add_bang(t_add *x){
    for(int j = 0; j < x->x_nchans; j++)
        x->x_sum[j] = x->x_start[j];
}

static void add_set(t_add *x, t_floatarg f){
    for(int j = 0; j < x->x_nchans; j++)
        x->x_start[j] = f;
}

static t_int *add_perform(t_int *w){
    t_add *x = (t_add *)(w[1]);
    int n = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    for(int j = 0; j < x->x_nchans; j++){
        t_float sum = x->x_sum[j];
        t_float start = x->x_start[j];
        t_float *trig = x->x_trigchans == 1 ? in2 : in2 + j*n;
        for(int i = 0; i < n; i++){
            t_float in = in1[j*n+i];
            if(trig[i] == 1)
                out[j*n+i] = sum = (start += in);
            else
                out[j*n+i] = (sum += in);
        }
        x->x_sum[j] = sum;
        x->x_start[j] = start;
    }
    return(w+6);
}

static void add_dsp(t_add *x, t_signal **sp){
    int chs = sp[0]->s_nchans;
    x->x_trigchans = sp[1]->s_nchans;
    if(x->x_nchans != chs){
        x->x_sum = (t_float *)resizebytes(x->x_sum,
            x->x_nchans * sizeof(*x->x_sum), chs * sizeof(*x->x_sum));
        x->x_start = (t_float *)resizebytes(x->x_start,
            x->x_nchans * sizeof(*x->x_start), chs * sizeof(*x->x_start));
        for(int j = x->x_nchans; j < chs; j++)
            x->x_sum[j] = x->x_start[j] = 0;
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[2], x->x_nchans);
    if(x->x_trigchans > 1 && x->x_trigchans != x->x_nchans){
        dsp_add_zero(sp[2]->s_vec, x->x_nchans * sp[2]->s_n);
        pd_error(x, "[add~]: channel sizes mismatch");
        return;
    }
    dsp_add(add_perform, 5, x, sp[0]->s_n, sp[0]->s_vec,
        sp[1]->s_vec, sp[2]->s_vec);
}

static void *add_free(t_add *x){
    freebytes(x->x_sum, x->x_nchans * sizeof(*x->x_sum));
    freebytes(x->x_start, x->x_nchans * sizeof(*x->x_start));
    inlet_free(x->x_triglet);
    outlet_free(x->x_outlet);
    return(void *)x;
}

static void *add_new(t_floatarg f){
    t_add *x = (t_add *)pd_new(add_class);
    x->x_nchans = 1;
    x->x_trigchans = 1;
    x->x_sum = (t_float *)getbytes(sizeof(*x->x_sum));
    x->x_start = (t_float *)getbytes(sizeof(*x->x_start));
    x->x_sum[0] = x->x_start[0] = f;
    x->x_triglet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void add_tilde_setup(void){
    add_class = class_new(gensym("add~"), (t_newmethod)add_new,
        (t_method)add_free, sizeof(t_add), CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(add_class, t_add, x_in);
    class_addmethod(add_class, (t_method)add_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(add_class, (t_method)add_set, gensym("set"), A_FLOAT, 0);
    class_addbang(add_class, (t_method)add_bang);
}
