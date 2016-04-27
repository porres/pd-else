// porres 2016

#include "m_pd.h"
#include <math.h>

typedef struct _hz2rad
{
    t_object  x_obj;
    t_float   x_radps;
    t_outlet  *x_floatout;
    t_float x_f;
} t_hz2rad;

static t_class *hz2rad_class;

static void hz2rad_float(t_hz2rad *x, t_float f)
{
    x->x_f = f;
    outlet_float(x->x_floatout, f * x->x_radps);
}

void hz2rad_set(t_hz2rad *x, t_float f)
{
    x->x_f = f;
}

static void hz2rad_bang(t_hz2rad *x)
{
    outlet_float(x->x_floatout,  x->x_f * x->x_radps);
}

static void *hz2rad_new(void)
{
    t_hz2rad *x = (t_hz2rad *)pd_new(hz2rad_class);
    x->x_radps = 2 * M_PI / sys_getsr();
    x->x_floatout = outlet_new((t_object *)x, &s_float);
    x->x_f = 0;
    return (x);
}

void hz2rad_setup(void)
{
    hz2rad_class = class_new(gensym("hz2rad"),
        (t_newmethod)hz2rad_new, 0, sizeof(t_hz2rad), CLASS_DEFAULT, 0);
    class_addfloat(hz2rad_class, (t_method)hz2rad_float);
    class_addmethod(hz2rad_class,(t_method)hz2rad_set,gensym("set"),A_DEFFLOAT,0);
    class_addbang(hz2rad_class, hz2rad_bang);
}