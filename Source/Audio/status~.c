// Porres 2017-2026

#include <m_pd.h>

static t_class *status_class;

typedef struct _status{
    t_object x_obj;
    int      x_nchans;
    int      x_n;
    t_float *x_lastin;
    t_float  x_in;
    t_float  x_arg;
} t_status;

static t_int * status_perform(t_int *w){
    t_status *x = (t_status *)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out1 = (t_float *)(w[3]);
    t_float *out2 = (t_float *)(w[4]);
    t_float *lastin = x->x_lastin;
    int n = x->x_n;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            float input = in[j*n+i];
            out1[j*n+i] = lastin[j] == 0 && input != 0;
            out2[j*n+i] = lastin[j] != 0 && input == 0;
            lastin[j] = input;
        }
    }
    x->x_lastin = lastin;
    return(w+5);
}

static void status_dsp(t_status *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    if(x->x_nchans != chs){
       x->x_lastin = (t_float *)resizebytes(x->x_lastin,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchans = chs;
        for(int i = 0; i < x->x_nchans; i++)
            x->x_lastin[i] = x->x_arg;
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    signal_setmultiout(&sp[2], x->x_nchans);
    dsp_add(status_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *status_free(t_status *x){
    freebytes(x->x_lastin, x->x_nchans * sizeof(*x->x_lastin));
    return(void *)x;
}

void *status_new(t_floatarg f){
    t_status *x = (t_status *)pd_new(status_class);
    x->x_lastin = (t_float *)getbytes(sizeof(*x->x_lastin));
    x->x_lastin[0] = x->x_arg = f;
    outlet_new(&x->x_obj, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    return(void *)x;
}

void status_tilde_setup(void){
    status_class = class_new(gensym("status~"), (t_newmethod) status_new,
        (t_method)status_free, sizeof (t_status), CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(status_class, t_status, x_in);
    class_addmethod(status_class, (t_method) status_dsp, gensym("dsp"), A_CANT, 0);
}
