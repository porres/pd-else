// porres 2018-2023

#include "m_pd.h"
#include "math.h"

static t_class *lin2db_tilde_class;

typedef struct _lin2db_tilde{
    t_object x_obj;
    t_float  x_min;
}t_lin2db_tilde;

static t_int *lin2db_tilde_perform(t_int *w){
    t_int n = (t_int)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    for(t_int i = 0; i < n; i++)
        out[i] = 20 * log10(in[i]);
    return(w+4);
}

static void lin2db_tilde_dsp(t_lin2db_tilde *x, t_signal **sp){
    x = NULL;
    signal_setmultiout(&sp[1], sp[0]->s_nchans);
    dsp_add(lin2db_tilde_perform, 3, (t_int)(sp[0]->s_length * sp[0]->s_nchans),
        sp[0]->s_vec, sp[1]->s_vec);
}

void * lin2db_tilde_new(void){
    t_lin2db_tilde *x = (t_lin2db_tilde *) pd_new(lin2db_tilde_class);
    outlet_new((t_object *)x, &s_signal);
    return(void *)x;
}

void lin2db_tilde_setup(void) {
    lin2db_tilde_class = class_new(gensym("lin2db~"), (t_newmethod)lin2db_tilde_new,
        0, sizeof(t_lin2db_tilde), CLASS_MULTICHANNEL, 0);
    class_addmethod(lin2db_tilde_class, nullfn, gensym("signal"), 0);
    class_addmethod(lin2db_tilde_class, (t_method)lin2db_tilde_dsp, gensym("dsp"), A_CANT, 0);
}
