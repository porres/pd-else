// Porres 2017-2025

#include "m_pd.h"
#include <stdlib.h>

static t_class *gate2imp_class;

typedef struct _gate2imp{
    t_object    x_obj;
    t_float    *x_lastin;
    t_float     x_in;
    int         x_nchs;
    t_int       x_n;
} t_gate2imp;


static t_int * gate2imp_perform(t_int *w){
    t_gate2imp *x = (t_gate2imp *)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    int n = x->x_n;
    t_float *lastin = x->x_lastin;
    for(int j = 0; j < x->x_nchs; j++){
        for(int i = 0; i < n; i++){
            float input = in[j*n+i];
            if(lastin[j] == 0 && input != 0)
                out[j*n+i] = input;
            else
                out[j*n+i] = 0;
            lastin[j] = input;
        }
    }
    x->x_lastin = lastin;
    return(w+4);
}

static void gate2imp_dsp(t_gate2imp *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    if(x->x_nchs != chs){
        x->x_lastin = (t_float *)resizebytes(x->x_lastin,
            x->x_nchs * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchs = chs;
    }
    signal_setmultiout(&sp[1], x->x_nchs);
    dsp_add(gate2imp_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
}

static void gate2imp_free(t_gate2imp *x){
    freebytes(x->x_lastin, x->x_nchs * sizeof(*x->x_lastin));
}

void *gate2imp_new(void){
    t_gate2imp *x = (t_gate2imp *)pd_new(gate2imp_class);
    x->x_lastin = (t_float*)malloc(sizeof(t_float));
    x->x_lastin[0] = 0;
    outlet_new(&x->x_obj, &s_signal);
    return(void *)x;
}

void gate2imp_tilde_setup(void){
  gate2imp_class = class_new(gensym("gate2imp~"), (t_newmethod)(void*)gate2imp_new, (t_method)gate2imp_free, sizeof (t_gate2imp), CLASS_MULTICHANNEL, 0);
  CLASS_MAINSIGNALIN(gate2imp_class, t_gate2imp, x_in);
  class_addmethod(gate2imp_class, (t_method) gate2imp_dsp, gensym("dsp"), A_CANT, 0);
}
