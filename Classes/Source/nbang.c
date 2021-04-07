// porres

#include "m_pd.h"

typedef struct _nbang_proxy{
    t_pd            l_pd;
    t_inlet        *p_inlet;
    struct _nbang  *p_owner;
}t_nbang_proxy;

static t_class *nbang_proxy_class;

typedef struct _nbang{
    t_object        x_obj;
    t_nbang_proxy   pxy; //proxy inlet
    int             x_count;
    int             x_n;
    t_outlet       *x_r_bng;
} t_nbang;

static t_class *nbang_class;

// proxy stuff
static void nbang_proxy_init(t_nbang_proxy * p, t_nbang *x){
    p->l_pd = nbang_proxy_class;
    p->p_owner = (void *) x;
}

static void *nbang_proxy_new(void){
    t_nbang_proxy *p = (t_nbang_proxy *)pd_new(nbang_proxy_class);
    return p;
}

static void nbang_proxy_bang(t_nbang_proxy *p){
    t_nbang *x = p->p_owner;
    x->x_count = 0;
}

static void nbang_proxy_float(t_nbang_proxy *p, t_floatarg f){
    t_nbang *x = p->p_owner;
    x->x_count = 0;
    x->x_n = f < 0 ? 0 : (int)f;
}

static void nbang_proxy_setup(void){
    nbang_proxy_class = (t_class *)class_new(gensym("nbang_proxy"), (t_newmethod)nbang_proxy_new,
        0, sizeof(t_nbang_proxy), 0, 0);
    class_addbang(nbang_proxy_class, (t_method)nbang_proxy_bang);
    class_addfloat(nbang_proxy_class, (t_method)nbang_proxy_float);
}

// non proxy stuff
static void nbang_bang(t_nbang *x){
    if(x->x_count < x->x_n){
        x->x_count++;
        outlet_bang(((t_object *)x)->ob_outlet);
    }
    else
        outlet_bang(x->x_r_bng);
}

static void nbang_reset(t_nbang *x){
    x->x_count = 0;
}

static void *nbang_new(t_symbol *s, int argc, t_atom *argv){
    s = NULL;
    t_nbang *x = (t_nbang *)pd_new(nbang_class);
    x->x_n = 1;
    x->x_count = 0;
    int argnum = 0;
    if(argc && argv->a_type == A_FLOAT)
        t_float argval = atom_getfloatarg(0, argc, argv);
        x->x_n = argval < 0 ? 0 : (int)argval;
    nbang_proxy_init(&x->pxy, x);
    inlet_new(&x->x_obj, &x->pxy.l_pd, 0, 0);
    outlet_new((t_object *)x, &s_bang);
    x->x_r_bng = outlet_new((t_object *)x, &s_bang);
    return(x);
}

void nbang_setup(void){
    nbang_class = class_new(gensym("nbang"), (t_newmethod)nbang_new, 0, sizeof(t_nbang),
        0, A_GIMME, 0);
    class_addbang(nbang_class, nbang_bang);
    class_addmethod(nbang_class, (t_method)nbang_reset, gensym("reset"), 0);
    nbang_proxy_setup();
}
