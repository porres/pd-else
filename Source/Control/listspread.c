
#include "m_pd.h"

#define listspread_MINOUTS  1
#define listspread_DEFOUTS  2

typedef struct _listspread{
    t_object    x_obj;
    int         x_nouts;
    t_outlet  **x_outs;
}t_listspread;

static t_class *listspread_class;

static void listspread_list(t_listspread *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(!ac)
        return;
    if(av->a_type == A_SYMBOL){
        pd_error(x, "[listspread]: 1st list element cannot be a symbol");
        return;
    }
    int ndx = (int)av->a_w.w_float;
    if(ndx < 1)
        ndx = 1;
    if(ndx > x->x_nouts)
        ndx = x->x_nouts;
    ndx--;
    t_atom *argp;
    t_outlet **outp;
    if(ac == 1){
        outp = x->x_outs + ndx;
        outlet_bang(*outp);
    }
    else{
        int last = ac - 1 + ndx; // ndx of last outlet filled (first is 1)
        if(last > x->x_nouts){
            argp = av + 1 + x->x_nouts - ndx;
            outp = x->x_outs + x->x_nouts;
        }
        else{
            argp = av + ac;
            outp = x->x_outs + last;
        }
        // argp/outp now point to one after the first atom/outlet to deliver
        for(argp--, outp--; argp > av; argp--, outp--){
            if(argp->a_type == A_FLOAT)
                outlet_float(*outp, argp->a_w.w_float);
            else if(argp->a_type == A_SYMBOL)
                outlet_symbol(*outp, argp->a_w.w_symbol);
        }
    }
}

static void listspread_free(t_listspread *x){
    if(x->x_outs)
        freebytes(x->x_outs, x->x_nouts * sizeof(*x->x_outs));
}

static void *listspread_new(t_floatarg f){
    t_listspread *x  = (t_listspread *)pd_new(listspread_class);
    int i, nouts = (int)f;
    t_outlet **outs;
    if(nouts < listspread_MINOUTS)
        nouts = listspread_DEFOUTS;
    if(!(outs = (t_outlet **)getbytes(nouts * sizeof(*outs))))
        return(0);
    x = (t_listspread *)pd_new(listspread_class);
    x->x_nouts = nouts;
    x->x_outs = outs;
    for(i = 0; i < nouts; i++)
        x->x_outs[i] = outlet_new((t_object *)x, &s_anything);
    return(x);
}

void listspread_setup(void){
    listspread_class = class_new(gensym("listspread"), (t_newmethod)(void*)listspread_new,
        (t_method)listspread_free, sizeof(t_listspread), 0, A_DEFFLOAT, 0);
    class_addlist(listspread_class, listspread_list);
}
