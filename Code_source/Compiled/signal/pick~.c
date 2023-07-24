// porres

#include "m_pd.h"

static t_class *pick_class;

typedef struct _pick{
    t_object x_obj;
    int      x_ch;
}t_pick;

static void pick_float(t_pick *x, t_floatarg f){
    x->x_ch = f;
}

static t_int *pick_perform(t_int *w){
    t_pick *x = (t_pick *)(w[1]);
    t_int n = (t_int)(w[2]);
    t_int nchans = (t_int)(w[3]);
    t_sample *in = (t_sample *)(w[4]);
    t_sample *out = (t_sample *)(w[5]);
    int i, ch = x->x_ch;
    if(ch == 0 || ch > nchans){
        for(i = 0; i < n; i++)
            *out++ = 0;
    }
    else if(ch < 0){ // negative
        ch += nchans;
        if(ch < 0){
            for(i = 0; i < n; i++)
                *out++ = 0;
        }
        else
            for(i = 0; i < n; i++)
                *out++ = in[ch*n+i];
    }
    else{
        ch--;
        for(i = 0; i < n; i++)
            *out++ = in[ch*n+i];
    }
    return(w+6);
}
    
static void pick_dsp(t_pick *x, t_signal **sp){
    signal_setmultiout(&sp[1], 1);
    dsp_add(pick_perform, 5, x, sp[0]->s_n, sp[0]->s_nchans, sp[0]->s_vec, sp[1]->s_vec);
}

static void *pick_new(t_floatarg f){
    t_pick *x = (t_pick *)pd_new(pick_class);
    x->x_ch = f;
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void pick_tilde_setup(void){
    pick_class = class_new(gensym("pick~"), (t_newmethod)pick_new,
        0, sizeof(t_pick), CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
    class_addfloat(pick_class, pick_float);
    class_addmethod(pick_class, nullfn, gensym("signal"), 0);
    class_addmethod(pick_class, (t_method)pick_dsp, gensym("dsp"), 0);
}
