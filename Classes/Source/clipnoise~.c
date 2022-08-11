// matt barber and porres (2018)
// based on SuperCollider's clipnoise UGen

#include "m_pd.h"
#include "random.h"

static t_class *clipnoise_class;

typedef struct _clipnoise{
    t_object       x_obj;
    t_random_state x_rstate;
}t_clipnoise;

static unsigned int instanc_n = 0;

static void clipnoise_seed(t_clipnoise *x, t_symbol *s, int ac, t_atom *av){
    random_init(&x->x_rstate, get_seed(s, ac, av, ++instanc_n));
}

static t_int *clipnoise_perform(t_int *w){
    int n = (t_int)(w[1]);
    t_random_state *rstate = (t_random_state *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    uint32_t *s1 = &rstate->s1;
    uint32_t *s2 = &rstate->s2;
    uint32_t *s3 = &rstate->s3;
    while(n--)
        *out++ = (t_float)(random_frand(s1, s2, s3)) > 0 ? 1 : -1;
    return(w+5);
}

static void clipnoise_dsp(t_clipnoise *x, t_signal **sp){
    dsp_add(clipnoise_perform, 4, sp[0]->s_n, &x->x_rstate, sp[0]->s_vec);
}

static void *clipnoise_new(t_symbol *s, int ac, t_atom *av){
    t_clipnoise *x = (t_clipnoise *)pd_new(clipnoise_class);
    outlet_new(&x->x_obj, &s_signal);
    clipnoise_seed(x, s, ac, av);
    return(x);
}

void clipnoise_tilde_setup(void){
    clipnoise_class = class_new(gensym("clipnoise~"), (t_newmethod)clipnoise_new,
        0, sizeof(t_clipnoise), 0, A_GIMME, 0);
    class_addmethod(clipnoise_class, (t_method)clipnoise_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(clipnoise_class, (t_method)clipnoise_seed, gensym("seed"), A_GIMME, 0);
}
