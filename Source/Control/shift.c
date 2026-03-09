// based on cyclone/bucket

#include "m_pd.h"

typedef struct _shift{
    t_object    x_obj;
    int         x_nouts;
    t_float    *x_values;
    t_outlet  **x_outs;
    short int   x_rev;
    short int   x_freeze;
}t_shift;

static t_class *shift_class;

static void shift_bang(t_shift *x){
    int i = x->x_nouts;
    while(i--)
        outlet_float(x->x_outs[i], x->x_values[i]);
}

static void shift_float(t_shift *x, t_float val){
    int i;
    if(!x->x_rev){
        for(i = x->x_nouts - 1; i > 0; i--)
            x->x_values[i] = x->x_values[i - 1];
        x->x_values[0] = val;
    }
    else{
        for(i = 0; i < x->x_nouts - 1; i++)
            x->x_values[i] = x->x_values[i + 1];
        x->x_values[x->x_nouts - 1] = val;
    }
    if(!x->x_freeze)
        shift_bang(x);
}

static void shift_shift(t_shift *x){
    if(x->x_rev)
        shift_float(x, x->x_values[0]);
    else
        shift_float(x, x->x_values[x->x_nouts - 1]);
}

static void shift_rev(t_shift *x, t_floatarg f){
    x->x_rev = (int)(f != 0);
}

static void shift_freeze(t_shift *x, t_floatarg f){
    x->x_freeze = (int)(f != 0);
}

static void shift_set(t_shift *x, t_floatarg f){
    int i = x->x_nouts;
    while (i--)
        x->x_values[i] = f;
}

static void shift_free(t_shift *x){
    if(x->x_values)
        freebytes(x->x_values, x->x_nouts * sizeof(*x->x_values));
    if(x->x_outs)
        freebytes(x->x_outs, x->x_nouts * sizeof(*x->x_outs));
}

static void *shift_new(t_floatarg f1, t_floatarg f2){
    t_shift *x;
    int nouts = (int)f1;
    t_outlet **outs;
    if(nouts < 1)
        nouts = 1;
    t_float *bucks = (t_float *)getbytes(nouts * sizeof(*bucks));
    if(!(outs = (t_outlet **)getbytes(nouts * sizeof(*outs)))){
        freebytes(bucks, nouts * sizeof(*bucks));
        return(0);
    }
    x = (t_shift *)pd_new(shift_class);
    x->x_nouts = nouts;
    x->x_values = bucks;
    x->x_outs = outs;
    x->x_rev = (int)(f2 != 0);
    while(nouts--)
        *outs++ = outlet_new((t_object *)x, &s_float);
    return(x);
}

void shift_setup(void){
    shift_class = class_new(gensym("shift"), (t_newmethod)(void*)shift_new,
        (t_method)shift_free, sizeof(t_shift), 0, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addbang(shift_class, shift_bang);
    class_addfloat(shift_class, shift_float);
    class_addmethod(shift_class, (t_method)shift_rev, gensym("rev"), A_FLOAT, 0);
    class_addmethod(shift_class, (t_method)shift_freeze, gensym("freeze"), A_FLOAT, 0);
    class_addmethod(shift_class, (t_method)shift_shift, gensym("shift"), 0);
    class_addmethod(shift_class, (t_method)shift_set, gensym("set"), A_FLOAT, 0);
}
