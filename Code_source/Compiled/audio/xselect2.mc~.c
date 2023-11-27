// porres 2023

#include "m_pd.h"
#include <math.h>

#define MAXOUTPUT 512

static t_class *xselect2mc_class;

typedef struct _xselect2mc{
    t_object    x_obj;
    int         x_nchs;
    int         x_block;
    int         x_inchs;
    t_int       x_index;
    t_inlet    *x_inlet_spread;
}t_xselect2mc;

static void xselect2mc_index(t_xselect2mc *x, t_floatarg f){
    x->x_index = f != 0;
}

t_int *xselect2mc_perform(t_int *w){
    t_xselect2mc *x = (t_xselect2mc*)w[1];
    t_float *in = (t_float *)(w[2]);
    t_float *ch_select = (t_float *)(w[3]);
    t_float *spreadin = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    int n = x->x_block, inchs = x->x_inchs;
    for(int i = 0; i < n; i++){
        t_float pos = ch_select[i];
        t_float spread = spreadin[i];
        if(spread < 0.1)
            spread = 0.1;
        spread *= 2;
        float range = inchs / spread;
        if(!x->x_index)
            pos *= inchs;
        if(pos < 0)
            pos = 0;
        else if(pos > inchs)
            pos = inchs;
        pos += (spread * 0.5);
        for(int j = 0; j < inchs; j++){
            float chanpos = (pos - j) / spread;
            chanpos = chanpos - range * floor(chanpos/range);
            float chanamp = chanpos >= 1 ? 0 : sin(chanpos*M_PI);
            out[i] += in[j*n + i] * chanamp;
        }
	}
    return(w+6);
}

void xselect2mc_dsp(t_xselect2mc *x, t_signal **sp){
    x->x_block = sp[0]->s_n, x->x_inchs = sp[0]->s_nchans;
    signal_setmultiout(&sp[3], 1);
    if(sp[1]->s_nchans > 1 || sp[2]->s_nchans > 1){
        dsp_add_zero(sp[3]->s_vec, x->x_block);
        pd_error(x, "[xselect2.mc~] secondary input channels cannot be greater than 1");
        return;
    }
    dsp_add(xselect2mc_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

void xselect2mc_free(t_xselect2mc *x){
    inlet_free(x->x_inlet_spread);
}

void *xselect2mc_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_xselect2mc *x = (t_xselect2mc *)pd_new(xselect2mc_class);
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
        if(ac)
            spread = atom_getfloat(av);
    };
    x->x_nchs = 1;
    x->x_block = sys_getblksize();
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    x->x_inlet_spread = inlet_new(&x->x_obj, &x->x_obj.ob_pd,  &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_spread, spread);
    outlet_new(&x->x_obj, gensym("signal"));
    return(x);
    errstate:
        pd_error(x, "[xselect2.mc~]: improper args");
        return(NULL);
}

void setup_xselect20x2emc_tilde(void){
    xselect2mc_class = class_new(gensym("xselect2.mc~"), (t_newmethod)xselect2mc_new,
        (t_method)xselect2mc_free, sizeof(t_xselect2mc), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(xselect2mc_class, nullfn, gensym("signal"), 0);
    class_addmethod(xselect2mc_class, (t_method)xselect2mc_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(xselect2mc_class, (t_method)xselect2mc_index, gensym("index"), A_FLOAT, 0);
}
