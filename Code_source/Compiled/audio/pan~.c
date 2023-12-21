// porres

#include "m_pd.h"
#include <math.h>

#define MAXOUTPUT 512

typedef struct _pan{
    t_object    x_obj;
    t_float    *x_azimuth;      // channel selector signal
    t_float    *x_spread;       // spread signal
    t_float    *x_gain;         // spread signal
    t_inlet    *x_inlet_spread;
    t_inlet    *x_inlet_gain;
    int         x_n_outlets;    // outlets excluding main signal
    t_float   **x_ovecs;        // copying from matrix
    t_float    *x_ivec;         // single pointer since (an array rather than an array of arrays)
}t_pan;

static t_class *pan_class;

static t_int *pan_perform(t_int *w){
    t_pan *x = (t_pan *)(w[1]);
    int nblock = (int)(w[2]);
    t_float *ivec = x->x_ivec;
    t_float *gain = x->x_gain;
    t_float *azimuth = x->x_azimuth;
    t_float *spreadin = x->x_spread;
    t_float **ovecs = x->x_ovecs;
    int n_outlets = x->x_n_outlets;
    for(int i = 0; i < nblock; i++){
        t_float input = ivec[i];
        t_float g = gain[i];
        t_float pos = azimuth[i];
        t_float spread = spreadin[i];
        if(spread < 0.1)
            spread = 0.1;
        pos *= n_outlets;
        if(pos < 0)
            pos = 0;
        else if(pos > n_outlets)
            pos = n_outlets;
        pos += spread;
        spread *= 2;
        float range = n_outlets / spread;
        for(int j = 0; j < n_outlets; j++){
            float chanpos = (pos - j) / spread;
            chanpos = chanpos - range * floor(chanpos/range);
            float chanamp = chanpos >= 1 ? 0 : sin(chanpos*M_PI);
            ovecs[j][i] = (input * chanamp) * g;
        }
    };
    return(w+3);
}

static void pan_dsp(t_pan *x, t_signal **sp){
    int i, nblock = sp[0]->s_n;
    t_signal **sigp = sp;
    x->x_ivec = (*sigp++)->s_vec;       // the input inlet
    x->x_azimuth = (*sigp++)->s_vec;    // the azimuth inlet
    x->x_gain = (*sigp++)->s_vec;       // the gain inlet
    x->x_spread = (*sigp++)->s_vec;     // the spread inlet
    for(i = 0; i < x->x_n_outlets; i++) // the n_outlets
        *(x->x_ovecs+i) = (*sigp++)->s_vec;
    dsp_add(pan_perform, 2, x, nblock);
}

static void *pan_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_pan *x = (t_pan *)pd_new(pan_class);
    t_float n_outlets = 2;
    float spread = 1, gain = 1;
    if(ac){
        n_outlets = atom_getfloat(av);
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
    x->x_ovecs = getbytes(n_outlets * sizeof(*x->x_ovecs));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    x->x_inlet_gain = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_gain, gain);
    x->x_inlet_spread = inlet_new(&x->x_obj, &x->x_obj.ob_pd,  &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_spread, spread);
    for(int i = 0; i < n_outlets; i++)
        outlet_new((t_object *)x, &s_signal);
    return(x);
}

void *pan_free(t_pan *x){
    freebytes(x->x_ovecs, x->x_n_outlets * sizeof(*x->x_ovecs));
    inlet_free(x->x_inlet_spread);
    inlet_free(x->x_inlet_gain);
    return(void *)x;
}

void pan_tilde_setup(void){
    pan_class = class_new(gensym("pan~"), (t_newmethod)pan_new,
        (t_method)pan_free, sizeof(t_pan), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(pan_class, nullfn, gensym("signal"), 0);
    class_addmethod(pan_class, (t_method)pan_dsp, gensym("dsp"), A_CANT, 0);
}
