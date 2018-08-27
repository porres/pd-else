// porres 2018

#include "m_pd.h"
#include <math.h>

typedef struct _power{
    t_object    x_obj;
    t_float     x_f1;
    t_float     x_f2;
} t_power;

static t_class *power_class;

static void power_bang(t_power *x){
    outlet_float(x->x_obj.ob_outlet, pow(x->x_f1, x->x_f2));
}

static void power_float(t_power *x, t_float f){
    x->x_f1 = f;
    power_bang(x);
}

static void *power_new(t_floatarg f){
    t_power *x = (t_power *)pd_new(power_class);
    outlet_new(&x->x_obj, &s_float);
    floatinlet_new(&x->x_obj, &x->x_f2);
    x->x_f1 = 0;
    x->x_f2 = f;
    return(x);
}

void power_setup(void){
    power_class = class_new(gensym("power"), (t_newmethod)power_new, 0,
        sizeof(t_power), 0, A_DEFFLOAT, 0);
    class_addbang(power_class, power_bang);
    class_addfloat(power_class, power_float);
}
