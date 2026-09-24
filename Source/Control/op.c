// porres 2026

#include <m_pd.h>
#include <math.h>
#include <stdint.h>

static t_class *op_class;
static t_class *opproxy_class;

typedef struct _op{
    t_object    x_obj;
    t_outlet   *x_outlet;
    int         x_op;
    t_float    *x_val_list;         // right-operand values
    int         x_val_size;         // always >= 1 ("no args" -> single value 0)
    t_atom     *x_out_scratch;      // persistent output buffer, grown as needed
    int         x_out_scratch_size;
    void       *x_proxy;            // right inlet receiver, actually a t_opproxy*
}t_op;

typedef struct _opproxy{
    t_pd        p_pd;
    t_op       *p_owner;
}t_opproxy;

static t_float op_compute(int op, t_float f1, t_float f2){
    switch(op){
        case 0: return(f1 < f2);
        case 1: return(f1 > f2);
        case 2: return(f1 <= f2);
        case 3: return(f1 >= f2);
        case 4: return(f1 != f2);
        case 5: return(f1 == f2);
        case 6: return(f1 && f2);
        case 7: return(f1 || f2);
        case 8: return(!f1);
        case 9: return((t_float)((int32_t)f1 & (int32_t)f2));
        case 10: return((t_float)((int32_t)f1 | (int32_t)f2));
        case 11: return((t_float)(~(int32_t)f1));
        case 12: return((t_float)((int32_t)f1 ^ (int32_t)f2));
        case 13: return((t_float)((int32_t)f1 << (int32_t)f2));
        case 14: return((t_float)((int32_t)f1 >> (int32_t)f2));
        case 15: return(f2 == 0 ? 0. : fmod(f1, f2));
        case 16: return(f1 * f2);
        case 17: return(f1 / f2);
        case 18: return(f1 + f2);
        case 19: return(f1 - f2);
        case 20: return(f2 - f1);
        case 21: return(f2 / f1);
        case 22: return(f1 < f2 ? f1 : f2);
        case 23: return(f1 > f2 ? f1 : f2);
        case 24: return(pow(f1, f2));
        case 25: return(log(f1) / log(f2));
        default: return(0);
    }
}

static void op_dooutput(t_op *x, int n1, t_atom *av){
    int n2 = x->x_val_size;
    t_float *val = x->x_val_list;
    int op = x->x_op;
    if(n1 == 1 && n2 == 1){ // skip list bullshit
        outlet_float(x->x_outlet, op_compute(op, atom_getfloat(av), val[0]));
        return;
    }
    int outsize = n1 > n2 ? n1 : n2;
    if(x->x_out_scratch_size < outsize){
        x->x_out_scratch = (t_atom *)resizebytes(x->x_out_scratch,
            x->x_out_scratch_size * sizeof(t_atom), outsize * sizeof(t_atom));
        x->x_out_scratch_size = outsize;
    }
    t_atom *out = x->x_out_scratch;
    if(n1 == n2){ // equal sizes: no wraparound needed
        for(int i = 0; i < outsize; i++)
            SETFLOAT(out+i, op_compute(op, atom_getfloat(av+i), val[i]));
    }
    else if(n1 == 1){ // scalar left broadcast across a right-hand list
        t_float f1 = atom_getfloat(av);
        for(int i = 0; i < outsize; i++)
            SETFLOAT(out+i, op_compute(op, f1, val[i]));
    }
    else if(n2 == 1){ // list left, scalar right operand
        t_float f2 = val[0];
        for(int i = 0; i < outsize; i++)
            SETFLOAT(out+i, op_compute(op, atom_getfloat(av+i), f2));
    }
    else{ // genuine mismatch: cycle the shorter against the longer
        for(int i = 0; i < outsize; i++)
            SETFLOAT(out+i, op_compute(op, atom_getfloat(av + i % n1), val[i % n2]));
    }
    outlet_list(x->x_outlet, &s_list, outsize, out);
}

static void op_list(t_op *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(ac < 1)
        return;
    for(int i = 0; i < ac; i++)
        if(av[i].a_type != A_FLOAT){
            pd_error(x, "[op]: list must contain only floats");
            return;
        }
    op_dooutput(x, ac, av);
}

static void op_float(t_op *x, t_float f){
    t_atom a;
    SETFLOAT(&a, f);
    op_dooutput(x, 1, &a);
}

