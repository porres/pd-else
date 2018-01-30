// Porres 2017

#include "m_pd.h"
#include "g_canvas.h"  // for LB_LOAD

static t_class *sr_class;

typedef struct _sr{
    t_object x_obj;
    int x_khz;
    int x_period;
} t_sr;

static void sr_bang(t_sr *x){
    t_float srate = sys_getsr();
    if(x->x_khz)
        srate *= 0.001;
    if(x->x_period)
        srate = 1./srate;
    outlet_float(x->x_obj.ob_outlet, srate);
}

static void sr_hz(t_sr *x){
    x->x_khz = x->x_period = 0;
    sr_bang(x);
}

static void sr_khz(t_sr *x){
    x->x_khz = 1;
    x->x_period = 0;
    sr_bang(x);
}

static void sr_ms(t_sr *x){
    x->x_khz = x->x_period = 1;
    sr_bang(x);
}

static void sr_sec(t_sr *x){
    x->x_khz = 0;
    x->x_period = 1;
    sr_bang(x);
}

static void sr_loadbang(t_sr *x, t_floatarg action){
    if (action == LB_LOAD)
        sr_bang(x);
}

static void *sr_new(t_symbol *s, int argc, t_atom *argv){
    t_sr *x = (t_sr *)pd_new(sr_class);
    x->x_khz = x->x_period = 0;
    if (argv -> a_type == A_SYMBOL){
        t_symbol *curarg = atom_getsymbolarg(0, argc, argv);
        if(strcmp(curarg->s_name, "-khz")==0)
            x->x_khz = 1;
        else if(strcmp(curarg->s_name, "-ms")==0)
            x->x_khz = x->x_period = 1;
        else if(strcmp(curarg->s_name, "-sec")==0)
            x->x_period = 1;
        else
            goto errstate;
    }
    outlet_new(&x->x_obj, &s_float);
    return (x);
    errstate:
        pd_error(x, "sr~: improper args");
        return NULL;
}

void sr_tilde_setup(void){
    sr_class = class_new(gensym("sr~"),
        (t_newmethod)sr_new, 0, sizeof(t_sr), 0, A_GIMME, 0);
    class_addmethod(sr_class, (t_method)sr_loadbang, gensym("loadbang"), A_DEFFLOAT, 0);
    class_addmethod(sr_class, (t_method)sr_hz, gensym("hz"), 0);
    class_addmethod(sr_class, (t_method)sr_khz, gensym("khz"), 0);
    class_addmethod(sr_class, (t_method)sr_ms, gensym("ms"), 0);
    class_addmethod(sr_class, (t_method)sr_sec, gensym("sec"), 0);
    class_addbang(sr_class, (t_method)sr_bang);
}
