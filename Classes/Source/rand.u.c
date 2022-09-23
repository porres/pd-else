// porres 2022

#include "m_pd.h"
#include <stdlib.h>
#include <string.h>
#include <random.h>

typedef struct _rand_u{
    t_object        x_obj;
    int             x_nvalues;  // number of values
    int            *x_probs;    // probability of a value
    int            *x_ovalues;  // number of outputs of each value
    t_random_state  x_rstate;
    t_outlet       *x_bang_outlet;
    int             x_id;
}t_rand_u;

static t_class *rand_u_class;

static void rand_u_n(t_rand_u *x, t_float f){
    int n = (int)f;
    if(n < 1)
        n = 1;
    if(n != x->x_nvalues){
        x->x_nvalues = n;
        x->x_probs = (int*) getbytes(x->x_nvalues*sizeof(int));
        if(!x->x_probs){
            pd_error(x, "[rand.u]: could not allocate buffer");
            return;
        }
        x->x_ovalues = (int*) getbytes(x->x_nvalues*sizeof(int));
        if(!x->x_ovalues){
            pd_error(x, "[rand.u]: could not allocate buffer");
            return;
        }
        memset(x->x_ovalues, 0x0, x->x_nvalues*sizeof(int));
        int eq = 1; // default value
        for(int i = 0; i < x->x_nvalues; i++)
            *(x->x_probs+i) = eq;
    }
}

static void rand_u_seed(t_rand_u *x, t_symbol *s, int ac, t_atom *av){
    random_init(&x->x_rstate, get_seed(s, ac, av, x->x_id));
}

static void rand_u_bang(t_rand_u *x){
    int *candidates;
    int ei, ci, nbcandidates = 0, nevalues = 0;
    for(ei = 0; ei < x->x_nvalues; ei++) // get number of eligible values
        nevalues += (*(x->x_probs+ei) - *(x->x_ovalues+ei));
    if(nevalues == 0){
        post("[rand.u]: probabilities are null");
        return;
    }
    candidates = (int*)getbytes(nevalues*sizeof(int));
    for(ei = 0; ei < x->x_nvalues; ei++){ // select eligible values
        if(*(x->x_ovalues+ei) < *(x->x_probs+ei)){
            for(ci = 0; ci < *(x->x_probs+ei) - *(x->x_ovalues+ei); ci++){
                *(candidates+nbcandidates) = ei;
                nbcandidates++;
            }
        }
    }
    uint32_t *s1 = &x->x_rstate.s1;
    uint32_t *s2 = &x->x_rstate.s2;
    uint32_t *s3 = &x->x_rstate.s3;
    t_float noise = (t_float)(random_frand(s1, s2, s3)) * 0.5 + 0.5;
    int nval = (int)(noise * nbcandidates);    
    if(nval >= nbcandidates)
        nval = nbcandidates-1;
    int v = *(candidates+nval);
    outlet_float(x->x_obj.ob_outlet, v);
    *(x->x_ovalues+v) = *(x->x_ovalues+v) + 1;
    if(nbcandidates == 1){ // end of the serial
        outlet_bang(x->x_bang_outlet);
        memset(x->x_ovalues, 0x0, x->x_nvalues*sizeof(int));
    }
    if(candidates)
        freebytes(candidates, nevalues*sizeof(int));
}

static void rand_u_list(t_rand_u *x, t_symbol*s, int ac, t_atom *av){
    s = NULL;
    if(!ac)
        rand_u_bang(x);
    else{
        x->x_nvalues = ac;
        x->x_probs = (int*)getbytes(x->x_nvalues*sizeof(int));
        x->x_ovalues = (int*)getbytes(x->x_nvalues*sizeof(int));
        for(int i = 0; i < x->x_nvalues; i++){
            int v = (int)av[i].a_w.w_float;
            *(x->x_probs + i) = v < 0 ? 0 : v;
        }
        memset(x->x_ovalues, 0x0, x->x_nvalues*sizeof(int));
    }
}

static void rand_u_set(t_rand_u *x, t_float f, t_float v){
    int i = (int)f;
    if(i < 0 || i >= x->x_nvalues){
        post("[rand.u]: %d not available", i);
        return;
    }
    *(x->x_probs+i) = v < 0 ? 0 : (int)v;
}

