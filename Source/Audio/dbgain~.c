// porres 2025

#include <m_pd.h>
#include "math.h"

static t_class *dbgain_tilde_class;

typedef struct _dbgain_tilde{
    t_object    x_obj;
    t_inlet    *x_db_inlet;
    t_int       x_ch2;
    t_int         x_nchans;
    t_int       x_n;
}t_dbgain_tilde;

static t_int *dbgain_tilde_perform(t_int *w){
    t_dbgain_tilde *x = (t_dbgain_tilde *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0, n = x->x_n; i < n; i++){
            float db = x->x_ch2 == 1 ? in2[i] : in2[j*n + i];
            if(db <= -100)
                out[j*n + i] = 0;
            else
                out[j*n + i] = in1[j*n + i] * pow(10, db * 0.05);
        }
    }
    return(w+5);
}

static void dbgain_tilde_dsp(t_dbgain_tilde *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    x->x_nchans = sp[0]->s_nchans;
    x->x_ch2 = sp[1]->s_nchans;
    signal_setmultiout(&sp[2], x->x_nchans);
    if(x->x_ch2 > 1 && x->x_ch2 != x->x_nchans){
        dsp_add_zero(sp[2]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[dbgain~]: channel sizes mismatch");
        return;
    }
    dsp_add(dbgain_tilde_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

void * dbgain_tilde_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_dbgain_tilde *x = (t_dbgain_tilde *) pd_new(dbgain_tilde_class);
    float db = 0;
    if(ac){
        if(av->a_type == A_FLOAT)
            db = atom_getfloat(av);
    }
    x->x_db_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_db_inlet, db);
    outlet_new((t_object *)x, &s_signal);
    return(void *)x;
}

void dbgain_tilde_setup(void) {
    dbgain_tilde_class = class_new(gensym("dbgain~"), (t_newmethod)dbgain_tilde_new,
        0, sizeof(t_dbgain_tilde), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(dbgain_tilde_class, nullfn, gensym("signal"), 0);
    class_addmethod(dbgain_tilde_class, (t_method)dbgain_tilde_dsp, gensym("dsp"), A_CANT, 0);
}
