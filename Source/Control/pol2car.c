// porres

#include <math.h>
#include "m_pd.h"

#if defined(_WIN32) || defined(__APPLE__)
/* cf pd/src/x_arithmetic.c */
#define sinf  sin
#define cosf  cos
#endif

typedef struct _pol2car{
    t_object   x_obj;
    t_float    x_amp;
    t_float    x_phase;
    t_outlet  *x_out2;
}t_pol2car;

static t_class *pol2car_class;

static void pol2car_float(t_pol2car *x, t_float f){
    outlet_float(x->x_out2, (x->x_amp = f) * sinf(x->x_phase));
    outlet_float(x->x_obj.ob_outlet, x->x_amp * cosf(x->x_phase));
}

static void pol2car_bang(t_pol2car *x){
    outlet_float(x->x_out2, x->x_amp * sinf(x->x_phase));
    outlet_float(x->x_obj.ob_outlet, x->x_amp * cosf(x->x_phase));
}

static void *pol2car_new(void){
    t_pol2car *x = (t_pol2car *)pd_new(pol2car_class);
    floatinlet_new((t_object *)x, &x->x_phase);
    outlet_new((t_object *)x, &s_float);
    x->x_out2 = outlet_new((t_object *)x, &s_float);
    return(x);
}

void pol2car_setup(void){
    pol2car_class = class_new(gensym("pol2car"), (t_newmethod)pol2car_new,
        0, sizeof(t_pol2car), 0, 0);
    class_addfloat(pol2car_class, pol2car_float);
    class_addbang(pol2car_class, pol2car_bang);
}

void poltocar_setup(void){
    pol2car_class = class_new(gensym("poltocar"), (t_newmethod)pol2car_new,
        0, sizeof(t_pol2car), 0, 0);
    class_addfloat(pol2car_class, pol2car_float);
    class_addbang(pol2car_class, pol2car_bang);
    class_sethelpsymbol(pol2car_class, gensym("pol2car"));
}
