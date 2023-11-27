
#include "m_pd.h"
#include <math.h>

#define MAXINTPUT 512

typedef struct _xselect2{
    t_object    x_obj;
    t_float    *x_ch_select; // main signal (channel selector)
    int 	    x_n_inlets; // inlets excluding main signal
    int 	    x_indexed; // inlets excluding main signal
    t_float    *x_spread; // spread signal
    t_inlet    *x_inlet_spread;
    t_float   **x_ivecs; // copying from matrix
    t_float    *x_ovec; // single pointer (an array rather than an array of arrays)
}t_xselect2;

static t_class *xselect2_class;

static void xselect2_index(t_xselect2 *x, t_floatarg f){
    x->x_indexed = f != 0;
}

static t_int *xselect2_perform(t_int *w){
    t_xselect2 *x = (t_xselect2 *)(w[1]);
    int nblock = (int)(w[2]);
    t_float **ivecs = x->x_ivecs;
    t_float *channel = x->x_ch_select;
    t_float *spreadin = x->x_spread;
    t_float *ovec = x->x_ovec;
    int n_inlets = x->x_n_inlets;
    for(int i = 0; i < nblock; i++){
        t_float output = 0;
        t_float pos = channel[i];
        t_float spread = spreadin[i];
        if(spread < 0.1)
            spread = 0.1;
        spread *= 2;
        float range = n_inlets / spread;
        if(!x->x_indexed)
            pos *= n_inlets;
        if(pos < 0)
            pos = 0;
        else if(pos > n_inlets)
            pos = n_inlets;
        pos += (spread * 0.5);
        for(int j = 0; j < n_inlets; j++){
            float chanpos = (pos - j) / spread;
            chanpos = chanpos - range * floor(chanpos/range);
            float chanamp = chanpos >= 1 ? 0 : sin(chanpos*M_PI);
            output += ivecs[j][i] * chanamp;
        }
        ovec[i] = output;
    };
    return(w+3);
}

static void xselect2_dsp(t_xselect2 *x, t_signal **sp){
    int i, nblock = sp[0]->s_n;
    t_signal **sigp = sp;
    for(i = 0; i < x->x_n_inlets; i++) // n_inlets
        *(x->x_ivecs+i) = (*sigp++)->s_vec;
    x->x_ch_select = (*sigp++)->s_vec; // idx
    x->x_spread = (*sigp++)->s_vec; // the spread inlet
    x->x_ovec = (*sigp++)->s_vec; // now for the outlet
    dsp_add(xselect2_perform, 2, x, nblock);
}

static void *xselect2_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_xselect2 *x = (t_xselect2 *)pd_new(xselect2_class);
    t_float n_inlets = 2; // inlets not counting channel selection
    x->x_indexed = 0;
    float spread = 1;
    int i;
    if(ac){
        if(av->a_type == A_SYMBOL){
            t_symbol *curarg = atom_getsymbol(av);
            if(curarg == gensym("-index")){
                x->x_indexed = 1;
                ac--, av++;
            }
            else
                goto errstate;
        };
        if(ac){
            n_inlets = atom_getfloat(av);
            ac--, av++;
            if(ac)
                spread = atom_getfloat(av);
        }
    };
    if(n_inlets < 2)
        n_inlets = 2;
    else if(n_inlets > (t_float)MAXINTPUT)
        n_inlets = MAXINTPUT;
    x->x_n_inlets = (int)n_inlets;
    x->x_ivecs = getbytes(n_inlets * sizeof(*x->x_ivecs));
    for(i = 0; i < n_inlets; i++)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    x->x_inlet_spread = inlet_new(&x->x_obj, &x->x_obj.ob_pd,  &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_spread, spread);
    outlet_new((t_object *)x, &s_signal);
    return(x);
    errstate:
        pd_error(x, "xselect2~: improper args");
        return(NULL);
}

void * xselect2_free(t_xselect2 *x){
    freebytes(x->x_ivecs, x->x_n_inlets * sizeof(*x->x_ivecs));
    inlet_free(x->x_inlet_spread);
    return(void *)x;
}

void xselect2_tilde_setup(void){
    xselect2_class = class_new(gensym("xselect2~"), (t_newmethod)xselect2_new,
        (t_method)xselect2_free, sizeof(t_xselect2), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(xselect2_class, (t_method)xselect2_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(xselect2_class, nullfn, gensym("signal"), 0);
    class_addmethod(xselect2_class, (t_method)xselect2_index, gensym("index"), A_FLOAT, 0);
}
