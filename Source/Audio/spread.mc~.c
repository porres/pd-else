// porres 2024

#include "m_pd.h"
#include "buffer.h"

#define MAX_INOUT 4096

static t_class *spreadmc_class;

typedef struct _spreadmc{
    t_object    x_obj;
    int         x_n, x_n_ins, x_n_outs;
    t_float    *x_input;                 // inputs copy
    t_float     x_ratio;
    t_inlet    *x_inlet_spread;
}t_spreadmc;

static void spreadmc_n(t_spreadmc *x, t_floatarg f){
    int n = f < 2 ? 2 : f > MAX_INOUT ? MAX_INOUT : (int)f;
    if(x->x_n_outs != n){
        x->x_n_outs = n;
        x->x_ratio = (float)(x->x_n_outs - 1)/(float)(x->x_n_ins - 1);
        canvas_update_dsp();
    }
}

t_int *spreadmc_perform(t_int *w){
    t_spreadmc *x = (t_spreadmc*) w[1];
    t_float *input = (t_float *)(w[2]);
    t_float *spreadin = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    t_float *in = x->x_input;
	int n, ch, i, o, n_ins = x->x_n_ins, n_outs = x->x_n_outs;
	for(n = 0; n < x->x_n; n++){ // block loop
        t_float spread = spreadin[n];
        if(spread < 0.1)
            spread = 0.1;
        float spread2 = spread*2;
        float range = n_outs / spread2;
        for(ch = 0; ch < n_ins; ch++) // copy input sample frames
            in[ch] = input[ch*x->x_n+n];
        for(ch = 0; ch < n_outs; ch++) // zero outs
            out[ch*x->x_n+n] = 0.0;
        for(i = 0; i < n_ins; i++){ // in loop
            float pos = i * x->x_ratio + spread;
            for(o = 0; o < n_outs; o++){ // out loop
                float chanpos = (pos - o) / spread2;
                chanpos = chanpos - range * floor(chanpos/range);
                float g = chanpos >= 1 ? 0 : read_sintab(chanpos*0.5);
                out[o*x->x_n+n] += in[i] * g;
            }
        }
	}
    return(w+5);
}

void spreadmc_dsp(t_spreadmc *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    int ins = sp[0]->s_nchans;
    if(x->x_n_ins != ins){ // check if only one and add zero, check right inlet too
        x->x_input = (t_float *)resizebytes(x->x_input,
            x->x_n_ins * sizeof(t_float), ins * sizeof(t_float));
        x->x_n_ins = ins;
        x->x_ratio = (float)(x->x_n_outs - 1)/(float)(x->x_n_ins - 1);
    }
    signal_setmultiout(&sp[2], x->x_n_outs);
    if(x->x_n_ins  == 1)
        dsp_add_zero(sp[2]->s_vec, x->x_n_outs*x->x_n);
    else
        dsp_add(spreadmc_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

void spreadmc_free(t_spreadmc *x){
    freebytes(x->x_input, x->x_n_ins * sizeof(*x->x_input));
    inlet_free(x->x_inlet_spread);
}

void *spreadmc_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_spreadmc *x = (t_spreadmc *)pd_new(spreadmc_class);
    init_sine_table();
    int outs = 2;
    float spread = 1;
    x->x_n = sys_getblksize();
    if(ac){
        outs = atom_getint(av);
        ac--, av++;
    }
    if(ac){
        spread = atom_getfloat(av);
        ac--, av++;
    }
    x->x_n_ins = 2;
    x->x_n_outs = outs < 2 ? 2 : outs > MAX_INOUT ? MAX_INOUT : outs;
    x->x_ratio = (t_float)(x->x_n_outs - 1)/(t_float)(x->x_n_ins - 1);
    x->x_input = getbytes(x->x_n_ins * sizeof(*x->x_input));
    x->x_inlet_spread = inlet_new(&x->x_obj, &x->x_obj.ob_pd,  &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_spread, spread);
    outlet_new(&x->x_obj, gensym("signal"));
    return(x);
}

void setup_spread0x2emc_tilde(void){
    spreadmc_class = class_new(gensym("spread.mc~"), (t_newmethod)spreadmc_new,
        (t_method)spreadmc_free, sizeof(t_spreadmc), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(spreadmc_class, nullfn, gensym("signal"), 0);
    class_addmethod(spreadmc_class, (t_method)spreadmc_dsp, gensym("dsp"),0);
    class_addmethod(spreadmc_class, (t_method)spreadmc_n, gensym("n"), A_DEFFLOAT, 0);
}
