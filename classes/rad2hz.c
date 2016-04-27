// porres 2016

#include "m_pd.h"
#include <math.h>

typedef struct _rad2hz
{
    t_object  x_obj;
    t_float   x_iradps;
    t_outlet  *x_floatout;
    t_float x_f;
} t_rad2hz;

static t_class *rad2hz_class;

static void rad2hz_float(t_rad2hz *x, t_float f)
{
    x->x_f = f;
    outlet_float(x->x_floatout, f * x->x_iradps);
}

void rad2hz_set(t_rad2hz *x, t_float f)
{
    x->x_f = f;
}

static void rad2hz_bang(t_rad2hz *x)
{
    outlet_float(x->x_floatout,  x->x_f * x->x_iradps);
}

static void *rad2hz_new(void)
{
    t_rad2hz *x = (t_rad2hz *)pd_new(rad2hz_class);
    x->x_iradps = sys_getsr() / (2 * M_PI);
    x->x_floatout = outlet_new((t_object *)x, &s_float);
    x->x_f = 0;
    return (x);
}

void rad2hz_setup(void)
{
    rad2hz_class = class_new(gensym("rad2hz"),
        (t_newmethod)rad2hz_new, 0, sizeof(t_rad2hz), CLASS_DEFAULT, 0);
    class_addfloat(rad2hz_class, (t_method)rad2hz_float);
    class_addmethod(rad2hz_class,(t_method)rad2hz_set,gensym("set"),A_DEFFLOAT,0);
    class_addbang(rad2hz_class, rad2hz_bang);
}