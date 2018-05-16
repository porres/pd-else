// porres 2018

#include "m_pd.h"
#include <string.h>

static t_class *blocksize_class;

typedef struct _blocksize{
    t_object    x_obj;
    t_float     x_size;
    t_int       x_mode;
    t_int       x_sr;
} t_blocksize;

static void blocksize_bang(t_blocksize *x){
    t_float out = x->x_size;
    if(x->x_mode == 1)
        out *= (1000/(t_float)x->x_sr);
    else if(x->x_mode == 2)
        out = ((t_float)x->x_sr/out);
    outlet_float(x->x_obj.ob_outlet, out);
}

static void blocksize_samps(t_blocksize *x){
    if(x->x_mode != 0){
        x->x_mode = 0;
        blocksize_bang(x);
    }
}

static void blocksize_ms(t_blocksize *x){
    if(x->x_mode != 1){
        x->x_mode = 1;
        blocksize_bang(x);
    }
}

static void blocksize_hz(t_blocksize *x){
    if(x->x_mode != 2){
        x->x_mode = 2;
        blocksize_bang(x);
    }
}

static void blocksize_dsp(t_blocksize *x, t_signal **sp){
    t_float n = (t_float)sp[0]->s_n, out;
    if(n != x->x_size){
        x->x_size = out = n;
        if(x->x_mode){
            x->x_sr = sp[0]->s_sr;
            if(x->x_mode == 1)
                out *= (1000/(t_float)x->x_sr);
            else if(x->x_mode == 2)
                out = ((t_float)x->x_sr/out);
        }
        outlet_float(x->x_obj.ob_outlet, out);
    }
}

static void *blocksize_new(t_symbol *s, int ac, t_atom *av){
    t_blocksize *x = (t_blocksize *)pd_new(blocksize_class);
    x->x_size = x->x_mode = 0;
    if(ac && av->a_type == A_SYMBOL){
        t_symbol *sym = atom_getsymbolarg(0, ac, av);
        if(!strcmp(sym->s_name, "-ms"))
            x->x_mode = 1;
        else if(!strcmp(sym->s_name, "-hz"))
            x->x_mode = 2;
        else
            goto errstate;
    }
    outlet_new(&x->x_obj, &s_float);
    return (x);
errstate:
    pd_error(x, "blocksize~: improper args");
    return NULL;
}

void blocksize_tilde_setup(void){
    blocksize_class = class_new(gensym("blocksize~"), (t_newmethod)blocksize_new,
                                0, sizeof(t_blocksize), 0, A_GIMME, 0);
    class_addmethod(blocksize_class, nullfn, gensym("signal"), 0);
    class_addmethod(blocksize_class, (t_method)blocksize_dsp, gensym("dsp"), 0);
    class_addmethod(blocksize_class, (t_method)blocksize_ms, gensym("ms"), 0);
    class_addmethod(blocksize_class, (t_method)blocksize_hz, gensym("hz"), 0);
    class_addmethod(blocksize_class, (t_method)blocksize_samps, gensym("samps"), 0);
    class_addbang(blocksize_class, (t_method)blocksize_bang);
}
