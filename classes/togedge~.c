// Porres 2017

#include "m_pd.h"

static t_class *togedge_class;

typedef struct _togedge{
    t_object x_obj;
    t_float  x_lastin;
    t_float  x_in;
} t_togedge;


static t_int * togedge_perform(t_int *w){
    t_togedge *x = (t_togedge *)(w[1]);
    int n = (int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_float *out1 = (t_float *)(w[4]);
    t_float *out2 = (t_float *)(w[5]);
    t_float lastin = x->x_lastin;
    while(n--){
        float input = *in++;
        *out1++ = lastin == 0 && input != 0;
        *out2++ = lastin != 0 && input == 0;
        lastin = input;
    }
    x->x_lastin = lastin;
    return (w + 6);
}

static void togedge_dsp(t_togedge *x, t_signal **sp){
  dsp_add(togedge_perform, 5, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

void *togedge_new(void){
  t_togedge *x = (t_togedge *)pd_new(togedge_class);
  x->x_lastin = 0;
  outlet_new(&x->x_obj, &s_signal);
  outlet_new((t_object *)x, &s_signal);
  return (void *)x;
}

void togedge_tilde_setup(void){
  togedge_class = class_new(gensym("togedge~"),
    (t_newmethod) togedge_new, 0, sizeof (t_togedge), CLASS_DEFAULT, 0);
  CLASS_MAINSIGNALIN(togedge_class, t_togedge, x_in);
  class_addmethod(togedge_class, (t_method) togedge_dsp, gensym("dsp"), 0);
}
