// porres

#include "m_pd.h"
#include "buffer.h"

typedef struct _fader_tilde{
    t_object    x_obj;
    t_int       x_table;
}t_fader_tilde;

static t_class *fader_tilde_class;

static void fader_quartic(t_fader_tilde *x){
    x->x_table = 0;
}

static void fader_lin(t_fader_tilde *x){
    x->x_table = 1;
}

static void fader_linsin(t_fader_tilde *x){
    x->x_table = 2;
}

static void fader_sqrt(t_fader_tilde *x){
    x->x_table = 3;
}

static void fader_sin(t_fader_tilde *x){
    x->x_table = 4;
}

static void fader_hannsin(t_fader_tilde *x){
    x->x_table = 5;
}

static void fader_hann(t_fader_tilde *x){
    x->x_table = 6;
}

static t_int *fader_tilde_perform(t_int *w){
    t_fader_tilde *x = (t_fader_tilde *)(w[1]);
    int n = (int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    while(n--){
        t_float phase = *in++;
        if(phase < 0.)
            phase = 0.;
        if(phase > 1.)
            phase = 1.;
        *out++ = read_fadetab(phase, x->x_table);
    }
    return(w+5);
}

static void fader_tilde_dsp(t_fader_tilde *x, t_signal **sp){
    signal_setmultiout(&sp[1], sp[0]->s_nchans);
    dsp_add(fader_tilde_perform, 4, x,
        (t_int)(sp[0]->s_n * sp[0]->s_nchans),
        sp[0]->s_vec, sp[1]->s_vec);
}

static void *fader_tilde_new(t_symbol *s){
    t_fader_tilde *x = (t_fader_tilde *)pd_new(fader_tilde_class);
    init_fade_tables();
    x->x_table = 0; // quartic
    if(s == gensym("quartic"))
        x->x_table = 0;
    else if(s == gensym("lin"))
        x->x_table = 1;
    else if(s == gensym("linsin"))
        x->x_table = 2;
    else if(s == gensym("sqrt"))
        x->x_table = 3;
    else if(s == gensym("sin"))
        x->x_table = 4;
    else if(s == gensym("hannsin"))
        x->x_table = 5;
    else if(s == gensym("hann"))
        x->x_table = 6;
    outlet_new(&x->x_obj, gensym("signal"));
    return(x);
}

void fader_tilde_setup(void){
  fader_tilde_class = class_new(gensym("fader~"), (t_newmethod)fader_tilde_new, 0,
    sizeof(t_fader_tilde), CLASS_MULTICHANNEL, A_DEFSYM, 0);
    class_addmethod(fader_tilde_class, nullfn, gensym("signal"), 0);
    class_addmethod(fader_tilde_class, (t_method)fader_tilde_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(fader_tilde_class, (t_method)fader_quartic, gensym("quartic"), 0);
    class_addmethod(fader_tilde_class, (t_method)fader_lin, gensym("lin"), 0);
    class_addmethod(fader_tilde_class, (t_method)fader_sqrt, gensym("sqrt"), 0);
    class_addmethod(fader_tilde_class, (t_method)fader_sin, gensym("sin"), 0);
    class_addmethod(fader_tilde_class, (t_method)fader_hann, gensym("hann"), 0);
    class_addmethod(fader_tilde_class, (t_method)fader_linsin, gensym("linsin"), 0);
    class_addmethod(fader_tilde_class, (t_method)fader_hannsin, gensym("hannsin"), 0);
}
