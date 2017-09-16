#include "m_pd.h"

#define IMPGEN_MAX_SIZE  256  

typedef struct _impgen
{
    t_object x_obj;
    int       x_nsamples;  // as used
    int       x_bufsize;   // as allocated
    t_float  *x_buffer;
    t_float   x_bufini[IMPGEN_MAX_SIZE];
    int       x_nleft;
    t_float  *x_head;
} t_impgen;

static t_class *impgen_class;

static void impgen_bang(t_impgen *x)
{
    x->x_nleft = x->x_nsamples;
    x->x_head = x->x_buffer;
}

static void impgen_set(t_impgen *x, t_symbol *s, int ac, t_atom *av)
{
    int i, nsamples = 0;
    t_atom *ap;
    t_float *bp;
    for (i = 0, ap = av; i < ac; i++, ap++)
    {
	if (ap->a_type == A_FLOAT) nsamples++;
	/* CHECKED no restrictions (refman's error about 0.0-1.0 range)
	   CHECKED nonnumeric atoms silently ignored */
    }
    if (nsamples > IMPGEN_MAX_SIZE)
        nsamples = IMPGEN_MAX_SIZE;
    if (nsamples)
    {
	x->x_nsamples = nsamples;
	bp = x->x_buffer;
	while (nsamples--) *bp++ = av++->a_w.w_float;
    }
    else x->x_nsamples = 0;  // CHECKED, needs to 'set 1' explicitly
    x->x_nleft = 0;
    x->x_head = x->x_buffer;
}

static t_int *impgen_perform(t_int *w)
{
    t_impgen *x = (t_impgen *)(w[1]);
    int nblock = (int)(w[2]);
    t_float *out = (t_float *)(w[3]);
    if (x->x_nleft){
        int nleft = x->x_nleft;
        t_float *head = x->x_head;
        if (nleft >= nblock){
            x->x_nleft -= nblock;
            while (nblock--) *out++ = *head++;
            x->x_head = head;
        }
        else{
            nblock -= nleft;
            while (nleft--)
                *out++ = *head++;
            while (nblock--)
                *out++ = 0.;
            x->x_nleft = 0;
            x->x_head = x->x_buffer;
        }
    }
    else
        while (nblock--)
            *out++ = 0.;
    return (w + 4);
}

static void impgen_dsp(t_impgen *x, t_signal **sp){
    dsp_add(impgen_perform, 3, x, sp[0]->s_n, sp[0]->s_vec);
}

static void impgen_free(t_impgen *x){
    if (x->x_buffer != x->x_bufini)
	freebytes(x->x_buffer, x->x_bufsize * sizeof(*x->x_buffer));
}

static void *impgen_new(t_symbol *s, int ac, t_atom *av){
    t_impgen *x = (t_impgen *)pd_new(impgen_class);
    x->x_nsamples = 1;  // CHECKED
    x->x_bufsize = IMPGEN_MAX_SIZE;
    x->x_buffer = x->x_bufini;
    x->x_buffer[0] = 1.;  // CHECKED
    x->x_nleft = 0;
    x->x_head = x->x_buffer;
    outlet_new((t_object *)x, &s_signal);
    if (ac) impgen_set(x, 0, ac, av);
    return (x);
}

void impgen_tilde_setup(void)
{
    impgen_class = class_new(gensym("impgen~"),
			    (t_newmethod)impgen_new,
			    (t_method)impgen_free,
			    sizeof(t_impgen), 0, A_GIMME, 0);
    class_domainsignalin(impgen_class, -1);
    class_addbang(impgen_class, impgen_bang);
    class_addmethod(impgen_class, (t_method)impgen_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(impgen_class, (t_method)impgen_set, gensym("set"), A_GIMME, 0);
}
