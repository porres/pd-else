// porres

#include "m_pd.h"
#include <math.h>

#define MAXOUTPUT 512

typedef struct _xgate2{
    t_object    x_obj;
    t_float    *x_ch_select;    // channel selector signal
    t_float    *x_spread;       // spread signal
    t_int       x_index;
    t_inlet    *x_inlet_spread;
    int         x_n_outlets;    // outlets excluding main signal
    t_float   **x_ovecs;        // copying from matrix
    t_float    *x_ivec;         // single pointer since (an array rather than an array of arrays)
}t_xgate2;

static t_class *xgate2_class;

static void xgate2_index(t_xgate2 *x, t_floatarg f){
    x->x_index = f != 0;
}

static t_int *xgate2_perform(t_int *w){
    t_xgate2 *x = (t_xgate2 *)(w[1]);
    int nblock = (int)(w[2]);
    t_float *ivec = x->x_ivec;
    t_float *ch_select = x->x_ch_select;
    t_float *spreadin = x->x_spread;
    t_float **ovecs = x->x_ovecs;
    int n_outlets = x->x_n_outlets;
    int i, j;
    for(i = 0; i < nblock; i++){
        t_float input = ivec[i];
        t_float pos = ch_select[i];
        t_float spread = spreadin[i];
        if(spread < 0.1)
            spread = 0.1;
        spread *= 2;
        float range = x->x_n_outlets / spread;
        if(!x->x_index)
            pos *= n_outlets;
        if(pos < 0)
            pos = 0;
        else if(pos > n_outlets)
            pos = n_outlets;
        pos += (spread * 0.5);
        for(j = 0; j < n_outlets; j++){
            float chanpos = (pos - j) / spread;
            chanpos = chanpos - range * floor(chanpos/range);
            float chanamp = chanpos >= 1 ? 0 : sin(chanpos*M_PI);
            ovecs[j][i] = input * chanamp;
        }
    };
    return(w+3);
}

static void xgate2_dsp(t_xgate2 *x, t_signal **sp){
    int i, nblock = sp[0]->s_n;
    t_signal **sigp = sp;
    x->x_ivec = (*sigp++)->s_vec; // the input inlet
    x->x_ch_select = (*sigp++)->s_vec; // the idx inlet
    x->x_spread = (*sigp++)->s_vec; // the spread inlet
    for(i = 0; i < x->x_n_outlets; i++) // the n_outlets
        *(x->x_ovecs+i) = (*sigp++)->s_vec;
    dsp_add(xgate2_perform, 2, x, nblock);
}

static void *xgate2_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_xgate2 *x = (t_xgate2 *)pd_new(xgate2_class);
    t_float n_outlets = 2; //inlets not counting xgate2 input
    float spread = 1;
    if(ac){
        if(av->a_type == A_SYMBOL){
            t_symbol *curarg = atom_getsymbol(av);
            if(curarg == gensym("-index")){
                x->x_index = 1;
                ac--, av++;
            }
            else
                goto errstate;
        };
        if(ac){
            n_outlets = atom_getfloat(av);
            ac--, av++;
            if(ac)
                spread = atom_getfloat(av);
        }
    };
    if(n_outlets < 2)
        n_outlets = 2;
    else if(n_outlets > (t_float)MAXOUTPUT)
        n_outlets = MAXOUTPUT;
    x->x_n_outlets = (int)n_outlets;
    x->x_ovecs = getbytes(n_outlets * sizeof(*x->x_ovecs));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    x->x_inlet_spread = inlet_new(&x->x_obj, &x->x_obj.ob_pd,  &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_spread, spread);
    for(int i = 0; i < n_outlets; i++)
        outlet_new((t_object *)x, &s_signal);
    return(x);
    errstate:
        pd_error(x, "[xgate2~]: improper args");
        return(NULL);
}

void * xgate2_free(t_xgate2 *x){
    freebytes(x->x_ovecs, x->x_n_outlets * sizeof(*x->x_ovecs));
    inlet_free(x->x_inlet_spread);
    return(void *)x;
}

void xgate2_tilde_setup(void){
    xgate2_class = class_new(gensym("xgate2~"), (t_newmethod)xgate2_new,
        (t_method)xgate2_free, sizeof(t_xgate2), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(xgate2_class, nullfn, gensym("signal"), 0);
    class_addmethod(xgate2_class, (t_method)xgate2_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(xgate2_class, (t_method)xgate2_index, gensym("index"), A_FLOAT, 0);
}