static void op_lt(t_op *x){ x->x_op = 0; }
static void op_gt(t_op *x){ x->x_op = 1; }
static void op_le(t_op *x){ x->x_op = 2; }
static void op_ge(t_op *x){ x->x_op = 3; }
static void op_ne(t_op *x){ x->x_op = 4; }
static void op_eq(t_op *x){ x->x_op = 5; }
static void op_and(t_op *x){ x->x_op = 6; }
static void op_or(t_op *x){ x->x_op = 7; }
static void op_not(t_op *x){ x->x_op = 8; }
static void op_bitand(t_op *x){ x->x_op = 9; }
static void op_bitor(t_op *x){ x->x_op = 10; }
static void op_bitnot(t_op *x){ x->x_op = 11; }
static void op_bitxor(t_op *x){ x->x_op = 12; }
static void op_bitshift_l(t_op *x){ x->x_op = 13; }
static void op_bitshift_r(t_op *x){ x->x_op = 14; }
static void op_mod(t_op *x){ x->x_op = 15; }
static void op_mul(t_op *x){ x->x_op = 16; }
static void op_div(t_op *x){ x->x_op = 17; }
static void op_plus(t_op *x){ x->x_op = 18; }
static void op_minus(t_op *x){ x->x_op = 19; }
static void op_rmin(t_op *x){ x->x_op = 20; }
static void op_rdiv(t_op *x){ x->x_op = 21; }
static void op_min(t_op *x){ x->x_op = 22; }
static void op_max(t_op *x){ x->x_op = 23; }
static void op_pow(t_op *x){ x->x_op = 24; }
static void op_log(t_op *x){ x->x_op = 25; }

static void op_setright(t_op *x, int ac, t_atom *av){
    if(x->x_val_size != ac){
        x->x_val_list = (t_float *)resizebytes(x->x_val_list,
            x->x_val_size * sizeof(t_float), ac * sizeof(t_float));
        x->x_val_size = ac;
    }
    for(int i = 0; i < ac; i++)
        x->x_val_list[i] = atom_getfloat(av+i);
}

static void opproxy_float(t_opproxy *p, t_float f){
    t_atom a;
    SETFLOAT(&a, f);
    op_setright(p->p_owner, 1, &a);
}

static void opproxy_list(t_opproxy *p, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(ac < 1)
        return;
    for(int i = 0; i < ac; i++)
        if(av[i].a_type != A_FLOAT){
            pd_error(p->p_owner, "[op]: list must contain only floats");
            return;
        }
    op_setright(p->p_owner, ac, av);
}

static void *op_new(t_symbol *s, int ac, t_atom *av){
    t_op *x = (t_op *)pd_new(op_class);
    x->x_op = 1; // default ">"
    x->x_val_list = NULL;
    x->x_val_size = 0;
    x->x_out_scratch = NULL;
    x->x_out_scratch_size = 0;
    if(ac && av->a_type == A_SYMBOL){
        s = atom_getsymbol(av);
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
        else if(s == gensym("!"))
            x->x_op = 8;
        else if(s == gensym("&"))
            x->x_op = 9;
        else if(s == gensym("|"))
            x->x_op = 10;
        else if(s == gensym("~"))
            x->x_op = 11;
        else if(s == gensym("^"))
            x->x_op = 12;
        else if(s == gensym("<<"))
            x->x_op = 13;
        else if(s == gensym(">>"))
            x->x_op = 14;
        else if(s == gensym("%"))
            x->x_op = 15;
        else if(s == gensym("*"))
            x->x_op = 16;
        else if(s == gensym("/"))
            x->x_op = 17;
        else if(s == gensym("+"))
            x->x_op = 18;
        else if(s == gensym("-"))
            x->x_op = 19;
        else if(s == gensym("!-"))
            x->x_op = 20;
        else if(s == gensym("!/"))
            x->x_op = 21;
        else if(s == gensym("min"))
            x->x_op = 22;
        else if(s == gensym("max"))
            x->x_op = 23;
        else if(s == gensym("pow"))
            x->x_op = 24;
        else if(s == gensym("log"))
            x->x_op = 25;
        else
            goto errstate;
        ac--, av++;
    }
    else if(ac && av->a_type != A_FLOAT)
        goto errstate;

    if(ac){
        for(int i = 0; i < ac; i++)
            if(av[i].a_type != A_FLOAT)
                goto errstate;
        x->x_val_list = (t_float *)getbytes(ac * sizeof(t_float));
        for(int i = 0; i < ac; i++)
            x->x_val_list[i] = atom_getfloat(av+i);
        x->x_val_size = ac;
    }
    else{ // "no args": single value 0, matches op~'s default
        x->x_val_list = (t_float *)getbytes(sizeof(t_float));
        x->x_val_list[0] = 0.;
        x->x_val_size = 1;
    }
    {
        t_opproxy *proxy = (t_opproxy *)getbytes(sizeof(t_opproxy));
        proxy->p_pd = opproxy_class;
        proxy->p_owner = x;
        inlet_new((t_object *)x, (t_pd *)proxy, 0, 0);
        x->x_proxy = proxy; // stashed so op_free can reclaim it
    }
    x->x_outlet = outlet_new(&x->x_obj, &s_list);
    return(x);
errstate:
    pd_error(x, "[op]: improper args");
    return(NULL);
}

