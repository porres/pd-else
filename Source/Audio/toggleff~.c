// Porres 2016-2026

#include <m_pd.h>
#include "math.h"

static t_class *toggleff_class;

typedef struct _toggleff{
    t_object    x_obj;
    t_int       x_n;
    t_int       x_nchans;
    t_float    *x_xm1;
    t_float    *x_ym1;
    t_float     x_init;
}t_toggleff;

static t_int *toggleff_perform(t_int *w){
    t_toggleff *x = (t_toggleff *)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    t_float *xm1 = x->x_xm1;
    t_float *ym1 = x->x_ym1;
    int n = x->x_n;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            float x0 = in[j*n + i];
            if(x->x_init){
                out[j*n + i] = ym1[j] = 1;
                x->x_init = 0;
            }
            else{
                t_int cond = x0 > 0 && xm1[j] <= 0;
                if(x0 > 0 && xm1[j] <= 0)
                    ym1[j] = fmod(ym1[j] + 1, 2);
                out[j*n + i] = ym1[j];
            }
            xm1[j] = x0;
        }
    }
    x->x_xm1 = xm1;
    x->x_ym1 = ym1;
    return(w+4);
}

static void toggleff_dsp(t_toggleff *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    if(x->x_nchans != chs){
       x->x_xm1 = (t_float *)resizebytes(x->x_xm1,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_ym1 = (t_float *)resizebytes(x->x_ym1,
             x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(toggleff_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
}

static void *toggleff_new(t_floatarg f){
    t_toggleff *x = (t_toggleff *)pd_new(toggleff_class);
    x->x_init = (int)(f != 0);
    x->x_nchans = 1;
    x->x_xm1 = (t_float *)getbytes(sizeof(*x->x_xm1));
    x->x_xm1[0] = 0;
    x->x_ym1 = (t_float *)getbytes(sizeof(*x->x_ym1));
    x->x_ym1[0] = 0;
    outlet_new((t_object *)x, &s_signal);
    return(x);
}

void toggleff_tilde_setup(void){
    toggleff_class = class_new(gensym("toggleff~"), (t_newmethod)toggleff_new,
        0, sizeof(t_toggleff), CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
    class_addmethod(toggleff_class, nullfn, gensym("signal"), 0);
    class_addmethod(toggleff_class, (t_method)toggleff_dsp, gensym("dsp"), A_CANT, 0);
}
