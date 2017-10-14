// Porres 2017

#include "m_pd.h"

static t_class *sr2_class;

typedef struct _sr2
{
    t_object x_obj;
    t_float x_sr;
    t_canvas *x_canvas;
} t_sr2;

void *canvas_getblock(t_class *blockclass, t_canvas **canvasp);

static void sr2_bang(t_sr2 *x){
    t_float srate = sys_getsr();
    t_float resample;
/*    t_canvas *canvas = x->x_canvas;
     while (canvas){
     t_block *b = (t_block *)canvas_getblock(block_class, &canvas);
     if (b)
         resample = (t_float)(b->x_upsample) / (t_float)(b->x_downsample);
     } */
    outlet_float(x->x_obj.ob_outlet, resample);
    outlet_float(x->x_obj.ob_outlet, srate);
}

static void *sr2_new(t_symbol *s){
    t_sr2 *x = (t_sr2 *)pd_new(sr2_class);
    outlet_new(&x->x_obj, &s_float);
    outlet_new(&x->x_obj, &s_float);
    x->x_canvas = canvas_getcurrent();
    return (x);
}

void sr2_tilde_setup(void){
    sr2_class = class_new(gensym("sr2~"),
        (t_newmethod)sr2_new, 0, sizeof(t_sr2), 0, 0);
    class_addbang(sr2_class, (t_method)sr2_bang);
}
