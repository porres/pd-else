// porres 2023

#include "m_pd.h"

static t_class *var_class, *varcommon_class;

typedef struct varcommon{
    t_pd    c_pd;
    int     c_refcount;
    float   c_f;
}t_varcommon;

typedef struct _var{
    t_object    x_obj;
    t_int       x_n_vars;
    t_symbol  **x_sym;
    float      *x_fval;
}t_var;

static void varcommon_float(t_varcommon *x, t_float f){
    x->c_f = f;
}

static void var_bang(t_var *x){
    t_atom at[x->x_n_vars];
    for(int i = 0; i < x->x_n_vars; i++)
        SETFLOAT(at+i, x->x_fval[i]);
    outlet_list(x->x_obj.ob_outlet, &s_list, x->x_n_vars, at);
}

static void var_list(t_var *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(!ac)
        var_bang(x);
    else for(int i = 0; i < (ac > x->x_n_vars ? x->x_n_vars : ac); i++)
        x->x_fval[i] = atom_getfloat(av+i);
}

static void var_free(t_var *x){
    freebytes(x->x_fval, x->x_n_vars * sizeof(*x->x_fval));
    for(int i = 0; i < x->x_n_vars; i++){
        post("[var] freeing %s", x->x_sym[i]->s_name);
        t_varcommon *c = (t_varcommon *)pd_findbyclass(x->x_sym[i], varcommon_class);
        if(c){
            post("[var] free, found %s", x->x_sym[i]->s_name);
            if(!--c->c_refcount){ // free variable when last one releases it
            post("[var] freeing last %s", x->x_sym[i]->s_name);
                pd_unbind(&c->c_pd, x->x_sym[i]);
                pd_free(&c->c_pd);
            }
        }
        else
            post("[var] free bug, releasing %s, which was not set", x->x_sym[i]->s_name);
    }
}

t_float *var_get(t_symbol *s){ // get [var] pointer that belongs to "varcommon"
    post("[var] var_get %s", s->s_name);
    t_varcommon *c = (t_varcommon *)pd_findbyclass(s, varcommon_class);
    if(!c){ // create named variable because it doesn't exist yet
        post("[var] creating inexistent named variable %s", s->s_name);
        c = (t_varcommon *)pd_new(varcommon_class);
        c->c_f = 0;
        c->c_refcount = 0;
        pd_bind(&c->c_pd, s);
    }
    c->c_refcount++;
    return(&c->c_f);
}

static void *var_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_var *x = (t_var *)pd_new(var_class);
    x->x_n_vars = (!ac ? 1 : ac);
    x->x_sym = getbytes(sizeof(t_symbol) * x->x_n_vars);
    x->x_fval = getbytes(sizeof(float) * x->x_n_vars);
    if(!ac)
        x->x_fval[0] = var_get(x->x_sym[0] = &s_);
    else for(int i = 0; i < x->x_n_vars; i++){
        x->x_fval[i] = var_get(x->x_sym[i] = atom_getsymbol(av+i));
        post("[var] new --> %s  = %g", x->x_sym[i]->s_name, x->x_fval[i]);
    }
    outlet_new(&x->x_obj, &s_list);
    return(x);
}

void var_setup(void){
    var_class = class_new(gensym("var"), (t_newmethod)var_new,
        (t_method)var_free, sizeof(t_var), 0, A_GIMME, 0);
    class_addlist(var_class, var_list);
    varcommon_class = class_new(gensym("var"), 0, 0, sizeof(t_varcommon), CLASS_PD, 0);
    class_addfloat(varcommon_class, varcommon_float); // why???
}
