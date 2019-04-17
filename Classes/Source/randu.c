
#include "m_pd.h"
#include "random.h"

#define SIZE    65536

typedef struct _randu{
    t_object         x_ob;
    int              x_count;
    int              x_size;   /* as allocated (in bytes) */
    int              x_range;  /* as used */
    unsigned short  *x_randu;
    unsigned short   x_randuini[SIZE];
    unsigned int     x_seed;
    t_outlet        *x_bangout;
}t_randu;

static t_class *randu_class;

static int randu_resize(t_randu *x, t_float f){
    int range = (int)f;
    x->x_range = range < 1 ? 1 : range > SIZE ? SIZE : range;
    return (1);
}

static void randu_bang(t_randu *x){
    if(x->x_count){
        int ndx = rand_int(&x->x_seed, x->x_count);
        unsigned short pick = x->x_randu[ndx];
        x->x_randu[ndx] = x->x_randu[--x->x_count];
        outlet_float(((t_object *)x)->ob_outlet, pick);
    }
    else{
        outlet_bang(x->x_bangout);
        x->x_count = x->x_range;
        for(int i = 0; i < x->x_count; i++)
            x->x_randu[i] = i;
        int ndx = rand_int(&x->x_seed, x->x_count);
        unsigned short pick = x->x_randu[ndx];
        x->x_randu[ndx] = x->x_randu[--x->x_count];
        outlet_float(((t_object *)x)->ob_outlet, pick);
    }
}

static void randu_restart(t_randu *x){
    x->x_count = x->x_range;
    for(int i = 0; i < x->x_count; i++)
        x->x_randu[i] = i;
}

static void randu_ft1(t_randu *x, t_floatarg f){
    if(randu_resize(x, f))
        randu_restart(x);
}

static void randu_seed(t_randu *x, t_floatarg f){
    unsigned int i = f < 1 ? 1 : (unsigned int)f;
    rand_seed(&x->x_seed, i);
}

static void randu_free(t_randu *x){
    if(x->x_randu != x->x_randuini)
        freebytes(x->x_randu, x->x_size * sizeof(*x->x_randu));
}

static void *randu_new(t_floatarg f1, t_floatarg f2){
    t_randu *x = (t_randu *)pd_new(randu_class);
    x->x_size = SIZE;
    x->x_randu = x->x_randuini;
    if(f1 < 1)
        f1 = 1;
    if(f2 > SIZE)
        f2 = SIZE;
    randu_resize(x, f1);
    randu_seed(x, f2);  /* CHECKME */
    inlet_new((t_object *)x, (t_pd *)x, &s_float, gensym("ft1"));
    outlet_new((t_object *)x, &s_float);
    x->x_bangout = outlet_new((t_object *)x, &s_bang);
    randu_restart(x);
    return (x);
}

void randu_setup(void){
    randu_class = class_new(gensym("randu"), (t_newmethod)randu_new, (t_method)randu_free,
        sizeof(t_randu), 0, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addbang(randu_class, randu_bang);
    class_addmethod(randu_class, (t_method)randu_ft1, gensym("ft1"), A_FLOAT, 0);
    class_addmethod(randu_class, (t_method)randu_seed, gensym("seed"), A_FLOAT, 0);
    class_addmethod(randu_class, (t_method)randu_restart, gensym("restart"), 0);
}
