

#include "m_pd.h"

typedef struct _peak
{
    t_object  x_obj;
    t_inlet  *peak;
    t_float   x_value;
    int       x_nwait;
    int       x_nleft;
    int       x_precount;
    float     x_waittime;
    float     x_ksr;
    t_clock  *x_clock;
} t_peak;

static t_class *peak_class;

static void peak_tick(t_peak *x)
{
    outlet_float(((t_object *)x)->ob_outlet, x->x_value);
    x->x_value = 0;
    if ((x->x_nleft = x->x_nwait - x->x_precount) < 0)  /* CHECKME */
	x->x_nleft = 0;
}

static void peak_bang(t_peak *x)
{
    peak_tick(x);  /* CHECKME */
}

static void peak_ft1(t_peak *x, t_floatarg f)
{
    if ((x->x_waittime = f) < 0.)
	x->x_waittime = 0.;
    if ((x->x_nwait = (int)(x->x_waittime * x->x_ksr)) < 0)
	x->x_nwait = 0;
}

static t_int *peak_perform(t_int *w)
{
    t_peak *x = (t_peak *)(w[1]); 
    int nblock = (int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_float value = x->x_value;
    if (x->x_nwait)
        {
        if (x->x_nleft < nblock)
            {
            clock_delay(x->x_clock, 0);
            x->x_precount = nblock - x->x_nleft;
            x->x_nleft = 0;  /* LATER rethink */
            }
        else x->x_nleft -= nblock;
        }
    while (nblock--)
        {
        t_float f = *in++;
        if (f > value)
            value = f;
        else if (f < -value)
            value = -f;
        }
    x->x_value = value;
    return (w + 4);
}

static void peak_dsp(t_peak *x, t_signal **sp)
{
    x->x_ksr = sp[0]->s_sr * 0.001;
    x->x_nwait = (int)(x->x_waittime * x->x_ksr);
    dsp_add(peak_perform, 3, x, sp[0]->s_n, sp[0]->s_vec);
}

static void peak_free(t_peak *x)
{
    if (x->x_clock) clock_free(x->x_clock);
}

static void *peak_new(t_floatarg f)
{
    t_peak *x = (t_peak *)pd_new(peak_class);
    x->x_value = 0.;
    x->x_nleft = 0;
    x->x_ksr = sys_getsr() * 0.001;
    peak_ft1(x, f);
    inlet_new((t_object *)x, (t_pd *)x, &s_float, gensym("ft1"));
    outlet_new((t_object *)x, &s_float);
    x->x_clock = clock_new(x, (t_method)peak_tick);
    return (x);
}

void peak_tilde_setup(void)
{
    peak_class = class_new(gensym("peak~"),
			      (t_newmethod)peak_new,
			      (t_method)peak_free,
			      sizeof(t_peak), 0,
			      A_DEFFLOAT, 0);
    class_addmethod(peak_class, nullfn, gensym("signal"), 0);
    class_addmethod(peak_class, (t_method) peak_dsp, gensym("dsp"), A_CANT, 0);
    class_addbang(peak_class, peak_bang);
    class_addmethod(peak_class, (t_method)peak_ft1,
		    gensym("ft1"), A_FLOAT, 0);
}
