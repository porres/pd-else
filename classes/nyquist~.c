// Porres 2017

#include "m_pd.h"
#include "g_canvas.h"  // for LB_LOAD

static t_class *nyquist_class;

typedef struct _nyquist{
    t_object x_obj;
    int x_khz;
} t_nyquist;

static void nyquist_bang(t_nyquist *x){
    t_float nyq = sys_getsr() * 0.5;
    if(x->x_khz)
        nyq *= 0.001;
    outlet_float(x->x_obj.ob_outlet, nyq);
}

static void nyquist_hz(t_nyquist *x){
    x->x_khz = 0;
    nyquist_bang(x);
}

static void nyquist_khz(t_nyquist *x){
    x->x_khz = 1;
    nyquist_bang(x);
}

static void nyquist_loadbang(t_nyquist *x, t_floatarg action){
    if (action == LB_LOAD)
        nyquist_bang(x);
}

static void *nyquist_new(t_symbol *s, int argc, t_atom *argv){
    t_nyquist *x = (t_nyquist *)pd_new(nyquist_class);
    if (argv -> a_type == A_SYMBOL){
        t_symbol *curarg = atom_getsymbolarg(0, argc, argv);
        if(strcmp(curarg->s_name, "-khz")==0)
            x->x_khz = 1;
        else
            goto errstate;
    }
    outlet_new(&x->x_obj, &s_float);
    return (x);
    errstate:
        pd_error(x, "nyquist~: improper args");
        return NULL;
}

void nyquist_tilde_setup(void){
    nyquist_class = class_new(gensym("nyquist~"),
        (t_newmethod)nyquist_new, 0, sizeof(t_nyquist), 0, A_GIMME, 0);
    class_addmethod(nyquist_class, (t_method)nyquist_loadbang, gensym("loadbang"), A_DEFFLOAT, 0);
    class_addmethod(nyquist_class, (t_method)nyquist_hz, gensym("hz"), 0);
    class_addmethod(nyquist_class, (t_method)nyquist_khz, gensym("khz"), 0);
    class_addbang(nyquist_class, (t_method)nyquist_bang);
}
