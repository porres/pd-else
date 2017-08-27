// Porres 2017

#include "m_pd.h"

static t_class *tdelay2_class;

typedef struct _tdelay2{
    t_object  x_obj;
    t_int     x_on;
    t_inlet  *x_del_let;
    t_float   x_count;
    t_float   x_sr_khz;
} t_tdelay2;

static t_int *tdelay2_perform(t_int *w){
    t_tdelay2 *x = (t_tdelay2 *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_float *del_in = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    t_float count = x->x_count;
    t_float sr_khz = x->x_sr_khz;
    while (nblock--){
        t_float input = *in++;
        t_float del_ms = *del_in++;
        t_int samps = (int)roundf(del_ms * sr_khz);
        if(count > samps)
            x->x_on = 0;
        if (input == 1 && !x->x_on){
            x->x_on = 1;
            count = 0;
            }
        if(x->x_on){
            *out++ = count == samps;
            count += 1;
            }
        else
            *out++ = 0;
        }
    x->x_count = count;
    return (w + 6);
}

static void tdelay2_dsp(t_tdelay2 *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    dsp_add(tdelay2_perform, 5, x, sp[0]->s_n,
        sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *tdelay2_new(t_floatarg f1){
    t_tdelay2 *x = (t_tdelay2 *)pd_new(tdelay2_class);
    x->x_count = x->x_on = 0;
    x->x_sr_khz = sys_getsr() * 0.001;
    x->x_del_let = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_del_let, f1);
    outlet_new(&x->x_obj, &s_signal);
    return (x);
}

static void * tdelay2_free(t_tdelay2 *x){
    inlet_free(x->x_del_let);
    return (void *)x;
}

void tdelay2_tilde_setup(void){
    tdelay2_class = class_new(gensym("tdelay2~"), (t_newmethod)tdelay2_new,
        0, sizeof(t_tdelay2), CLASS_DEFAULT, A_DEFFLOAT, 0);
    class_addmethod(tdelay2_class, nullfn, gensym("signal"), 0);
    class_addmethod(tdelay2_class, (t_method) tdelay2_dsp, gensym("dsp"), 0);
}
