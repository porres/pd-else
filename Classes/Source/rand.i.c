// Porres 2017
 
#include "m_pd.h"
#include <time.h>

typedef struct _randi{
    t_object     x_obj;
    unsigned int x_state;
    t_float      x_min;
    t_float      x_max;
}t_randi;

static t_class *randi_class;
static int initflag = 0;

static void randi_bang(t_randi *x){
    int min = (int)x->x_min; // Output LOW
    int max = (int)x->x_max + 1; // Output HIGH
    if(min > max){
        int temp = min;
        min = max;
        max = temp;
    }
    int range = max - min; // range
    int random = min;
    if(range){
        x->x_state = x->x_state * 472940017 + 832416023;
        random = ((double)range) * ((double)x->x_state) * (1./4294967296.);
        if(random >= range)
            random = range-1;
        random += min;
    }
    outlet_float(x->x_obj.ob_outlet, random);
}

static void randi_seed(t_randi *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(!ac){
        unsigned int seed = time(NULL) + (initflag * 151);
        seed = seed * 435898247 + 938284287;
        x->x_state = seed & 0x7fffffff;
    }
    else
        x->x_state = (unsigned int)(atom_getfloat(av));
}

static void *randi_new(t_symbol *s, int argc, t_atom *argv){
    s = NULL;
    t_randi *x = (t_randi *) pd_new(randi_class);
    unsigned int initseed = time(NULL);
    int seed_flag = 0;
//////////////////////////////////////////////////////////
    x->x_min = 0.;
    x->x_max = 1.;
    if(argc == 1){
        if(argv -> a_type == A_FLOAT){
            x->x_min = 0;
            x->x_max = atom_getfloat(argv);
        }
    }
    else if(argc > 1 && argc <= 3){
        int numargs = 0;
        while(argc > 0){
            if(argv->a_type == A_FLOAT){
                switch(numargs){
                    case 0: x->x_min = atom_getfloatarg(0, argc, argv);
                        numargs++;
                        argc--;
                        argv++;
                        break;
                    case 1: x->x_max = atom_getfloatarg(0, argc, argv);
                        numargs++;
                        argc--;
                        argv++;
                        break;
                    case 2: initseed = atom_getfloatarg(0, argc, argv);
                        seed_flag = 1;
                        numargs++;
                        argc--;
                        argv++;
                        break;
                    default:
                        argc--;
                        argv++;
                        break;
                };
            }
            else
                goto errstate;
        };
    }
    else if(argc > 3)
        goto errstate;
////////////////////////////////////////////////////////////
    if(seed_flag)
        x->x_state = initseed;
    else{
        unsigned int seed = initseed + (initflag * 151);
        seed = seed * 435898247 + 938284287;
        x->x_state = seed & 0x7fffffff;
    }
    initflag++;
    floatinlet_new((t_object *)x, &x->x_min);
    floatinlet_new((t_object *)x, &x->x_max);
    outlet_new((t_object *)x, &s_float);
    return(x);
errstate:
    pd_error(x, "[rand.i]: improper args");
    return NULL;
}

void setup_rand0x2ei(void){
  randi_class = class_new(gensym("rand.i"), (t_newmethod)randi_new, 0, sizeof(t_randi), 0, A_GIMME, 0);
  class_addmethod(randi_class, (t_method)randi_seed, gensym("seed"), A_GIMME, 0);
  class_addbang(randi_class, (t_method)randi_bang);
}
