// Porres 2018

#include "m_pd.h"
//#include <string.h>
#include <stdlib.h>

static t_class *chance_class;

typedef struct _chance{
    t_object   x_obj;
    t_atom    *x_av;
    int        x_ac;
    int        x_outlets;
    int        x_val;
    t_float    x_out_of;
    t_float    x_lastin;
    t_float     x_range;
    t_inlet   *x_n_let;
    float     *x_outs[512];
}t_chance;

static void chance_seed(t_chance *x, t_floatarg f){
    x->x_val = (int)f * 1319;
}

static t_int *chance_perform(t_int *w){
    int i;
    t_chance *x = (t_chance *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    for(i = 0; i < x->x_outlets; i++)
        x->x_outs[i] = (t_float *)(w[4+i]); // all outputs
    int val = x->x_val;
    t_float lastin = x->x_lastin;
    while(nblock--){
        float trig = *in++;
        if(trig != 0 && lastin == 0){
            float random = ((float)((val & 0x7fffffff) - 0x40000000)) * (float)(1.0 / 0x40000000);
            random = x->x_range * (random + 1) / 2;
            val = val * 435898247 + 382842987;
            for(int n = 0; n < x->x_outlets; n++){
                if(random < x->x_av[n].a_w.w_float){
                    *x->x_outs[n]++ = trig;
                    break;
                }
            }
        }
        else{
            for(int n = 0; n < x->x_outlets; n++)
                *x->x_outs[n]++ = 0;
        }
        lastin = trig;
    }
    x->x_val = val;
    x->x_lastin = lastin; // last input
    return(w+4+x->x_outlets);
}

static void chance_dsp(t_chance *x, t_signal **sp){
    int i, count = x->x_outlets + 3;
    t_int **sigvec;
    sigvec = (t_int **)calloc(count, sizeof(t_int *));
    for(i = 0; i < count; i++)
        sigvec[i] = (t_int *)calloc(sizeof(t_int), 1); // init sigvec
    sigvec[0] = (t_int *)x; // 1st => object
    sigvec[1] = (t_int *)sp[0]->s_n; // 2nd => block (n)
    for(i = 2; i < count; i++) // in/outs
        sigvec[i] = (t_int *)sp[i-2]->s_vec;
    dsp_addv(chance_perform, count, (t_int *)sigvec);
    free(sigvec);
}

static void *chance_new(t_symbol *s, int ac, t_atom *av){
    t_chance *x = (t_chance *)pd_new(chance_class);
    t_symbol *dummy = s;
    dummy = NULL;
// default
    static int init_seed = 234599;
    init_seed *= 1319;
    t_int seed = init_seed;
    x->x_out_of = 100;
/////////////////////////////////////////////////////////////////////////////////////
    
    if(!ac){
        x->x_ac = x->x_outlets = 2;
        x->x_av = (t_atom *)getbytes(x->x_ac*sizeof(t_atom));
        SETFLOAT(x->x_av, 50);
        SETFLOAT(x->x_av+1, 100);
        outlet_new(&x->x_obj, gensym("signal"));
        outlet_new(&x->x_obj, gensym("signal"));
        x->x_range = 100;
    }
    else if(ac == 1){
        if(av->a_type == A_FLOAT){
            t_float aval = atom_getfloatarg(0, ac, av);
            x->x_ac = x->x_outlets = 2;
            x->x_av = (t_atom *)getbytes(x->x_ac*sizeof(t_atom));
            SETFLOAT(x->x_av, aval);
            SETFLOAT(x->x_av+1, 100);
            x->x_range = 100;
            outlet_new(&x->x_obj, gensym("signal"));
            outlet_new(&x->x_obj, gensym("signal"));
        }
        else if(av->a_type == A_SYMBOL){
            pd_error(x, "[chance~]: takes only floats as arguments");
            return(NULL);
        }
    }
    else{
        x->x_ac = ac;
        x->x_av = (t_atom *)getbytes(x->x_ac*sizeof(t_atom));
        int n = 0;
        while(ac > 0){
            if(av->a_type == A_FLOAT){
                t_float aval = atom_getfloatarg(0, ac, av);
                SETFLOAT(x->x_av+n, x->x_range += aval);
            }
/*            else if(av->a_type == A_SYMBOL){
                if(atom_getsymbolarg(0, ac, av) == gensym("-index")){
                    x->x_index = 1;
                    x->x_ac--;
                    n--;
                }
            }*/
            n++;
            av++;
            ac--;
        }
        x->x_outlets = x->x_ac;
        for(int i = 0; i < x->x_ac; i++)
            outlet_new(&x->x_obj, gensym("signal"));
    }
    
/////////////////////////////////////////////////////////////////////////////////////
    x->x_val = seed; // load seed value
    x->x_lastin = 0;
//
    return(x);
}

void chance_tilde_setup(void){
    chance_class = class_new(gensym("chance~"), (t_newmethod)chance_new,
        0, sizeof(t_chance), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(chance_class, nullfn, gensym("signal"), 0);
    class_addmethod(chance_class, (t_method)chance_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(chance_class, (t_method)chance_seed, gensym("seed"), A_DEFFLOAT, 0);
}
