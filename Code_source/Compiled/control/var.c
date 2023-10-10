// porres 2023

#include "m_pd.h"

static t_class *var_class;

typedef struct _var{
    t_object    x_obj;
    t_int       x_n;    // number of given vars as arguments
    t_symbol  **x_sym;  // variable names
}t_var;

static void var_bang(t_var *x){
    t_atom at[x->x_n];
    for(int i = 0; i < x->x_n; i++){
        t_float f;
        value_getfloat(x->x_sym[i], &f);
        SETFLOAT(at+i, f);
    }
    outlet_list(x->x_obj.ob_outlet, &s_list, x->x_n, at);
}

static void var_list(t_var *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(!ac)
        var_bang(x);
    else for(int i = 0; i < (ac > x->x_n ? x->x_n : ac); i++)
        value_setfloat(x->x_sym[i], atom_getfloat(av+i));
}

static void var_free(t_var *x){
    for(int i = 0; i < x->x_n; i++)
        value_release(x->x_sym[i]);
    freebytes(x->x_sym, x->x_n * sizeof(*x->x_sym));
}

static void *var_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_var *x = (t_var *)pd_new(var_class);
    x->x_sym = getbytes(sizeof(t_symbol) * (x->x_n = (!ac ? 1 : ac)));
    if(!ac)
        value_get(x->x_sym[0] = &s_);
    else for(int i = 0; i < x->x_n; i++)
        value_get(x->x_sym[i] = atom_getsymbol(av+i));
    outlet_new(&x->x_obj, &s_list);
    return(x);
}

void var_setup(void){
    var_class = class_new(gensym("var"), (t_newmethod)var_new,
        (t_method)var_free, sizeof(t_var), 0, A_GIMME, 0);
    class_addbang(var_class, var_bang);
    class_addlist(var_class, var_list);
}
