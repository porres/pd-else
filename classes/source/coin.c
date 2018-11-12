// Porres 2018

#include "m_pd.h"

static t_class *coin_class;

typedef struct _coin{
    t_object    x_obj;
    t_int       x_val;
    t_float     x_prob;
} t_coin;

static void coin_seed(t_coin *x, t_float f){
    x->x_val = (int)f * 1319;
}

static void coin_bang(t_coin *x){
    int val = x->x_val;
    t_float random = ((float)((val & 0x7fffffff) - 0x40000000)) * (float)(1.0 / 0x40000000);
    random = 100 * (random + 1) / 2; // rescale
    x->x_val = val * 435898247 + 382842987;
    if(x->x_prob > 0){
        if(x->x_prob == 100)
            outlet_bang(x->x_obj.ob_outlet);
        else if(random < x->x_prob)
            outlet_bang(x->x_obj.ob_outlet);
    }
}

static void *coin_new(t_symbol *s, int ac, t_atom *av){
    t_symbol *sym = s;
    sym = NULL;
    t_coin *x = (t_coin *) pd_new(coin_class);
    static int init_seed = 54569;
    x->x_prob = 50;
    /////////////////////////////////////////////////////////////////////////////////////
    int argnum = 0;
    while(ac){
        if(av->a_type != A_FLOAT)
            goto errstate;
        else{
            t_float curf = atom_getfloatarg(0, ac, av);
            switch(argnum){
                case 0:
                    x->x_prob = curf;
                    break;
                case 1:
                    init_seed = curf;
                    break;
                default:
                    break;
            };
        };
        argnum++;
        ac--;
        av++;
    };
    /////////////////////////////////////////////////////////////////////////////////////
    if(x->x_prob < 0){
        x->x_prob = 0;
    }
    if(x->x_prob > 100){
        x->x_prob = 100;
    }
///////////////////////////////////////////////
    x->x_val = init_seed *= 1319; // load seed value
    floatinlet_new((t_object *)x, &x->x_prob);
    outlet_new((t_object *)x, &s_float);
    return (x);
errstate:
    pd_error(x, "[coin]: improper args");
    return NULL;
}

void coin_setup(void){
    coin_class = class_new(gensym("coin"), (t_newmethod)coin_new, 0, sizeof(t_coin), 0, A_GIMME, 0);
    class_addmethod(coin_class, (t_method)coin_seed, gensym("seed"), A_DEFFLOAT, 0);
    class_addbang(coin_class, (t_method)coin_bang);
}
