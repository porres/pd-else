// porres 2024

#include "m_pd.h"
#include "buffer.h"

#define MAX_INOUT 4096

static t_class *spread_class;

typedef struct _spread{
    t_object    x_obj;
    int         x_n, x_n_ins, x_n_outs;
    t_float   **x_ins, **x_outs;         // inputs and outputs
    t_float    *x_input;                 // inputs copy
    t_float    *x_spread;                // spread signal
    t_float     x_ratio;
    t_inlet    *x_inlet_spread;
}t_spread;

t_int *spread_perform(t_int *w){
    t_spread *x = (t_spread*) w[1];
	int n, ch, i, o, n_ins = x->x_n_ins, n_outs = x->x_n_outs;
    t_float *in = x->x_input;
	for(n = 0; n < x->x_n; n++){ // block loop
        t_float spread = x->x_spread[n];
        if(spread < 0.1)
            spread = 0.1;
        float spread2 = spread*2;
        float range = n_outs / spread2;
        for(ch = 0; ch < n_ins; ch++) // copy input sample frames
            in[ch] = x->x_ins[ch][n];
        for(ch = 0; ch < n_outs; ch++) // zero outs
            x->x_outs[ch][n] = 0.0;
        for(i = 0; i < n_ins; i++){ // in loop
            float pos = i * x->x_ratio + spread;
            for(o = 0; o < n_outs; o++){ // out loop
                float chanpos = (pos - o) / spread2;
                chanpos = chanpos - range * floor(chanpos/range);
                float g = chanpos >= 1 ? 0 : read_sintab(chanpos*0.5);
                x->x_outs[o][n] += in[i] * g;
            }
        }
	}
    return(w+2);
}

static void spread_dsp(t_spread *x, t_signal **sp){
    int i;
    x->x_n = sp[0]->s_n;
    t_signal **sigp = sp;
    for(i = 0; i < x->x_n_ins; i++)  // 'n' inlets
        *(x->x_ins+i) = (*sigp++)->s_vec;
    x->x_spread = (*sigp++)->s_vec;  // spread
    for(i = 0; i < x->x_n_outs; i++) // 'n' outlets
        *(x->x_outs+i) = (*sigp++)->s_vec;
    dsp_add(spread_perform, 1, x);
}

void spread_free(t_spread *x){
    freebytes(x->x_input, x->x_n_ins * sizeof(*x->x_input));
    freebytes(x->x_ins, x->x_n_ins * sizeof(*x->x_ins));
    freebytes(x->x_outs, x->x_n_outs * sizeof(*x->x_outs));
    inlet_free(x->x_inlet_spread);
}

void *spread_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_spread *x = (t_spread *)pd_new(spread_class);
    init_sine_table();
    int i, ins = 2, outs = 2;
    float spread = 1;
    x->x_n = sys_getblksize();
    if(ac){
        ins = atom_getint(av);
        ac--, av++;
    }
    if(ac){
        outs = atom_getint(av);
        ac--, av++;
    }
    if(ac){
        spread = atom_getfloat(av);
        ac--, av++;
    }
    x->x_n_ins = ins < 2 ? 2 : ins > MAX_INOUT ? MAX_INOUT : ins;
    x->x_n_outs = outs < 2 ? 2 : outs > MAX_INOUT ? MAX_INOUT : outs;
    x->x_ratio = (t_float)(x->x_n_outs - 1)/(t_float)(x->x_n_ins - 1);
    x->x_input = getbytes(x->x_n_ins * sizeof(*x->x_input));
    x->x_ins = getbytes(x->x_n_ins * sizeof(*x->x_ins));
    x->x_outs = getbytes(x->x_n_outs * sizeof(*x->x_outs));
    for(i = 1; i < x->x_n_ins; i++)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("signal"),gensym("signal"));
    x->x_inlet_spread = inlet_new(&x->x_obj, &x->x_obj.ob_pd,  &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_spread, spread);
    for(i = 0; i < x->x_n_outs; i++)
        outlet_new(&x->x_obj, gensym("signal"));
    return(x);
}

void spread_tilde_setup(void){
    spread_class = class_new(gensym("spread~"), (t_newmethod)spread_new,
        (t_method)spread_free, sizeof(t_spread), 0, A_GIMME, 0);
    class_addmethod(spread_class, nullfn, gensym("signal"), 0);
    class_addmethod(spread_class, (t_method)spread_dsp, gensym("dsp"),0);
}
