// porrres 2019

#include "m_pd.h"

static t_class  *op_class;

typedef struct _op{
    t_object    x_obj;
    t_inlet    *x_inlet_v;
    int         x_op;
}t_op;

typedef unsigned long shared_t_bitmask;

int32_t bitwise_getbitmask(int ac, t_atom *av){
    int32_t result = 0;
    if(sizeof(shared_t_bitmask) >= sizeof(int32_t)){
        int32_t nbits = sizeof(int32_t) * 8;
        if (ac > nbits)
            ac = nbits;
        while(ac--){
            if(av->a_type == A_FLOAT &&
                (int)av->a_w.w_float)  /* CHECKED */
                result += 1 << ac;
            av++;
        }
    }
    return(result);
}

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
            case 6: // and
                *out++ = *in1++ && *in2++;
                break;
            case 7: // or
                *out++ = *in1++ || *in2++;
                break;
            case 8: // bitand
                *out++ = (t_float)(((int32_t)*in1++) & ((int32_t)*in2++));
                break;
            case 9: // bitor
                *out++ = (t_float)(((int32_t)*in1++) | ((int32_t)*in2++));
                break;
            case 10: // bitnot
                *out++ = (t_float)(~((int32_t)*in1++));
                break;
            case 11: // bitexclusive
                *out++ = (t_float)(((int32_t)*in1++) ^ ((int32_t)*in2++));
                break;
            case 12: // bit shift left
                *out++ = (t_float)(((int32_t)*in1++) << ((int32_t)*in2++));
                break;
            case 13: // bit shift right
                *out++ = (t_float)(((int32_t)*in1++) >> ((int32_t)*in2++));
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

static void op_and(t_op *x){
    x->x_op = 6;
}

static void op_or(t_op *x){
    x->x_op = 7;
}

static void op_bitand(t_op *x){
    x->x_op = 8;
}

static void op_bitor(t_op *x){
    x->x_op = 9;
}


static void op_bitnot(t_op *x){
    x->x_op = 10;
}

static void op_bitxor(t_op *x){
    x->x_op = 11;
}

static void op_bitshift_l(t_op *x){
    x->x_op = 12;
}

static void op_bitshift_r(t_op *x){
    x->x_op = 13;
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
            else if(s == gensym("&&"))
                x->x_op = 6;
            else if(s == gensym("||"))
                x->x_op = 7;
            else if(s == gensym("&"))
                x->x_op = 8;
            else if(s == gensym("|"))
                x->x_op = 9;
            else if(s == gensym("~"))
                x->x_op = 10;
            else if(s == gensym("^"))
                x->x_op = 11;
            else if(s == gensym("<<"))
                x->x_op = 12;
            else if(s == gensym(">>"))
                x->x_op = 13;
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
    class_addmethod(op_class, (t_method)op_and, gensym("&&"), 0);
    class_addmethod(op_class, (t_method)op_or, gensym("||"), 0);
    class_addmethod(op_class, (t_method)op_bitand, gensym("&"), 0);
    class_addmethod(op_class, (t_method)op_bitor, gensym("|"), 0);
    class_addmethod(op_class, (t_method)op_bitnot, gensym("~"), 0);
    class_addmethod(op_class, (t_method)op_bitxor, gensym("^"), 0);
    class_addmethod(op_class, (t_method)op_bitshift_l, gensym("<<"), 0);
    class_addmethod(op_class, (t_method)op_bitshift_r, gensym(">>"), 0);
}
