// porres 2024

#include <m_pd.h>
#include <math.h>

#define MAX_INOUT 4096

static t_class *mixdown_class;

typedef struct _mixdown{
    t_object    x_obj;
    int         x_n;
    int         x_n_ins;
    int         x_nchans;
    int         x_norm;
    t_float     x_gain;
    t_float    *x_input;  // inputs copy
    t_symbol   *x_ignore;
}t_mixdown;

static void mixdown_n(t_mixdown *x, t_floatarg f){
    int n = f < 1 ? 1 : f > MAX_INOUT ? MAX_INOUT : (int)f;
    if(x->x_nchans != n){
        x->x_nchans = n;
        x->x_gain = x->x_norm ? 1./(float)x->x_nchans : 1;
        canvas_update_dsp();
    }
}

static void mixdown_norm(t_mixdown *x, t_floatarg f){
    int norm = f != 0;
    if(x->x_norm != norm){
        x->x_norm = norm;
        float ratio = ceil((float)x->x_n_ins/(float)x->x_nchans);
        x->x_gain = x->x_norm ? 1./ratio : 1;
    }
}

t_int *mixdown_perform(t_int *w){
    t_mixdown *x = (t_mixdown*) w[1];
    t_float *input = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    t_float *in = x->x_input;
    int ch, n_ins = x->x_n_ins, nchs = x->x_nchans;
	for(int n = 0; n < x->x_n; n++){ // block loop
        for(ch = 0; ch < n_ins; ch++) // copy input sample frames
            in[ch] = input[ch*x->x_n+n];
        for(ch = 0; ch < nchs; ch++) // zero outs
            out[ch*x->x_n+n] = 0.0;
        for(int i = 0; i < n_ins; i++) // input loop
            out[(i%nchs)*x->x_n + n] += in[i] * x->x_gain;
	}
    return(w+4);
}

void mixdown_dsp(t_mixdown *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    int ins = sp[0]->s_nchans;
    if(x->x_n_ins != ins){ // check if only one and add zero, check right inlet too
        x->x_input = (t_float *)resizebytes(x->x_input,
            x->x_n_ins * sizeof(t_float), ins * sizeof(t_float));
        x->x_n_ins = ins;
        float ratio = ceil((float)x->x_n_ins/(float)x->x_nchans);
        x->x_gain = x->x_norm ? 1./ratio : 1;
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(mixdown_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
}

void mixdown_free(t_mixdown *x){
    freebytes(x->x_input, x->x_n_ins * sizeof(*x->x_input));
}

void *mixdown_new(t_symbol *s, int ac, t_atom *av){
    t_mixdown *x = (t_mixdown *)pd_new(mixdown_class);
    x->x_ignore = s;
    x->x_n = sys_getblksize();
    x->x_n_ins = 1;
    x->x_input = getbytes(sizeof(*x->x_input));
    int outs = 1;
    x->x_norm = 0;
    if(ac){
        outs = atom_getint(av);
        ac--, av++;
        if(ac)
            x->x_norm = atom_getfloat(av) != 0;
    }
    x->x_nchans = outs < 1 ? 1 : outs > MAX_INOUT ? MAX_INOUT : outs;
    x->x_gain = x->x_norm ? 1./(float)x->x_nchans : 1;
    outlet_new(&x->x_obj, gensym("signal"));
    return(x);
}

void mixdown_tilde_setup(void){
    mixdown_class = class_new(gensym("mixdown~"), (t_newmethod)mixdown_new,
        (t_method)mixdown_free, sizeof(t_mixdown), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(mixdown_class, nullfn, gensym("signal"), 0);
    class_addmethod(mixdown_class, (t_method)mixdown_dsp, gensym("dsp"),0);
    class_addmethod(mixdown_class, (t_method)mixdown_n, gensym("n"), A_FLOAT, 0);
    class_addmethod(mixdown_class, (t_method)mixdown_norm, gensym("gain"), A_FLOAT, 0);
}