static void rand_u_inc(t_rand_u *x, t_float f){
    int v = (int)f;
    if(v < 0 || v >= x->x_nvalues){
        post("[rand.u]: %d not available", v);
        return;
    }
    *(x->x_probs+v) += 1;
}

static void rand_u_dec(t_rand_u *x, t_float f){
    int v = (int)f;
    if(v < 0 || v >= x->x_nvalues){
        post("[rand.u]: %d not available", v);
        return;
    }
    *(x->x_probs+v) -= 1;
}

static void rand_u_clear(t_rand_u *x){
    memset(x->x_ovalues, 0x0, x->x_nvalues*sizeof(int));
}

static void rand_u_eq(t_rand_u *x, t_float f){
    int v = f < 1 ? 1 : (int)f;
    for(int ei = 0; ei < x->x_nvalues; ei++)
        *(x->x_probs+ei) = v;
    memset(x->x_ovalues, 0x0, x->x_nvalues*sizeof(int));
}

static t_rand_u *rand_u_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_rand_u *x = (t_rand_u *)pd_new(rand_u_class);
    x->x_id = random_get_id();
    x->x_nvalues = 1;
    rand_u_seed(x, s, 0, NULL);
    x->x_nvalues = 1;
    int eq = 1; // default value
    while(ac && av[0].a_type == A_SYMBOL){
        if(av[0].a_w.w_symbol == gensym("-seed")){
            if(ac >= 2){
                if(av[1].a_type == A_FLOAT){
                    t_atom at[1];
                    SETFLOAT(at, atom_getfloat(av+1));
                    ac-=2, av+=2;
                    rand_u_seed(x, s, 1, at);
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
        }
        else if(av[0].a_w.w_symbol == gensym("-size")){
            if(ac >= 2){
                if(av[1].a_type == A_FLOAT){
                    x->x_nvalues = av[1].a_w.w_float;
                    ac-=2, av+=2;
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
        }
        else if(av[0].a_w.w_symbol == gensym("-eq")){
            if(ac >= 2){
                if(av[1].a_type == A_FLOAT){
                    eq = av[1].a_w.w_float;
                    ac-=2, av+=2;
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    }
    if(ac && av[0].a_type == A_FLOAT)
        x->x_nvalues = ac;
    // common fields for new and restored rand_us
    x->x_probs = (int*)getbytes(x->x_nvalues*sizeof(int));
    x->x_ovalues = (int*)getbytes(x->x_nvalues*sizeof(int));
    memset(x->x_ovalues, 0x0, x->x_nvalues*sizeof(int));
    for(int i = 0; i < x->x_nvalues; i++){
        if(ac){
            *(x->x_probs+i) = (int)av[i].a_w.w_float;
            ac--;
        }
        else
            *(x->x_probs+i) = eq;
    }
    outlet_new(&x->x_obj, &s_float);
    x->x_bang_outlet = outlet_new(&x->x_obj, &s_bang);
    return(x);
errstate:
    post("[rand.u] improper args");
    return(NULL);
}

static void rand_u_free(t_rand_u *x){
    if(x->x_probs)
        freebytes(x->x_probs, x->x_nvalues*sizeof(int));
    if(x->x_ovalues)
        freebytes(x->x_ovalues, x->x_nvalues*sizeof(int));
}

void setup_rand0x2eu(void){
    rand_u_class = class_new(gensym("rand.u"), (t_newmethod)rand_u_new,
        (t_method)rand_u_free, sizeof(t_rand_u), 0, A_GIMME, 0);
    class_addlist(rand_u_class, rand_u_list);
    class_addmethod(rand_u_class, (t_method)rand_u_eq, gensym("eq"), A_FLOAT, 0);
    class_addmethod(rand_u_class, (t_method)rand_u_inc, gensym("inc"), A_FLOAT, 0);
    class_addmethod(rand_u_class, (t_method)rand_u_dec, gensym("dec"), A_FLOAT, 0);
    class_addmethod(rand_u_class, (t_method)rand_u_n, gensym("size"), A_FLOAT, 0);
    class_addmethod(rand_u_class, (t_method)rand_u_seed, gensym("seed"), A_GIMME, 0);
    class_addmethod(rand_u_class, (t_method)rand_u_set, gensym("set"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(rand_u_class, (t_method)rand_u_clear, gensym("clear"), 0);
}
