// porres

#include "m_pd.h"
#include <stdlib.h>

static t_class *slice_class;

typedef struct _slice{
    t_object x_obj;
    t_int    x_n;
}t_slice;

static void slice_float(t_slice *x, t_floatarg f){
    x->x_n = (int)f;
    canvas_update_dsp();
}

static void slice_dsp(t_slice *x, t_signal **sp){
    int chs = sp[0]->s_nchans, n = sp[0]->s_n;
    int chs1 = x->x_n;
    if(chs1 > 0){
        if(chs1 > chs)
            chs1 = chs;
    }
    else{
        chs1+=chs;
        if(chs1 <= 0)
            chs1 = 0;
    }
    int chs2 = chs - chs1;
    signal_setmultiout(&sp[1], chs1 <= 0 ? 1 : chs1);
    signal_setmultiout(&sp[2], chs2 <= 0 ? 1 : chs2);
    if(chs1 <= 0)
        dsp_add_zero(sp[1]->s_vec, n);
    else
        dsp_add_copy(sp[0]->s_vec, sp[1]->s_vec, chs1*n);
    if(chs2 <= 0)
        dsp_add_zero(sp[2]->s_vec, n);
    else
        dsp_add_copy(sp[0]->s_vec + chs1*n, sp[2]->s_vec, chs2*n);
}

static void *slice_new(t_floatarg f){
    t_slice *x = (t_slice *)pd_new(slice_class);
    x->x_n = (int)f;
    outlet_new(&x->x_obj, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void slice_tilde_setup(void){
    slice_class = class_new(gensym("slice~"), (t_newmethod)slice_new,
        0, sizeof(t_slice), CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
    class_addfloat(slice_class, slice_float);
    class_addmethod(slice_class, nullfn, gensym("signal"), 0);
    class_addmethod(slice_class, (t_method)slice_dsp, gensym("dsp"), 0);
}
