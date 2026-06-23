// Porres 2017-2026

#include <m_pd.h>

static t_class *trighold_class;

typedef struct _trighold{
    t_object  x_obj;
    t_float  *x_lastin;
    t_float  *x_trig;
    t_float   x_in;
    t_int     x_n;
    t_int     x_nchans;
}t_trighold;

static void trighold_clear(t_trighold *x){
    for(int j = 0; j < x->x_nchans; j++)
        x->x_trig[j] = 0;
}

static t_int * trighold_perform(t_int *w){
    t_trighold *x = (t_trighold *)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    t_float *lastin = x->x_lastin;
    t_float *trig = x->x_trig;
    int n = x->x_n;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            float input = in[j*n + i];
            if(lastin[j] == 0 && input != 0)
                trig[j] = input;
            out[j*n + i] = trig[j];
            lastin[j] = input;
        }
    }
    x->x_lastin = lastin;
    x->x_trig = trig;
    return(w+4);
}

static void trighold_dsp(t_trighold *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    if(x->x_nchans != chs){
       x->x_lastin = (t_float *)resizebytes(x->x_lastin,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_trig = (t_float *)resizebytes(x->x_trig,
             x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        for(int i = 0; i < chs; i++){
            x->x_lastin[i] = 0;
            x->x_trig[i] = 0;
        }
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(trighold_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
}

static void *trighold_free(t_trighold *x){
    freebytes(x->x_lastin, x->x_nchans * sizeof(*x->x_lastin));
    freebytes(x->x_trig, x->x_nchans * sizeof(*x->x_trig));
    return(void *)x;
}

void *trighold_new(void){
    t_trighold *x = (t_trighold *)pd_new(trighold_class);
    x->x_lastin = (t_float *)getbytes(sizeof(*x->x_lastin));
    x->x_lastin[0] = 0;
    x->x_trig = (t_float *)getbytes(sizeof(*x->x_trig));
    x->x_trig[0] = 0;
    outlet_new(&x->x_obj, &s_signal);
    return(void *)x;
}

void trighold_tilde_setup(void){
    trighold_class = class_new(gensym("trighold~"),
        (t_newmethod) trighold_new, 0, sizeof (t_trighold), CLASS_MULTICHANNEL, 0);
    CLASS_MAINSIGNALIN(trighold_class, t_trighold, x_in);
    class_addmethod(trighold_class, (t_method) trighold_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(trighold_class, (t_method)trighold_clear, gensym("clear"), 0);
}
