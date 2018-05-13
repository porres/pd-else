// matt barber and porres (2017-2018)
// based on SuperCollider's GrayNoise UGen

#include "m_pd.h"
#include "random.h"

static t_class *brown_class;

typedef struct _brown{
    t_object  x_obj;
    t_random_state x_rstate;
    t_float  x_lastout;
    t_outlet *x_outlet;
} t_brown;

static void brown_float(t_brown *x, t_floatarg f){
    random_init(&x->x_rstate, f);
}

static t_int *brown_perform(t_int *w){
    t_brown *x = (t_brown *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_random_state *rstate = (t_random_state *)(w[3]);
    t_sample *out = (t_sample *)(w[4]);
    uint32_t *s1 = &rstate->s1;
    uint32_t *s2 = &rstate->s2;
    uint32_t *s3 = &rstate->s3;
    t_float lastout = x->x_lastout;
    while(nblock--){
        t_float input = random_frand(s1, s2, s3) * 0.125;
        t_float output = input + lastout;
        if (output > 1)
            output = 2 - output;
        if (output < -1)
            output = -2 - output;
        *out++ = output;
        lastout = output;
    }
    x->x_lastout = lastout;
    return (w + 5);
}

static void brown_dsp(t_brown *x, t_signal **sp){
    dsp_add(brown_perform, 4, x, sp[0]->s_n, &x->x_rstate, sp[0]->s_vec);
}

static void *brown_new(t_symbol *s, int ac, t_atom *av){
    t_brown *x = (t_brown *)pd_new(brown_class);
    x->x_lastout = 0;
    static int seed = 1;
    if((ac) && (av->a_type == A_FLOAT))
        random_init(&x->x_rstate, atom_getfloatarg(0, ac, av));
    else
    	random_init(&x->x_rstate, seed++);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void brown_tilde_setup(void){
    brown_class = class_new(gensym("brown~"), (t_newmethod)brown_new,
                            0, sizeof(t_brown), 0, A_GIMME, 0);
    class_addmethod(brown_class, (t_method)brown_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(brown_class, brown_float);
}
