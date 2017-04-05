// Porres 2017

#include "m_pd.h"

static t_class *random_class;

typedef struct _random
{
    t_object   x_obj;
    int        x_val;
    t_float    x_random;
    t_outlet  *x_outlet;
} t_random;

static void random_seed(t_random *x, t_floatarg f)
{
    x->x_val = (int)f * 1319;
}

static t_int *random_perform(t_int *w)
{
    t_random *x = (t_random *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_float *out = (t_sample *)(w[4]);
    int val = x->x_val;
    t_float random = x->x_random;
    while (nblock--)
        {
        float trig = *in++;
        if (trig == 1) // update
            {
            random = ((float)((val & 0x7fffffff) - 0x40000000)) * (float)(1.0 / 0x40000000);
            val = val * 435898247 + 382842987;
            }
        *out++ = random;
        }
    x->x_val = val;
    x->x_random = random; // current output
    return (w + 5);
}

static void random_dsp(t_random *x, t_signal **sp)
{
    dsp_add(random_perform, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
}

static void *random_free(t_random *x)
{
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *random_new(t_symbol *s, int ac, t_atom *av)
{
    t_random *x = (t_random *)pd_new(random_class);
// default seed
    static int init_seed = 234599;
    init_seed *= 1319;
    t_int seed = init_seed;
 // get arg
    int argnum = 0;
    while(ac)
    {
        if(av -> a_type != A_FLOAT) goto errstate;
        else
        {
            t_float curf = atom_getfloatarg(0, ac, av);
            switch(argnum)
            {
                case 0:
                    seed = (int)curf * 1319;
                    break;
            };
            argnum++;
        };
        ac--;
        av++;
    };
    x->x_val = seed; // load seed value
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return (x);
    errstate:
        pd_error(x, "random~: arguments needs to only contain floats");
        return NULL;
}

void random_tilde_setup(void)
{
    random_class = class_new(gensym("random~"),
        (t_newmethod)random_new,
        (t_method)random_free,
        sizeof(t_random),
        CLASS_DEFAULT,
        A_GIMME,
        0);
    class_addmethod(random_class, nullfn, gensym("signal"), 0);
    class_addmethod(random_class, (t_method)random_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(random_class, (t_method)random_seed, gensym("seed"), A_DEFFLOAT, 0);
}
