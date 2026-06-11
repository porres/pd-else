// porres 2018-2026

#include <m_pd.h>
#include <g_canvas.h>
#include "s_stuff.h"  // for DEFDACBLKSIZE

static t_class *blocksize_class;

typedef struct _blocksize{
    t_object    x_obj;
    float       x_size;
    float       x_sr;
    int         x_mode;
    t_symbol   *x_sym;    // [v] name
}t_blocksize;

static void blocksize_bang(t_blocksize *x){
    float size = x->x_size;
    if(x->x_mode == 1)
        size *= (1000./x->x_sr);
    else if(x->x_mode == 2)
        size = x->x_sr/size;
    if(x->x_sym != &s_)
        value_setfloat(x->x_sym, size);
    outlet_float(x->x_obj.ob_outlet, size);
}

static void blocksize_click(t_blocksize *x, t_floatarg xpos,
t_floatarg ypos, t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos; (void)ypos; (void)shift; (void)ctrl; (void)alt;
    blocksize_bang(x);
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

static void blocksize_loadbang(t_blocksize *x, t_floatarg action){
    if(action == LB_INIT)
        blocksize_bang(x);
}

static void blocksize_dsp(t_blocksize *x, t_signal **sp){
    if(x->x_size != (float)sp[0]->s_n){
        x->x_size = (float)sp[0]->s_n;
        blocksize_bang(x);
    }
    if(x->x_mode && x->x_sr != (float)sp[0]->s_sr){
        x->x_sr = (float)sp[0]->s_sr;
        blocksize_bang(x);
    }
}

static void *blocksize_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_blocksize *x = (t_blocksize *)pd_new(blocksize_class);
    x->x_sym = &s_;
    x->x_mode = 0;
    x->x_size = DEFDACBLKSIZE;
    x->x_sr = (float)sys_getsr();
    if(ac > 2)
        goto errstate;
    while(ac){
        if(av->a_type == A_SYMBOL){
            t_symbol *sym = atom_getsymbol(av);
            if(sym == gensym("-ms"))
                x->x_mode = 1;
            else if(sym == gensym("-hz"))
                x->x_mode = 2;
            else
                value_get(x->x_sym = atom_getsymbol(av));
            ac--, av++;
        }
        else
            goto errstate;
    }
    outlet_new(&x->x_obj, &s_float);
    return(x);
errstate:
    pd_error(x, "[blocksize~]: improper args");
    return(NULL);
}

void blocksize_tilde_setup(void){
    blocksize_class = class_new(gensym("blocksize~"), (t_newmethod)(void*)blocksize_new,
        0, sizeof(t_blocksize), 0, A_GIMME, 0);
    class_addmethod(blocksize_class, nullfn, gensym("signal"), 0);
    class_addmethod(blocksize_class, (t_method)blocksize_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(blocksize_class, (t_method)blocksize_loadbang, gensym("loadbang"), A_DEFFLOAT, 0);
    class_addmethod(blocksize_class, (t_method)blocksize_ms, gensym("ms"), 0);
    class_addmethod(blocksize_class, (t_method)blocksize_hz, gensym("hz"), 0);
    class_addmethod(blocksize_class, (t_method)blocksize_samps, gensym("samps"), 0);
    class_addbang(blocksize_class, (t_method)blocksize_bang);
    class_addmethod(blocksize_class, (t_method)blocksize_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT,0);
}
