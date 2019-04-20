
#include "m_pd.h"
#include <stdlib.h> // random
#include <string.h> // memset

/*#ifdef NT
#include <io.h>
#else
#include <unistd.h>
#endif*/

typedef struct _rands{
    t_object     x_obj;
    t_int        x_nvalues;         // number of values
    t_int       *x_probs;           // probability of a value
    t_int       *x_ovalues;         // number of outputs of each value
    t_outlet    *x_bang_outlet;
}t_rands;

static t_class *rands_class;

static void rands_list(t_rands *x, t_symbol*s, int ac, t_atom *av){
    t_symbol *dummy = s;
    dummy = NULL;
    if(ac){
        if(ac > x->x_nvalues)
            ac = x->x_nvalues;
        for(int i = 0; i < ac; i++)
            *(x->x_probs + i) = (int)av[i].a_w.w_float;
    }
}

static void rands_n(t_rands *x, t_float f){
    t_int n = (t_int)f;
    if(n < 1)
        n = 1;
    if(n != x->x_nvalues){
        x->x_nvalues = n;
        x->x_probs = (t_int*) getbytes(x->x_nvalues*sizeof(t_int));
        if(!x->x_probs){
            error("rands : could not allocate buffer");
            return;
        }
        x->x_ovalues = (t_int*) getbytes(x->x_nvalues*sizeof(t_int));
        if(!x->x_ovalues){
            error("rands : could not allocate buffer");
            return;
        }
        memset(x->x_ovalues, 0x0, x->x_nvalues*sizeof(t_int));
        int eq = 1; // default value
        for(int i = 0; i < x->x_nvalues; i++)
            *(x->x_probs+i) = eq;
    }
}

static void rands_bang(t_rands *x){
    t_int ei, ci;
    t_int *candidates;
    t_int nbcandidates = 0;
    t_int nevalues = 0;
    for(ei = 0; ei < x->x_nvalues; ei++) // get number of eligible values
        nevalues += (*(x->x_probs+ei) - *(x->x_ovalues+ei));
    if(nevalues == 0){
        post("[rands]: probabilities are null");
        outlet_bang(x->x_bang_outlet);
        return;
    }
    candidates = (t_int*) getbytes(nevalues*sizeof(t_int));
    if(!candidates){
        error("rands : could not allocate buffer for internal computation");
        return;
    }
    for(ei = 0; ei < x->x_nvalues; ei++){ // select eligible values
        if(*(x->x_ovalues+ei) < *(x->x_probs+ei)){
            for(ci = 0; ci < *(x->x_probs+ei) - *(x->x_ovalues+ei); ci++){
                *(candidates+nbcandidates) = ei;
                nbcandidates++;
            }
        }
    }
    int chosen = random() % nbcandidates;
    int v = *(candidates+chosen);
    outlet_float(x->x_obj.ob_outlet, v);
    *(x->x_ovalues+v) = *(x->x_ovalues+v) + 1;
    if(nbcandidates == 1 ){ // end of the serial
        outlet_bang(x->x_bang_outlet);
        memset(x->x_ovalues, 0x0, x->x_nvalues*sizeof(t_int));
    }
    if(candidates)
        freebytes(candidates,  nevalues*sizeof(t_int));
}

static void rands_set(t_rands *x, t_float f, t_float v){
    int i = (int)f;
    if(i < 0 || i >= x->x_nvalues){
        post("[rands]: %d not available", i);
        return;
    }
    *(x->x_probs+i) = v < 0 ? 0 : (int)v;
}

static void rands_inc(t_rands *x, t_float f){
    int v = (int)f;
    if(v < 0 || v >= x->x_nvalues){
        post("[rands]: %d not available", v);
        return;
    }
    *(x->x_probs+v) += 1;
}

static void rands_restart(t_rands *x){
    memset(x->x_ovalues, 0x0, x->x_nvalues*sizeof(t_int));
}

static void rands_eq(t_rands *x, t_float f){
    int v = f < 1 ? 1 : (int)f;
    for(t_int ei = 0; ei < x->x_nvalues; ei++)
        *(x->x_probs+ei) = v;
    memset(x->x_ovalues, 0x0, x->x_nvalues*sizeof(t_int));
}

static t_rands *rands_new(t_symbol *s, int ac, t_atom *av){
    t_symbol *dummy = s;
    dummy = NULL;
    t_rands *x = (t_rands *)pd_new(rands_class);
    x->x_nvalues = 1;
    if(ac){
        x->x_nvalues = av[0].a_w.w_float;
        if(x->x_nvalues < 1)
            x->x_nvalues = 1;
        ac--;
        if(ac > x->x_nvalues) // ignore extra args
            ac = x->x_nvalues;
    }
    // common fields for new and restored randss
    x->x_probs = (t_int*) getbytes(x->x_nvalues*sizeof(t_int));
    if(!x->x_probs){
        error("rands : could not allocate buffer");
        return NULL;
    }
    x->x_ovalues = (t_int*) getbytes(x->x_nvalues*sizeof(t_int));
    if(!x->x_ovalues){
        error("rands : could not allocate buffer");
        return NULL;
    }
    memset(x->x_ovalues, 0x0, x->x_nvalues*sizeof(t_int));
    int eq = 1; // default value
    for(int i = 0; i < x->x_nvalues; i++){
        if(ac){
            *(x->x_probs + i) = (int)av[i+1].a_w.w_float;
            ac--;
        }
        else
            *(x->x_probs+i) = eq;
    }
    inlet_new((t_object *)x, (t_pd *)x, &s_float, gensym("n"));
    outlet_new(&x->x_obj, &s_float);
    x->x_bang_outlet = outlet_new(&x->x_obj, &s_bang);
    return(x);
}

static void rands_free(t_rands *x){
    if(x->x_probs)
        freebytes(x->x_probs, x->x_nvalues*sizeof(int));
    if(x->x_ovalues)
        freebytes(x->x_ovalues, x->x_nvalues*sizeof(int));
}

void rands_setup(void){
    rands_class = class_new(gensym("rands"), (t_newmethod)rands_new,
            (t_method)rands_free, sizeof(t_rands), 0, A_GIMME, 0);
    class_addbang(rands_class, rands_bang);
    class_addlist(rands_class, rands_list);
    class_addmethod(rands_class, (t_method)rands_eq, gensym("eq"), A_FLOAT, 0);
    class_addmethod(rands_class, (t_method)rands_inc, gensym("inc"), A_FLOAT, 0);
    class_addmethod(rands_class, (t_method)rands_n, gensym("n"), A_FLOAT, 0);
    class_addmethod(rands_class, (t_method)rands_set, gensym("set"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(rands_class, (t_method)rands_restart, gensym("restart"), 0);
}
