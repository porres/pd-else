// porres

#include "m_pd.h"

static t_class *var_class, *vcommon_class;

typedef struct vcommon{
    t_pd    c_pd;
    int     c_refcount;
    t_float c_f;
}t_vcommon;

typedef struct _var{
    t_object    x_obj;
    t_int       x_n_vars;
    t_symbol  **x_sym;
    t_float   **x_floatstar;
}t_var;

// get pointer to a named variable, which belongs to a "vcommon" object
t_float *var_get(t_symbol *s){
    t_vcommon *c = (t_vcommon *)pd_findbyclass(s, vcommon_class);
    if(!c){ // create named variable because it doesn't exist yet
        c = (t_vcommon *)pd_new(vcommon_class);
        c->c_f = 0;
        c->c_refcount = 0;
        pd_bind(&c->c_pd, s);
    }
    c->c_refcount++;
    return(&c->c_f);
}

// release a variable. Only frees "vcommon" resource when last one releases it.
void var_release(t_symbol *s){
    t_vcommon *c = (t_vcommon *)pd_findbyclass(s, vcommon_class);
    if(c){
        if(!--c->c_refcount){
            pd_unbind(&c->c_pd, s);
            pd_free(&c->c_pd);
        }
    }
    else
        post("var release bug, releaing");
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
        for(int i = 0; i < n; i++){
            t_float f = *x->x_floatstar[i] = atom_getfloat(av+i);
            if((x->x_sym[i])->s_thing)
                pd_float((x->x_sym[i])->s_thing, f);
        }
    }
}

static void var_free(t_var *x){
    for(int i = 0; i < x->x_n_vars; i++)
        var_release(x->x_sym[i]);
}

static void *var_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_var *x = (t_var *)pd_new(var_class);
    x->x_n_vars = (ac ? ac : 1);
    x->x_sym = getbytes(sizeof(t_symbol) * x->x_n_vars);
    x->x_floatstar = getbytes(sizeof(float) * x->x_n_vars);
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
    vcommon_class = class_new(gensym("var"), 0, 0, sizeof(t_vcommon), CLASS_PD, 0);
    class_addfloat(vcommon_class, vcommon_float); // why???
}
