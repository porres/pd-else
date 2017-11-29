

#include "m_pd.h"

#define float2sig_MAX_SIZE  1

typedef struct _float2sigseg
{
    float  s_target;
    float  s_delta;
} t_float2sigseg;

typedef struct _float2sig
{
    t_object x_obj;
    float       x_value;
    float       x_target;
    float       x_delta;
    int         x_deltaset;
    float       x_inc;
    float       x_biginc;
    float       x_ksr;
    int         x_nleft;
    int         x_retarget;
    int         x_size;   /* as allocated */
    int         x_nsegs;  /* as used */
    int         x_pause;
    t_float2sigseg  *x_curseg;
    t_float2sigseg  *x_segs;
    t_float2sigseg   x_segini[float2sig_MAX_SIZE];
} t_float2sig;

static t_class *float2sig_class;


static t_int *float2sig_perform(t_int *w)
{
    t_float2sig *x = (t_float2sig *)(w[1]);
    t_float *out = (t_float *)(w[2]);
    int nblock = (int)(w[3]);
    int nxfer = x->x_nleft;
    float curval = x->x_value;
    float inc = x->x_inc;
    float biginc = x->x_biginc;
    
    if (x->x_pause)
    {
	while (nblock--) *out++ = curval;
	return (w + 4);
    }
    if (PD_BIGORSMALL(curval))  /* LATER rethink */
	curval = x->x_value = 0;
retarget:
    if (x->x_retarget)
    {
	float target = x->x_curseg->s_target;
	float delta = x->x_curseg->s_delta;
    	int npoints = delta * x->x_ksr + 0.5;  /* LATER rethink */
	x->x_nsegs--;
	x->x_curseg++;
    	while (npoints <= 0)
	{
	    curval = x->x_value = target;
	    if (x->x_nsegs)
	    {
		target = x->x_curseg->s_target;
		delta = x->x_curseg->s_delta;
		npoints = delta * x->x_ksr + 0.5;  /* LATER rethink */
		x->x_nsegs--;
		x->x_curseg++;
	    }
	    else
	    {
		while (nblock--) *out++ = curval;
		x->x_nleft = 0;
		clock_delay(x->x_clock, 0);
		x->x_retarget = 0;
		return (w + 4);
	    }
	}
    	nxfer = x->x_nleft = npoints;
    	inc = x->x_inc = (target - x->x_value) / (float)npoints;
	x->x_biginc = (int)(w[3]) * inc;
	biginc = nblock * inc;
	x->x_target = target;
    	x->x_retarget = 0;
    }
    if (nxfer >= nblock)
    {
	if ((x->x_nleft -= nblock) == 0)
	{
	    if (x->x_nsegs) x->x_retarget = 1;
	    else
	    {
		clock_delay(x->x_clock, 0);
	    }
	    x->x_value = x->x_target;
	}
	else x->x_value += biginc;
    	while (nblock--)
	    *out++ = curval, curval += inc;
    }
    else if (nxfer > 0)
    {
	nblock -= nxfer;
	do
	    *out++ = curval, curval += inc;
	while (--nxfer);
	curval = x->x_value = x->x_target;
	if (x->x_nsegs)
	{
	    x->x_retarget = 1;
	    goto retarget;
	}
	else
	{
	    while (nblock--) *out++ = curval;
	    x->x_nleft = 0;
	    clock_delay(x->x_clock, 0);
	}
    }
    else while (nblock--) *out++ = curval;
    return (w + 4);
}

static void float2sig_float(t_float2sig *x, t_float f)
{
    if (x->x_deltaset)
    {
    	x->x_deltaset = 0;
    	x->x_target = f;
	x->x_nsegs = 1;
	x->x_curseg = x->x_segs;
	x->x_curseg->s_target = f;
	x->x_curseg->s_delta = x->x_delta;
    	x->x_retarget = 1;
    }
    else
    {
    	x->x_value = x->x_target = f;
	x->x_nsegs = 0;
	x->x_curseg = 0;
    	x->x_nleft = 0;
	x->x_retarget = 0;
    }
}

static void float2sig_ft1(t_float2sig *x, t_floatarg f)
{
    x->x_delta = f;
    x->x_deltaset = (f > 0);
}

static void float2sig_dsp(t_float2sig *x, t_signal **sp)
{
    dsp_add(float2sig_perform, 3, x, sp[0]->s_vec, sp[0]->s_n);
    x->x_ksr = sp[0]->s_sr * 0.001;
}

static void float2sig_free(t_float2sig *x)
{
    if (x->x_segs != x->x_segini)
	freebytes(x->x_segs, x->x_size * sizeof(*x->x_segs));
    if (x->x_clock) clock_free(x->x_clock);
}

static void *float2sig_new(t_floatarg f)
{
    t_float2sig *x = (t_float2sig *)pd_new(float2sig_class);
    x->x_value = x->x_target = f;
    x->x_deltaset = 0;
    x->x_nleft = 0;
    x->x_retarget = 0;
    x->x_size = float2sig_MAX_SIZE;
    x->x_nsegs = 0;
    x->x_segs = x->x_segini;
    x->x_curseg = 0;
    inlet_new((t_object *)x, (t_pd *)x, &s_float, gensym("ft1"));
    outlet_new((t_object *)x, &s_signal);
    return (x);
}

void float2sig_tilde_setup(void)
{
    float2sig_class = class_new(gensym("float2sig~"),
        (t_newmethod)float2sig_new, (t_method)float2sig_free, sizeof(t_float2sig), 0, A_DEFFLOAT, 0);
    class_domainsignalin(float2sig_class, -1);
    class_addmethod(float2sig_class, (t_method)float2sig_dsp, gensym("dsp"), A_CANT, 0);
    class_addfloat(float2sig_class, float2sig_float);
    class_addmethod(float2sig_class, (t_method)float2sig_ft1, gensym("ft1"), A_FLOAT, 0);
}
