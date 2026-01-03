// porres 2023

#include <m_pd.h>
#include <buffer.h>

#define MAXINTPUT 4096

static t_class *xsel2mc_class;

typedef struct _xsel2mc{
    t_object    x_obj;
    int         x_nchs;
    int         x_block;
    int         x_inchs;
    t_int       x_index;
    int         x_circular;
    t_inlet    *x_inlet_spread;
}t_xsel2mc;

static void xsel2mc_index(t_xsel2mc *x, t_floatarg f){
    x->x_index = f != 0;
}

static void xsel2mc_circular(t_xsel2mc *x, t_floatarg f){
    x->x_circular = f != 0;
}

t_int *xsel2mc_perform(t_int *w){
    t_xsel2mc *x = (t_xsel2mc*)w[1];
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
        if(!x->x_index)
            pos *= (inchs - !x->x_circular);
        if(x->x_circular){
            while(pos < 0)
                pos += 1;
            while(pos > inchs)
                pos -= inchs;
        }
        pos += spread;
        spread *= 2;
        int j;
        if(x->x_circular){
            float range = inchs / spread;
            for(j = 0; j < inchs; j++){
                float chanpos = (pos - j) / spread;
                chanpos = chanpos - range * floor(chanpos/range);
                float g = chanpos >= 1 ? 0 : read_sintab(chanpos*0.5);
                out[i] += in[j*n + i] * g;
            }
        }
        else for(j = 0; j < inchs; j++){
            float chanpos = (pos - j) / spread;
            if(chanpos >= 1 || chanpos < 0)
                chanpos = 0;
            out[i] += in[j*n + i] * read_sintab(chanpos*0.5);
        }
	}
    return(w+6);
}

void xsel2mc_dsp(t_xsel2mc *x, t_signal **sp){
    x->x_block = sp[0]->s_n, x->x_inchs = sp[0]->s_nchans;
    signal_setmultiout(&sp[3], 1);
    if(sp[1]->s_nchans > 1 || sp[2]->s_nchans > 1){
        dsp_add_zero(sp[3]->s_vec, x->x_block);
        pd_error(x, "[xselect2.mc~] secondary input channels cannot be greater than 1");
        return;
    }
    dsp_add(xsel2mc_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

void xsel2mc_free(t_xsel2mc *x){
    inlet_free(x->x_inlet_spread);
}

void *xsel2mc_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_xsel2mc *x = (t_xsel2mc *)pd_new(xsel2mc_class);
    float spread = 1;
    x->x_index = x->x_circular = 0;
    if(ac){
        while(av->a_type == A_SYMBOL){
            if(atom_getsymbol(av) == gensym("-index")){
                x->x_index = 1;
                ac--, av++;
            }
            else if(atom_getsymbol(av) == gensym("-circular")){
                x->x_circular = 1;
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
    xsel2mc_class = class_new(gensym("xselect2.mc~"), (t_newmethod)xsel2mc_new,
        (t_method)xsel2mc_free, sizeof(t_xsel2mc), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(xsel2mc_class, nullfn, gensym("signal"), 0);
    class_addmethod(xsel2mc_class, (t_method)xsel2mc_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(xsel2mc_class, (t_method)xsel2mc_index, gensym("index"), A_FLOAT, 0);
    class_addmethod(xsel2mc_class, (t_method)xsel2mc_circular, gensym("circular"), A_FLOAT, 0);
    init_sine_table();
}
