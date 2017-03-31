// Porres 2017

#include <math.h>
#include "m_pd.h"

static t_class *quadmap_class;

typedef struct _quadmap
{
    t_object  x_obj;
    int x_val;
    t_float x_sr;
    double  x_a;
    double  x_b;
    double  x_c;
    double  x_yn;
    double  x_phase;
    t_float  x_freq;
    t_outlet *x_outlet;
} t_quadmap;


static void quadmap_k(t_quadmap *x, t_float f)
{
    //    x->x_a = f;
    //    x->x_c = f;
    //    x->x_m = f;
    //    x->x_yn = f;
}

static void quadmap_list(t_quadmap *x, t_symbol *s, int argc, t_atom * argv)
{
/*    if (argc > 2)
        {
        pd_error(x, "quadmap~: list size needs to be = 2");
        }
    else{
        int argnum = 0; // current argument
        while(argc)
        {
            if(argv -> a_type != A_FLOAT)
                {
                pd_error(x, "quadmap~: list needs to only contain floats");
                }
            else
                {
                t_float curf = atom_getfloatarg(0, argc, argv);
                switch(argnum)
                    {
                    case 0:
                        x->x_xn = curf;
                        break;
                    case 1:
                        x->x_yn = curf;
                        break;
                    };
                argnum++;
                };
            argc--;
            argv++;
        };
    } */
}


static t_int *quadmap_perform(t_int *w)
{
    t_quadmap *x = (t_quadmap *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_sample *out = (t_sample *)(w[4]);
    double yn = x->x_yn;
    double a = x->x_a;
    double b = x->x_b;
    double c = x->x_c;
    double phase = x->x_phase;
    double sr = x->x_sr;
    while (nblock--)
    {
        t_float hz = *in++;
        double phase_step = hz / sr; // phase_step
        phase_step = phase_step > 1 ? 1. : phase_step < -1 ? -1 : phase_step; // clipped phase_step
        int trig;
        t_float output;
        if (hz >= 0)
            {
            trig = phase >= 1.;
            if (trig) phase = phase - 1;
            }
        else
            {
            trig = (phase <= 0.);
            if (trig) phase = phase + 1.;
            }
        if (trig) // update
            {
            yn = a * yn*yn + b * yn + c;
            }
        *out++ = yn;
        phase += phase_step;
    }
    x->x_phase = phase;
    x->x_yn = yn;
    return (w + 5);
}


static void quadmap_dsp(t_quadmap *x, t_signal **sp)
{
    x->x_sr = sp[0]->s_sr;
    dsp_add(quadmap_perform, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
}

static void *quadmap_free(t_quadmap *x)
{
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *quadmap_new(t_symbol *s, int ac, t_atom *av)
{
    t_quadmap *x = (t_quadmap *)pd_new(quadmap_class);
    x->x_sr = sys_getsr();
    t_float hz = x->x_sr * 0.5, a = 1, b = -1, c = -0.75, yn = 0; // default parameters
    if (ac && av->a_type == A_FLOAT)
    {
        hz = av->a_w.w_float;
        ac--; av++;
        if (ac && av->a_type == A_FLOAT)
            a = av->a_w.w_float;
            ac--; av++;
            if (ac && av->a_type == A_FLOAT)
                b = av->a_w.w_float;
                ac--; av++;
                if (ac && av->a_type == A_FLOAT)
                    c = av->a_w.w_float;
                    ac--; av++;
                    if (ac && av->a_type == A_FLOAT)
                        yn = av->a_w.w_float;
    }
    if(hz >= 0) x->x_phase = 1;
    x->x_freq  = hz;
    x->x_a = a;
    x->x_b = b;
    x->x_c = c;
    x->x_yn = yn;
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return (x);
}

void quadmap_tilde_setup(void)
{
    quadmap_class = class_new(gensym("quadmap~"),
        (t_newmethod)quadmap_new, (t_method)quadmap_free,
        sizeof(t_quadmap), 0, A_GIMME, 0);
    CLASS_MAINSIGNALIN(quadmap_class, t_quadmap, x_freq);
    class_addlist(quadmap_class, quadmap_list);
    class_addmethod(quadmap_class, (t_method)quadmap_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(quadmap_class, (t_method)quadmap_k, gensym("k"), A_DEFFLOAT, 0);
}
