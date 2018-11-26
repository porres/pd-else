#include "m_pd.h"

static t_class *binshift_tilde_class;

typedef struct _binshift_tilde{
    t_object x_obj;
    int x_shift;
}t_binshift_tilde;

static void binshift_float(t_binshift_tilde *x, t_float f){
    x->x_shift = (int)f;
}

static t_int *binshift_perform(t_int *w){
    t_sample *in = (t_sample *)(w[1]);
    t_sample *out= (t_sample *)(w[2]);
    int n = (int)(w[3]);
    t_binshift_tilde *x = (t_binshift_tilde *)(w[4]);
    int shift = x->x_shift;
    if(shift > n)
        shift = n;
    if(shift < -n)
        shift = -n;    
    if(shift < 0)
        shift += n;
    for(int i = 0; i < n; i++){
        int j = (i + shift) % n;
        out[i] = in[j];
    }
    return(w + 5);
}

static void binshift_tilde_dsp(t_binshift_tilde *x, t_signal **sp){
    dsp_add(binshift_perform, 4, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n, x);
}

static void *binshift_tilde_new(t_floatarg f){
    t_binshift_tilde *x = (t_binshift_tilde *)pd_new(binshift_tilde_class);
    x->x_shift = f;
    outlet_new(&x->x_obj, gensym("signal"));
    return(x);
}

void setup_bin0x2eshift_tilde(void){
    binshift_tilde_class = class_new(gensym("bin.shift~"),
        (t_newmethod)binshift_tilde_new, 0, sizeof(t_binshift_tilde), 0, A_DEFFLOAT, 0);
    class_addmethod(binshift_tilde_class, nullfn, gensym("signal"), 0);
    class_addmethod(binshift_tilde_class, (t_method)binshift_tilde_dsp, gensym("dsp"), A_CANT, 0);
    class_addfloat(binshift_tilde_class, (t_method)binshift_float);
}