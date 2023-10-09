// porres

#include "m_pd.h"

static t_class *var_class, *vcommon_class;

typedef struct vcommon{
    t_pd c_pd;
    int c_refcount;
    t_float c_f;
}t_vcommon;

typedef struct _var{
    t_object    x_obj;
    t_int       x_n_vars;
    t_symbol  **x_sym;
    t_float   **x_floatstar;
}t_var;

// get a pointer to a named floating-point variable.  The variable
// belongs to a "vcommon" object, which is created if necessary.
t_float *var_get(t_symbol *s){
    t_vcommon *c = (t_vcommon *)pd_findbyclass(s, vcommon_class);
    if(!c){
        c = (t_vcommon *)pd_new(vcommon_class);
        c->c_f = 0;
        c->c_refcount = 0;
        pd_bind(&c->c_pd, s);
    }
    c->c_refcount++;
    return(&c->c_f);
}

// release a variable.  This only frees the "vcommon" resource when the
// last interested party releases it.
void var_release(t_symbol *s){
    t_vcommon *c = (t_vcommon *)pd_findbyclass(s, vcommon_class);
    if(c){
        if(!--c->c_refcount){
            pd_unbind(&c->c_pd, s);
            pd_free(&c->c_pd);
        }
    }
    else
        post("var_release");
}

// var_getfloat -- obtain the float var of a "var" object
// return 0 on success, 1 otherwise
int var_getfloat(t_symbol *s, t_float *f){
    t_vcommon *c = (t_vcommon *)pd_findbyclass(s, vcommon_class);
    if(!c)
        return(1);
    *f = c->c_f;
    return(0);
}

// var_setfloat -- set the float var of a "var" object
// return 0 on success, 1 otherwise
int var_setfloat(t_symbol *s, t_float f){
    t_vcommon *c = (t_vcommon *)pd_findbyclass(s, vcommon_class);
    if(!c)
        return(1);
    c->c_f = f;
    return(0);
}

static void vcommon_float(t_vcommon *x, t_float f){
    x->c_f = f;
}

static void var_bang(t_var *x){
    int n = x->x_n_vars;
    t_atom at[n];
    for(int i = 0; i < n; i++)
        SETFLOAT(at+i, *x->x_floatstar[i]);
    outlet_list(x->x_obj.ob_outlet, &s_list, n, at);
}



static void var_list(t_var *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(!ac)
        var_bang(x);
    else{
        int n = ac > x->x_n_vars ? x->x_n_vars : ac;
//        post("list: n = %d", n);
        for(int i = 0; i < n; i++){
            t_float f = *x->x_floatstar[i] = atom_getfloat(av+i);
            if((x->x_sym[i])->s_thing)
                pd_float((x->x_sym[i])->s_thing, f);
        }
    }
}

// set method
/*static void var_symbol2(t_var *x, t_symbol *s){
    var_release(x->x_sym);
    x->x_sym = s;
    x->x_floatstar = var_get(s);
}*/

static void var_free(t_var *x){
    for(int i = 0; i < x->x_n_vars; i++)
        var_release(x->x_sym[i]);
}

static void *var_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_var *x = (t_var *)pd_new(var_class);
    x->x_n_vars = (ac ? ac : 1);
//    post("x->x_n_vars = %d", x->x_n_vars);
    x->x_sym = getbytes(sizeof(t_symbol) * x->x_n_vars);
    x->x_floatstar = getbytes(sizeof(float) * x->x_n_vars);
/*    if(!*s->s_name)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, gensym("symbol"), gensym("symbol2"));*/
    if(ac){
        for(int i = 0; i < x->x_n_vars; i++){
            x->x_sym[i] = atom_getsymbol(av+i);
            x->x_floatstar[i] = var_get(x->x_sym[i]);
        }
    }
    outlet_new(&x->x_obj, &s_list);
    return(x);
}

void var_setup(void){
    var_class = class_new(gensym("var"), (t_newmethod)var_new,
        (t_method)var_free, sizeof(t_var), 0, A_GIMME, 0);
    class_addlist(var_class, var_list);
//    class_addmethod(var_class, (t_method)var_symbol2, gensym("symbol2"), A_DEFSYM, 0);
    vcommon_class = class_new(gensym("var"), 0, 0, sizeof(t_vcommon), CLASS_PD, 0);
    class_addfloat(vcommon_class, vcommon_float);
}
