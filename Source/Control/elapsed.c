
#include "m_pd.h"

static t_class *elapsed_class;

typedef struct _elapsed{
    t_object        x_obj;
    double          x_settime;
    unsigned char   x_active;
}t_elapsed;

static void elapsed_reset(t_elapsed *x){
    x->x_settime = clock_getlogicaltime();
}

static void elapsed_bang(t_elapsed *x){
    if(!x->x_active){
        x->x_active = 1;
        elapsed_reset(x);
        outlet_float(x->x_obj.ob_outlet, 0);
    }
    else{
        double ms = clock_gettimesince(x->x_settime);
        outlet_float(x->x_obj.ob_outlet, ms);
    }
}

static void *elapsed_new(t_floatarg f){
    t_elapsed *x = (t_elapsed *)pd_new(elapsed_class);
    x->x_active = f != 0;
    if(x->x_active)
        elapsed_reset(x);
    outlet_new(&x->x_obj, gensym("float"));
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("bang"), gensym("reset"));
    return(x);
}

void elapsed_setup(void){
    elapsed_class = class_new(gensym("elapsed"), (t_newmethod)(void *)elapsed_new,
        0, sizeof(t_elapsed), 0, A_DEFFLOAT, 0);
    class_addbang(elapsed_class, elapsed_bang);
    class_addmethod(elapsed_class, (t_method)elapsed_reset, gensym("reset"), 0);
}