static void *op_free(t_op *x){
    if(x->x_val_list)
        freebytes(x->x_val_list, x->x_val_size * sizeof(t_float));
    if(x->x_out_scratch)
        freebytes(x->x_out_scratch, x->x_out_scratch_size * sizeof(t_atom));
    if(x->x_proxy)
        freebytes(x->x_proxy, sizeof(t_opproxy));
    return(void *)x;
}

void op_setup(void){
    op_class = class_new(gensym("op"), (t_newmethod)op_new, (t_method)op_free,
        sizeof(t_op), CLASS_DEFAULT, A_GIMME, 0);
    class_addlist(op_class, op_list);
    class_addfloat(op_class, op_float);
    class_addmethod(op_class, (t_method)op_lt, gensym("<"), 0);
    class_addmethod(op_class, (t_method)op_gt, gensym(">"), 0);
    class_addmethod(op_class, (t_method)op_le, gensym("<="), 0);
    class_addmethod(op_class, (t_method)op_ge, gensym(">="), 0);
    class_addmethod(op_class, (t_method)op_ne, gensym("!="), 0);
    class_addmethod(op_class, (t_method)op_eq, gensym("=="), 0);
    class_addmethod(op_class, (t_method)op_and, gensym("&&"), 0);
    class_addmethod(op_class, (t_method)op_or, gensym("||"), 0);
    class_addmethod(op_class, (t_method)op_not, gensym("!"), 0);
    class_addmethod(op_class, (t_method)op_bitand, gensym("&"), 0);
    class_addmethod(op_class, (t_method)op_bitor, gensym("|"), 0);
    class_addmethod(op_class, (t_method)op_bitnot, gensym("~"), 0);
    class_addmethod(op_class, (t_method)op_bitxor, gensym("^"), 0);
    class_addmethod(op_class, (t_method)op_bitshift_l, gensym("<<"), 0);
    class_addmethod(op_class, (t_method)op_bitshift_r, gensym(">>"), 0);
    class_addmethod(op_class, (t_method)op_mod, gensym("%"), 0);
    class_addmethod(op_class, (t_method)op_mul, gensym("*"), 0);
    class_addmethod(op_class, (t_method)op_div, gensym("/"), 0);
    class_addmethod(op_class, (t_method)op_plus, gensym("+"), 0);
    class_addmethod(op_class, (t_method)op_minus, gensym("-"), 0);
    class_addmethod(op_class, (t_method)op_rmin, gensym("!-"), 0);
    class_addmethod(op_class, (t_method)op_rdiv, gensym("!/"), 0);
    class_addmethod(op_class, (t_method)op_min, gensym("min"), 0);
    class_addmethod(op_class, (t_method)op_max, gensym("max"), 0);
    class_addmethod(op_class, (t_method)op_pow, gensym("pow"), 0);
    class_addmethod(op_class, (t_method)op_log, gensym("log"), 0);
    opproxy_class = class_new(gensym("op proxy"), 0, 0, sizeof(t_opproxy),
        CLASS_PD | CLASS_NOINLET, 0);
    class_addlist(opproxy_class, opproxy_list);
    class_addfloat(opproxy_class, opproxy_float);
}
