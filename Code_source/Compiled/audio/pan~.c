// porres

#include "m_pd.h"
#include "buffer.h"

#define MAXOUTPUT 512

typedef struct _pan{
    t_object    x_obj;
    t_float   **x_ins;          // inputs
    t_float   **x_outs;         // outputs
    t_inlet    *x_inlet_spread;
    t_inlet    *x_inlet_gain;
    int         x_n;            // block size
    int         x_n_outlets;    // outlets excluding main signal
    t_float     x_offset;
}t_pan;

static t_class *pan_class;

static t_int *pan_perform(t_int *w){
    t_pan *x = (t_pan *)(w[1]);
    for(int i = 0; i < x->x_n; i++){
        t_float in = x->x_ins[0][i];
        t_float g = x->x_ins[1][i];
        t_float pos = x->x_ins[2][i];
        t_float spread = x->x_ins[3][i];
        pos -= x->x_offset;
        while(pos < 0)
            pos += 1;
        while(pos >= 1)
            pos -= 1;
        if(spread < 0.1)
            spread = 0.1;
        pos = pos * x->x_n_outlets + spread;
        spread *= 2;
        float range = x->x_n_outlets / spread;
        for(int j = 0; j < x->x_n_outlets; j++){
            float chanpos = (pos - j) / spread;
            chanpos = chanpos - range * floor(chanpos/range);
            float chanamp = chanpos >= 1 ? 0 : read_sintab(chanpos*0.5);
            x->x_outs[j][i] = (in * chanamp) * g;
        }
    };
    return(w+2);
}

static void pan_dsp(t_pan *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    t_signal **sigp = sp;
    int i;
    for(i = 0; i < 4; i++) // inlets
        *(x->x_ins+i) = (*sigp++)->s_vec;
    for(i = 0; i < x->x_n_outlets; i++) // outlets
        *(x->x_outs+i) = (*sigp++)->s_vec;
    dsp_add(pan_perform, 1, x);
}

static void pan_offset(t_pan *x, t_floatarg f){
    x->x_offset = (f < 0 ? 0 : f) / 360;
}

void *pan_free(t_pan *x){
    freebytes(x->x_outs, x->x_n_outlets * sizeof(*x->x_outs));
    freebytes(x->x_outs, 4 * sizeof(*x->x_ins));
    inlet_free(x->x_inlet_spread);
    inlet_free(x->x_inlet_gain);
    return(void *)x;
}

static void *pan_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_pan *x = (t_pan *)pd_new(pan_class);
    init_sine_table();
    t_float n_outlets = 2;
    float spread = 1, gain = 1;
    x->x_offset = 90. / 360.;
    if(atom_getsymbol(av) == gensym("-offset")){
        ac--, av++;
        x->x_offset = atom_getfloat(av) / 360.;
        ac--, av++;
    }
    if(ac){
        n_outlets = atom_getint(av);
        ac--, av++;
    }
    if(ac){
        spread = atom_getfloat(av);
        ac--, av++;
    }
    if(ac){
        gain = atom_getfloat(av);
        ac--, av++;
    }
    if(n_outlets < 2)
        n_outlets = 2;
    else if(n_outlets > (t_float)MAXOUTPUT)
        n_outlets = MAXOUTPUT;
    x->x_n_outlets = (int)n_outlets;
    x->x_ins = getbytes(4 * sizeof(*x->x_ins));
    x->x_outs = getbytes(n_outlets * sizeof(*x->x_outs));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    x->x_inlet_gain = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_gain, gain);
    x->x_inlet_spread = inlet_new(&x->x_obj, &x->x_obj.ob_pd,  &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_spread, spread);
    for(int i = 0; i < n_outlets; i++)
        outlet_new((t_object *)x, &s_signal);
    return(x);
}

void pan_tilde_setup(void){
    pan_class = class_new(gensym("pan~"), (t_newmethod)pan_new,
        (t_method)pan_free, sizeof(t_pan), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(pan_class, nullfn, gensym("signal"), 0);
    class_addmethod(pan_class, (t_method)pan_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(pan_class, (t_method)pan_offset, gensym("offset"), A_FLOAT, 0);
}
