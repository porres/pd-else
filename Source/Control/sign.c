// Porres 2026

#include <m_pd.h>
#include <else_alloca.h>
#include <stdlib.h>

static t_class *sign_class;

typedef struct _sign{
	t_object    x_obj;
}t_sign;

static void sign_list(t_sign *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    float in;
    if(ac == 1){
        in = atom_getfloat(av);
        outlet_float(x->x_obj.ob_outlet, (in > 0) - (in < 0));
    }
    else if(ac > 1){
        t_atom* at = ALLOCA(t_atom, ac);
        for(int i = 0; i < ac; i++){
            in = atom_getfloatarg(i, ac, av);
            SETFLOAT(at+i, (in > 0) - (in < 0));
        }
        outlet_list(x->x_obj.ob_outlet, &s_list, ac, at);
        FREEA(at, t_atom, ac);
    }
}

static void *sign_new(void){
    t_sign *x = (t_sign *)pd_new(sign_class);
    outlet_new(&x->x_obj, 0);
    return(x);
}

void sign_setup(void){
	sign_class = class_new(gensym("sign"), (t_newmethod)sign_new, 0,
        sizeof(t_sign), 0, 0);
	class_addlist(sign_class, (t_method)sign_list);
}
