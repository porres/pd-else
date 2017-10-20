// Porres 2017

#include "m_pd.h"
#include "g_canvas.h"

typedef struct _loadbanger{
    t_object    x_ob;
    int         x_nouts;
//    int         x_banged;
    t_outlet  **x_outs;
    t_outlet   *x_outbuf[1];
} t_loadbanger;

static t_class *loadbanger_class;

/*
static void loadbanger_loadbang(t_loadbanger *x, t_float f){
//    post("f is %d", f);
    if((int)f == LB_INIT){ // LB_INIT is 1
        int i = x->x_nouts;
        while (i--){
            outlet_bang(x->x_outs[i]);
            }
//        x->x_banged = 1;
        };
/*    if((int)f == LB_LOAD && !x->x_banged){ // LB_LOAD is 0
        int j = x->x_nouts;
        while (j--){
            outlet_bang(x->x_outs[j]);
            }
    };
} */


static void loadbanger_loadbang(t_loadbanger *x, t_float f){
    if((int)f == LB_INIT){ // LB_INIT is 1
        int i = x->x_nouts;
        while (i--){
            outlet_bang(x->x_outs[i]);
        }
    }
}

static void loadbanger_bang(t_loadbanger *x){
    int i = x->x_nouts;
    while (i--)
        outlet_bang(x->x_outs[i]);
}

static void loadbanger_anything(t_loadbanger *x, t_symbol *s, int ac, t_atom *av){
    loadbanger_bang(x);
}

static void loadbanger_free(t_loadbanger *x){
    if (x->x_outs != x->x_outbuf)
        freebytes(x->x_outs, x->x_nouts * sizeof(*x->x_outs));
}

static void *loadbanger_new(t_floatarg f){
    t_loadbanger *x;
    int i, nouts = (int)f;
    t_outlet **outs;
//    x->x_banged = 0;
    if (nouts < 1)
        nouts = 1;
    if (nouts > 64)
        nouts = 64;
    if (nouts > 1){
        if (!(outs = (t_outlet **)getbytes(nouts * sizeof(*outs))))
            return (0);
        }
    else
        outs = 0;
    x = (t_loadbanger *)pd_new(loadbanger_class);
    x->x_nouts = nouts;
    x->x_outs = (outs ? outs : x->x_outbuf);
    for (i = 0; i < nouts; i++)
        x->x_outs[i] = outlet_new((t_object *)x, &s_bang);
    return (x);
}

void loadbanger_setup(void){
    loadbanger_class = class_new(gensym("loadbanger"), (t_newmethod)loadbanger_new,
        (t_method)loadbanger_free, sizeof(t_loadbanger), 0, A_DEFFLOAT, 0);
    class_addbang(loadbanger_class, loadbanger_bang);
    class_addanything(loadbanger_class, loadbanger_anything);
    class_addmethod(loadbanger_class, (t_method)loadbanger_loadbang, gensym("loadbang"), A_DEFFLOAT, 0);
}
