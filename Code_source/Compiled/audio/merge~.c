// porres

#include "m_pd.h"
#include <stdlib.h>

static t_class *merge_class;

typedef struct _merge{
    t_object x_obj;
    t_int   *x_ch;
    t_int   *x_totch;
    t_int    x_n;
}t_merge;

static void merge_dsp(t_merge *x, t_signal **sp){
    int i, totchs = 0, n = sp[0]->s_n;
    for(i = 0; i < x->x_n; i++)
        x->x_totch[i+1] = (totchs += (x->x_ch[i] = sp[i]->s_nchans));
    signal_setmultiout(&sp[x->x_n], totchs);
    for(i = 0; i < x->x_n; i++)
        dsp_add_copy(sp[i]->s_vec, sp[x->x_n]->s_vec + x->x_totch[i]*n, x->x_ch[i]*n);
}

void merge_free(t_merge *x){
    freebytes(x->x_ch, x->x_n * sizeof(*x->x_ch));
    freebytes(x->x_totch, (x->x_n+1) * sizeof(*x->x_ch));
}

static void *merge_new(t_floatarg f){
    t_merge *x = (t_merge *)pd_new(merge_class);
    x->x_n = f < 2 ? 2 : (int)f;
    x->x_ch = (t_int *)getbytes(x->x_n * sizeof(*x->x_ch));
    x->x_totch = (t_int *)getbytes((x->x_n+1) * sizeof(*x->x_ch));
    x->x_totch[0] = 0;
    for(int i = 1; i < x->x_n; i++)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void merge_tilde_setup(void){
    merge_class = class_new(gensym("merge~"), (t_newmethod)merge_new,
        (t_method)merge_free, sizeof(t_merge), CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
    class_addmethod(merge_class, nullfn, gensym("signal"), 0);
    class_addmethod(merge_class, (t_method)merge_dsp, gensym("dsp"), 0);
}
