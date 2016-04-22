/* Copyright (c) 2002-2003 krzYszcz and others.
 * For information on usage and redistribution, and for a DISCLAIMER OF ALL
 * WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

#include "m_pd.h"

typedef struct _sh
{
    t_object x_obj;
    t_float  x_threshold;
    t_float  x_lastin;
    t_float  x_lastout;
} t_sh;

static t_class *sh_class;


static t_int *sh_perform(t_int *w)
{
    t_sh *x = (t_sh *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    t_float threshold = x->x_threshold;
    t_float lastin = x->x_lastin;
    t_float lastout = x->x_lastout;
    while (nblock--)
    {
        float f = *in2++;
        if (f > threshold)  /* CHECKME <=, > */
            lastout = *in1;
        in1++;
        lastin = f;
        *out++ = lastout;
    }
    x->x_lastin = lastin;
    x->x_lastout = lastout;
    return (w + 6);
}

static void sh_dsp(t_sh *x, t_signal **sp)
{
    dsp_add(sh_perform, 5, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void sh_float(t_sh *x, t_float f)
{
    x->x_threshold = f;
}

static void *sh_new(t_floatarg f)
{
    t_sh *x = (t_sh *)pd_new(sh_class);
    x->x_threshold = f;
    x->x_lastin = 0;
    x->x_lastout = 0;
    inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    return (x);
}

void sh_tilde_setup(void)
{
    sh_class = class_new(gensym("sh~"),
        (t_newmethod)sh_new,
        0,
        sizeof(t_sh),
        CLASS_DEFAULT,
        A_DEFFLOAT,
        0);
        class_addmethod(sh_class, nullfn, gensym("signal"), 0);
    	class_addfloat(sh_class, (t_method)sh_float);
}

