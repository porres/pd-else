// porrres 2019

#include <m_pd.h>
#include "magic.h"
#include "math.h"

static t_class  *op_class;

typedef struct _op{
    t_object    x_obj;
    t_inlet    *x_inlet_v;
    int         x_op;
    t_float    *x_val_list;        // right-operand values, from creation args
    int         x_val_size;        // how many were given (0 = none -> default 0)
    t_float    *x_val_scratch;     // persistent per-channel floats fed to dsp_add
    int         x_val_scratch_size;
    t_float    *x_signalscalar;
    t_glist    *x_glist;
}t_op;

// shared by both perform routines so the 22 cases only exist once
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

static t_int *op_perform(t_int *w){
    t_op *x = (t_op *)(w[1]);
    t_sample *in1 = (t_sample *)(w[2]);
    t_sample *in2 = (t_sample *)(w[3]);
    t_sample *out = (t_sample *)(w[4]);
    int n = (int)(w[5]);
    while(n--){
        t_float f1 = *in1++, f2 = *in2++;
        *out++ = op_compute(x->x_op, f1, f2);
    }
    return(w+6);
}

static t_int *op_perform_checkscalar(t_int *w){
    t_op *x = (t_op *)(w[1]);
    if(!else_magic_isnan(*x->x_signalscalar)){
        t_float val = *x->x_signalscalar;
        for(int i = 0; i < x->x_val_scratch_size; i++)
            x->x_val_scratch[i] = val;
        else_magic_setnan(x->x_signalscalar);
    }
    return(w+2);
}

static t_int *op_perform_scalar(t_int *w){
    t_op *x = (t_op *)(w[1]);
    t_sample *in1 = (t_sample *)(w[2]);
    t_float f2 = *(t_float *)(w[3]);
    t_sample *out = (t_sample *)(w[4]);
    int n = (int)(w[5]);
    while(n--){
        t_float f1 = *in1++;
        *out++ = op_compute(x->x_op, f1, f2);
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

static void op_not(t_op *x){
    x->x_op = 8;
}

static void op_bitand(t_op *x){
    x->x_op = 9;
}

static void op_bitor(t_op *x){
    x->x_op = 10;
}

static void op_bitnot(t_op *x){
    x->x_op = 11;
}

static void op_bitxor(t_op *x){
    x->x_op = 12;
}

static void op_bitshift_l(t_op *x){
    x->x_op = 13;
}

static void op_bitshift_r(t_op *x){
    x->x_op = 14;
}

static void op_mod(t_op *x){
    x->x_op = 15;
}

static void op_mul(t_op *x){
    x->x_op = 16;
}

static void op_div(t_op *x){
    x->x_op = 17;
}

static void op_plus(t_op *x){
    x->x_op = 18;
}

static void op_minus(t_op *x){
    x->x_op = 19;
}

static void op_rmin(t_op *x){
    x->x_op = 20;
}

static void op_rdiv(t_op *x){
    x->x_op = 21;
}

static void op_min(t_op *x){
    x->x_op = 22;
}

static void op_max(t_op *x){
    x->x_op = 23;
}

static void op_pow(t_op *x){
    x->x_op = 24;
}

static void op_log(t_op *x){
    x->x_op = 25;
}

static void op_dsp(t_op *x, t_signal **sp){
    if(else_magic_inlet_connection((t_object *)x, x->x_glist, 1, &s_signal)){
        int n1 = sp[0]->s_length * sp[0]->s_nchans,
            n2 = sp[1]->s_length * sp[1]->s_nchans;
        int outchans = n2 > n1 ? sp[1]->s_nchans : n1 > 1 ? sp[0]->s_nchans : 1;
        signal_setmultiout(&sp[2], outchans);
        t_sample *vec1 = sp[0]->s_vec, *vec2 = sp[1]->s_vec;
        t_sample *outvec1 = sp[2]->s_vec;
        int i;
        if(n1 > n2)
            for(i = (n1+n2-1)/n2; i--; ){
                t_int blocksize = (n2 < n1 - i*n2 ? n2 : n1 - i*n2);
                dsp_add(op_perform, 5, x, vec1 + i * n2, vec2, outvec1 + i*n2, blocksize);
            }
        else for(i = (n1+n2-1)/n1; i--; ){
            t_int blocksize = (n1 < n2 - i*n1 ? n1 : n2 - i*n1);
            dsp_add(op_perform, 5, x, vec1, vec2 + i*n1, outvec1 + i*n1, blocksize);
        }
    }
    else{ // no sig connected
        int lchans = sp[0]->s_nchans;
        int vsize = x->x_val_size > 0 ? x->x_val_size : 1; // "no args": 1-value list of 0
        int nchans = lchans > vsize ? lchans : vsize;
        int blocksize = sp[0]->s_length;
        signal_setmultiout(&sp[2], nchans);
        t_sample *vec1 = sp[0]->s_vec;
        t_sample *outvec1 = sp[2]->s_vec;
        if(x->x_val_scratch_size != nchans){
            int oldsize = x->x_val_scratch_size;
            x->x_val_scratch = (t_float *)resizebytes(x->x_val_scratch,
                oldsize * sizeof(t_float), nchans * sizeof(t_float));
            for(int i = oldsize; i < nchans; i++)
                x->x_val_scratch[i] = x->x_val_size > 0 ? x->x_val_list[i % x->x_val_size] : 0;
            x->x_val_scratch_size = nchans;
        }
        dsp_add(op_perform_checkscalar, 1, x);
        for(int i = 0; i < nchans; i++){
            int lch = i % lchans;
            dsp_add(op_perform_scalar, 5, x, vec1 + lch*blocksize,
                    &x->x_val_scratch[i], outvec1 + i*blocksize, blocksize);
        }
    }
}

static void *op_new(t_symbol *s, int ac, t_atom *av){
    t_op *x = (t_op *)pd_new(op_class);
    x->x_op = 1; // default greater than
    x->x_glist = canvas_getcurrent();
    x->x_val_list = NULL;
    x->x_val_size = 0;
    x->x_val_scratch = NULL;
    x->x_val_scratch_size = 0;
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
    x->x_inlet_v = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    outlet_new(&x->x_obj, &s_signal);
    x->x_signalscalar = obj_findsignalscalar((t_object *)x, 1);
    else_magic_setnan(x->x_signalscalar);
    return(x);
errstate:
    pd_error(x, "[op~]: improper args");
    return(NULL);
}

static void *op_free(t_op *x){
    if(x->x_val_list)
        freebytes(x->x_val_list, x->x_val_size * sizeof(t_float));
    if(x->x_val_scratch)
        freebytes(x->x_val_scratch, x->x_val_scratch_size * sizeof(t_float));
    inlet_free(x->x_inlet_v);
    return(void *)x;
}

void op_tilde_setup(void){
    op_class = class_new(gensym("op~"), (t_newmethod)op_new, (t_method)op_free,
        sizeof(t_op), CLASS_MULTICHANNEL, A_GIMME, 0);
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
}
