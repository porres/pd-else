// Porres 2017-2026

#include <m_pd.h>
#include <g_canvas.h>

static t_class *sr_class;

typedef struct _sr{
    t_object    x_obj;
    float       x_sr;
    float       x_new_sr;
    int         x_khz;
    int         x_period;
    t_symbol   *x_sym;    // [v] name
}t_sr;

static void sr_bang(t_sr *x){
    float sr = sys_getsr();
    x->x_sr = x->x_new_sr = sr;
    if(x->x_khz)
        sr *= 0.001;
    if(x->x_period)
        sr = 1./sr;
    if(x->x_sym != &s_)
        value_setfloat(x->x_sym, sr);
    outlet_float(x->x_obj.ob_outlet, sr);
}

static void sr_click(t_sr *x, t_floatarg xpos,
t_floatarg ypos, t_floatarg shift, t_floatarg ctrl, t_floatarg alt){
    (void)xpos; (void)ypos; (void)shift; (void)ctrl; (void)alt;
    sr_bang(x);
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
    if(action == LB_INIT)
        sr_bang(x);
}

static void sr_dsp(t_sr *x, t_signal **sp){
    if(x->x_new_sr != (float)sp[0]->s_sr){
        x->x_new_sr = (float)sp[0]->s_sr;
        sr_bang(x);
    }
}

static void *sr_new(t_symbol *s, int ac, t_atom *av){
    t_sr *x = (t_sr *)pd_new(sr_class);
    x->x_khz = x->x_period = 0;
    x->x_sym = &s_;
    if(ac > 2)
        goto errstate;
    while(ac){
        if(av->a_type == A_SYMBOL){
            t_symbol *sym = atom_getsymbol(av);
            if(sym == gensym("-khz"))
                x->x_khz = 1;
            else if(sym == gensym("-ms"))
                x->x_khz = x->x_period = 1;
            else if(sym == gensym("-sec"))
                x->x_period = 1;
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
        pd_error(x, "[sr~]: improper args");
        return(NULL);
}

void sr_tilde_setup(void){
    sr_class = class_new(gensym("sr~"), (t_newmethod)(void*)sr_new, 0,
        sizeof(t_sr), 0, A_GIMME, 0);
    class_addmethod(sr_class, nullfn, gensym("signal"), 0);
    class_addmethod(sr_class, (t_method)sr_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(sr_class, (t_method)sr_loadbang, gensym("loadbang"), A_DEFFLOAT, 0);
    class_addmethod(sr_class, (t_method)sr_hz, gensym("hz"), 0);
    class_addmethod(sr_class, (t_method)sr_khz, gensym("khz"), 0);
    class_addmethod(sr_class, (t_method)sr_ms, gensym("ms"), 0);
    class_addmethod(sr_class, (t_method)sr_sec, gensym("sec"), 0);
    class_addbang(sr_class, (t_method)sr_bang);
    class_addmethod(sr_class, (t_method)sr_click, gensym("click"),
        A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT, A_FLOAT,0);
}
