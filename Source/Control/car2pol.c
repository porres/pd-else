// porres

#include <math.h>
#include "m_pd.h"

#if defined(_WIN32) || defined(__APPLE__)
#define atan2f  atan2
#define hypotf  hypot
#endif

typedef struct _car2pol{
    t_object   x_obj;
    t_float    x_real;
    t_float    x_imag;
    t_outlet  *x_out2;
}t_car2pol;

static t_class *car2pol_class;

static void car2pol_float(t_car2pol *x, t_float f){
    outlet_float(x->x_out2, atan2f(x->x_imag, x->x_real = f));
    outlet_float(x->x_obj.ob_outlet, hypotf(x->x_real, x->x_imag));
}

static void car2pol_bang(t_car2pol *x){
    outlet_float(x->x_out2, atan2f(x->x_imag, x->x_real));
    outlet_float(x->x_obj.ob_outlet, hypotf(x->x_real, x->x_imag));
}

static void *car2pol_new(void){
    t_car2pol *x = (t_car2pol *)pd_new(car2pol_class);
    floatinlet_new((t_object *)x, &x->x_imag);
    outlet_new((t_object *)x, &s_float);
    x->x_out2 = outlet_new((t_object *)x, &s_float);
    return(x);
}

void car2pol_setup(void){
    car2pol_class = class_new(gensym("car2pol"), (t_newmethod)car2pol_new,
        0, sizeof(t_car2pol), 0, 0);
    class_addfloat(car2pol_class, car2pol_float);
    class_addbang(car2pol_class, car2pol_bang);
}

void cartopol_setup(void){
    car2pol_class = class_new(gensym("cartopol"), (t_newmethod)car2pol_new,
        0, sizeof(t_car2pol), 0, 0);
    class_addfloat(car2pol_class, car2pol_float);
    class_addbang(car2pol_class, car2pol_bang);
    class_sethelpsymbol(car2pol_class, gensym("car2pol"));
}
