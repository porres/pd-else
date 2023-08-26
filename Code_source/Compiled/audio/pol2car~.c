// porres

#include "m_pd.h"
#include <math.h>

static t_class *pol2car_tilde_class;

typedef struct _pol2car_tilde{
    t_object x_obj;
}t_pol2car_tilde;

t_int *pol2car_tilde_perform(t_int *w){
    t_sample *in1 = (t_sample *)(w[1]);
    t_sample *in2 = (t_sample *)(w[2]);
    t_sample *out1 = (t_sample *)(w[3]);
    t_sample *out2 = (t_sample *)(w[4]);
    int n = (int)(w[5]);
    while(n--){
        t_sample f1 = *in1++, f2 = *in2++;
        *out1++ = f1 * cosf(f2);
        *out2++ = f1 * sinf(f2);
    }
    return(w+6);
}

static void pol2car_tilde_dsp(t_pol2car_tilde *x, t_signal **sp){
    x = NULL;
    int n1 = sp[0]->s_length * sp[0]->s_nchans;
    int n2 = sp[1]->s_length * sp[1]->s_nchans;
    int outchans = sp[0]->s_nchans;
    signal_setmultiout(&sp[2], outchans);
    signal_setmultiout(&sp[3], outchans);
    if(sp[0]->s_nchans != sp[1]->s_nchans){
        pd_error(x, "[pol2car~]: number of channels mismatch");
        return;
    }
    t_sample *in1 = sp[0]->s_vec, *in2 = sp[1]->s_vec;
    t_sample *out1 = sp[2]->s_vec, *out2 = sp[3]->s_vec;
    for(int i = (n1+n2-1)/n1; i--; ){
        t_int blocksize = (n1 < n2 - i*n1 ? n1 : n2 - i*n1);
        dsp_add(pol2car_tilde_perform, 5, in1, in2 + i*n1,
                out1 + i*n1, out2 + i*n1, blocksize);
    }
}

static void *pol2car_tilde_new(void){
    t_pol2car_tilde *x = (t_pol2car_tilde *)pd_new(pol2car_tilde_class);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void pol2car_tilde_setup(void){
    pol2car_tilde_class = class_new(gensym("pol2car~"), (t_newmethod)pol2car_tilde_new, 0,
        sizeof(t_pol2car_tilde), CLASS_MULTICHANNEL, 0);
    class_addmethod(pol2car_tilde_class, nullfn, gensym("signal"), 0);
    class_addmethod(pol2car_tilde_class, (t_method)pol2car_tilde_dsp, gensym("dsp"), A_CANT, 0);
}
