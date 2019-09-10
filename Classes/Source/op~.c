// porrres 2019

#include "m_pd.h"

static t_class  *op_class;

typedef struct _op{
    t_object    x_obj;
    t_inlet    *x_inlet_v;
    int         x_op;
}t_op;

static t_int *op_perform(t_int *w){
    t_op *x = (t_op *)(w[1]);
    t_sample *in1 = (t_sample *)(w[2]);
    t_sample *in2 = (t_sample *)(w[3]);
    t_sample *out = (t_sample *)(w[4]);
    int n = (int)(w[5]);
    while(n--){
        switch(x->x_op){
            case 0: // lt
                *out++ = *in1++ < *in2++;
                break;
            case 1: // gt
                *out++ = *in1++ > *in2++;
                break;
            case 2: // le
                *out++ = *in1++ <= *in2++;
                break;
            case 3: // ge
                *out++ = *in1++ >= *in2++;
                break;
            case 4: // ne
                *out++ = *in1++ != *in2++;
                break;
            case 5: // eq
                *out++ = *in1++ == *in2++;
                break;
        }
    }
    return(w+6);
}

static void op_lt(t_op *x){
    x->x_op = 0;
}

static void op_gt(t_op *x){
    x->x_op = 1;
}

static void op_le(t_op *x){
    x->x_op = 2;
}

static void op_ge(t_op *x){
    x->x_op = 3;
}

static void op_ne(t_op *x){
    x->x_op = 4;
}

static void op_eq(t_op *x){
    x->x_op = 5;
}

static void op_dsp(t_op *x, t_signal **sp){
    dsp_add(op_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[0]->s_n);
}

static void *op_new(t_symbol *s, int ac, t_atom *av){
    t_op *x = (t_op *)pd_new(op_class);
    x->x_op = 1; // default greater than
    float v = 0;
    if(ac > 2)
        goto errstate;
    else if(ac){
            s = atom_getsymbolarg(0, ac, av);
            if(s == gensym("<"))
                x->x_op = 0;
            else if(s == gensym(">"))
                x->x_op = 1;
            else if(s == gensym("<="))
                x->x_op = 2;
            else if(s == gensym(">="))
                x->x_op = 3;
            else if(s == gensym("!="))
                x->x_op = 4;
            else if(s == gensym("=="))
                x->x_op = 5;
            else
                goto errstate;
            v = atom_getfloatarg(1, ac, av);
    }
    x->x_inlet_v = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_v, v);
    outlet_new(&x->x_obj, &s_signal);
    return(x);
errstate:
    pd_error(x, "[op~]: improper args");
    return NULL;
}

void op_tilde_setup(void){
    op_class = class_new(gensym("op~"), (t_newmethod)op_new, 0,
        sizeof(t_op), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(op_class, nullfn, gensym("signal"), 0);
    class_addmethod(op_class, (t_method)op_dsp, gensym("dsp"), 0);
    class_addmethod(op_class, (t_method)op_lt, gensym("<"), 0);
    class_addmethod(op_class, (t_method)op_gt, gensym(">"), 0);
    class_addmethod(op_class, (t_method)op_le, gensym("<="), 0);
    class_addmethod(op_class, (t_method)op_ge, gensym(">="), 0);
    class_addmethod(op_class, (t_method)op_ne, gensym("!="), 0);
    class_addmethod(op_class, (t_method)op_eq, gensym("=="), 0);
}
