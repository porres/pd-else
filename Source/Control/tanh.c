// Porres 2018

#include <m_pd.h>
#include <else_alloca.h>
#include <math.h>
#include <stdlib.h>

static t_class *tanh_class;

typedef struct _tanh{
    t_object    x_obj;
    float       x_value;
}t_tanh;

static void tanh_list(t_tanh *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(ac == 1)
        outlet_float(x->x_obj.ob_outlet, x->x_value = tanhf(atom_getfloat(av)));
    else if(ac > 1){
        t_atom* at = ALLOCA(t_atom, ac);
        
        for(int i = 0; i < ac; i++)
            SETFLOAT(at+i, tanhf(atom_getfloatarg(i, ac, av)));
        outlet_list(x->x_obj.ob_outlet, &s_list, ac, at);
        FREEA(at, t_atom, ac);
    }
}

static void tanh_bang(t_tanh *x){
    outlet_float(x->x_obj.ob_outlet, x->x_value);
}

static void *tanh_new(t_floatarg f){
    t_tanh *x = (t_tanh *)pd_new(tanh_class);
    x->x_value = tanhf(f);
    outlet_new(&x->x_obj, 0);
    return(x);
}

void tanh_setup(void){
    tanh_class = class_new(gensym("tanh"), (t_newmethod)tanh_new, 0,
        sizeof(t_tanh), 0, A_DEFFLOAT, 0);
    class_addbang(tanh_class, (t_method)tanh_bang);
    class_addlist(tanh_class, (t_method)tanh_list);
}
