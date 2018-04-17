/* Copyright (c) 1997-1999 Miller Puckette.
* For information on usage and redistribution, and for a DISCLAIMER OF ALL
* WARRANTIES, see the file, "LICENSE.txt," in this distribution.  */

/* graphical inlets and outlets, both for control and signals.  */

/* This code is highly inefficient; messages actually have to be forwarded
by inlets and outlets.  The outlet is in even worse shape than the inlet;
in order to avoid having a "signal" method in the class, the oulet actually
sprouts an inlet, which forwards the message to the "outlet" object, which
sends it on to the outlet proper.  Another way to do it would be to have
separate classes for "signal" and "control" outlets, but this would complicate
life elsewhere. */

#include "m_pd.h"
#include "g_canvas.h"
#include <string.h>

void signal_setborrowed(t_signal *sig, t_signal *sig2);
void signal_makereusable(t_signal *sig);

/* ------------------------- vinlet -------------------------- */

t_class *vinlet_class;

typedef struct _vinlet{
    t_object x_obj;
    t_canvas *x_canvas;
    t_inlet *x_inlet;
    int x_bufsize;
    t_float *x_buf;         /* signal buffer; zero if not a signal */
    t_float *x_endbuf;
    t_float *x_fill;
    t_float *x_read;
    int x_hop;
  /* if not reblocking, the next slot communicates the parent's inlet
     signal from the prolog to the DSP routine: */
    t_signal *x_directsignal;

  t_resample x_updown;
} t_vinlet;

static void vinlet_bang(t_vinlet *x)
{
    outlet_bang(x->x_obj.ob_outlet);
}

static void vinlet_pointer(t_vinlet *x, t_gpointer *gp)
{
    outlet_pointer(x->x_obj.ob_outlet, gp);
}

static void vinlet_float(t_vinlet *x, t_float f)
{
    outlet_float(x->x_obj.ob_outlet, f);
}

static void vinlet_symbol(t_vinlet *x, t_symbol *s)
{
    outlet_symbol(x->x_obj.ob_outlet, s);
}

static void vinlet_list(t_vinlet *x, t_symbol *s, int argc, t_atom *argv)
{
    outlet_list(x->x_obj.ob_outlet, s, argc, argv);
}

static void vinlet_anything(t_vinlet *x, t_symbol *s, int argc, t_atom *argv)
{
    outlet_anything(x->x_obj.ob_outlet, s, argc, argv);
}

static void vinlet_free(t_vinlet *x)
{
    canvas_rminlet(x->x_canvas, x->x_inlet);
    if (x->x_buf)
        t_freebytes(x->x_buf, x->x_bufsize * sizeof(*x->x_buf));
    resample_free(&x->x_updown);
}

t_inlet *vinlet_getit(t_pd *x)
{
    if (pd_class(x) != vinlet_class) bug("vinlet_getit");
    return (((t_vinlet *)x)->x_inlet);
}

/* ------------------------- signal inlet -------------------------- */
int vinlet_issignal(t_vinlet *x)
{
    return (x->x_buf != 0);
}

t_int *vinlet_perform(t_int *w)
{
    t_vinlet *x = (t_vinlet *)(w[1]);
    t_float *out = (t_float *)(w[2]);
    int n = (int)(w[3]);
    t_float *in = x->x_read;
    while (n--) *out++ = *in++;
    if (in == x->x_endbuf) in = x->x_buf;
    x->x_read = in;
    return (w+4);
}

static void vinlet_dsp(t_vinlet *x, t_signal **sp)
{
    t_signal *outsig;
        /* no buffer means we're not a signal inlet */
    if (!x->x_buf)
        return;
    outsig = sp[0];
    if (x->x_directsignal)
    {
        signal_setborrowed(sp[0], x->x_directsignal);
    }
    else
    {
        dsp_add(vinlet_perform, 3, x, outsig->s_vec, outsig->s_vecsize);
        x->x_read = x->x_buf;
    }
}

    /* prolog code: loads buffer from parent patch */
t_int *vinlet_doprolog(t_int *w)
{
    t_vinlet *x = (t_vinlet *)(w[1]);
    t_float *in = (t_float *)(w[2]);
    int n = (int)(w[3]);
    t_float *out = x->x_fill;
    if (out == x->x_endbuf)
    {
      t_float *f1 = x->x_buf, *f2 = x->x_buf + x->x_hop;
        int nshift = x->x_bufsize - x->x_hop;
        out -= x->x_hop;
        while (nshift--) *f1++ = *f2++;
    }

    while (n--) *out++ = *in++;
    x->x_fill = out;
    return (w+4);
}

int inlet_getsignalindex(t_inlet *x);

        /* set up prolog DSP code  */
void vinlet_dspprolog(struct _vinlet *x, t_signal **parentsigs,
    int myvecsize, int calcsize, int phase, int period, int frequency,
    int downsample, int upsample,  int reblock, int switched)
{
    t_signal *insig, *outsig;
        /* no buffer means we're not a signal inlet */
    if (!x->x_buf)
        return;
    x->x_updown.downsample = downsample;
    x->x_updown.upsample   = upsample;

        /* if the "reblock" flag is set, arrange to copy data in from the
        parent. */
    if (reblock)
    {
        int parentvecsize, bufsize, oldbufsize, prologphase;
        int re_parentvecsize; /* resampled parentvectorsize */
            /* this should never happen: */
        if (!x->x_buf) return;

            /* the prolog code counts from 0 to period-1; the
            phase is backed up by one so that AFTER the prolog code
            runs, the "x_fill" phase is in sync with the "x_read" phase. */
        prologphase = (phase - 1) & (period - 1);
        if (parentsigs)
        {
            insig = parentsigs[inlet_getsignalindex(x->x_inlet)];
            parentvecsize = insig->s_vecsize;
            re_parentvecsize = parentvecsize * upsample / downsample;
        }
        else
        {
            insig = 0;
            parentvecsize = 1;
            re_parentvecsize = 1;
        }

        bufsize = re_parentvecsize;
        if (bufsize < myvecsize) bufsize = myvecsize;
        if (bufsize != (oldbufsize = x->x_bufsize))
        {
            t_float *buf = x->x_buf;
            t_freebytes(buf, oldbufsize * sizeof(*buf));
            buf = (t_float *)t_getbytes(bufsize * sizeof(*buf));
            memset((char *)buf, 0, bufsize * sizeof(*buf));
            x->x_bufsize = bufsize;
            x->x_endbuf = buf + bufsize;
            x->x_buf = buf;
        }
        if (parentsigs)
        {
            x->x_hop = period * re_parentvecsize;

            x->x_fill = x->x_endbuf -
              (x->x_hop - prologphase * re_parentvecsize);

            if (upsample * downsample == 1)
                    dsp_add(vinlet_doprolog, 3, x, insig->s_vec,
                        re_parentvecsize);
            else {
              int method = (x->x_updown.method == 3?
                (pd_compatibilitylevel < 44 ? 0 : 1) : x->x_updown.method);
              resamplefrom_dsp(&x->x_updown, insig->s_vec, parentvecsize,
                re_parentvecsize, method);
              dsp_add(vinlet_doprolog, 3, x, x->x_updown.s_vec,
                re_parentvecsize);
        }

            /* if the input signal's reference count is zero, we have
               to free it here because we didn't in ugen_doit(). */
            if (!insig->s_refcount)
                signal_makereusable(insig);
        }
        else memset((char *)(x->x_buf), 0, bufsize * sizeof(*x->x_buf));
        x->x_directsignal = 0;
    }
    else
    {
            /* no reblocking; in this case our output signal is "borrowed"
            and merely needs to be pointed to the real one. */
        x->x_directsignal = parentsigs[inlet_getsignalindex(x->x_inlet)];
    }
}

static void *vinlet_newsig(t_symbol *s)
{
    t_vinlet *x = (t_vinlet *)pd_new(vinlet_class);
    x->x_canvas = canvas_getcurrent();
    x->x_inlet = canvas_addinlet(x->x_canvas, &x->x_obj.ob_pd, &s_signal);
    x->x_endbuf = x->x_buf = (t_float *)getbytes(0);
    x->x_bufsize = 0;
    x->x_directsignal = 0;
    outlet_new(&x->x_obj, &s_signal);

    resample_init(&x->x_updown);

    /* this should be though over:
     * it might prove hard to provide consistency between labeled up- & downsampling methods
     * maybe indeces would be better...
     *
     * up till now we provide several upsampling methods and 1 single downsampling method (no filtering !)
     */
    if (s == gensym("hold"))
        x->x_updown.method=1;       /* up: sample and hold */
    else if (s == gensym("lin") || s == gensym("linear"))
        x->x_updown.method=2;       /* up: linear interpolation */
    else if (s == gensym("pad"))
        x->x_updown.method=0;       /* up: zero-padding */
    else x->x_updown.method=3;      /* sample/hold unless version<0.44 */

    return (x);
}

void inlet2_tilde_setup(void){
    vinlet_class = class_new(gensym("inlet2~"), (t_newmethod)vinlet_newsig,
        (t_method)vinlet_free, sizeof(t_vinlet), CLASS_NOINLET, A_DEFSYM, 0);
    class_addbang(vinlet_class, vinlet_bang);
    class_addpointer(vinlet_class, vinlet_pointer);
    class_addfloat(vinlet_class, vinlet_float);
    class_addsymbol(vinlet_class, vinlet_symbol);
    class_addlist(vinlet_class, vinlet_list);
    class_addanything(vinlet_class, vinlet_anything);
    class_addmethod(vinlet_class, (t_method)vinlet_dsp, gensym("dsp"), A_CANT, 0);
//    class_sethelpsymbol(vinlet_class, gensym("pd"));
}

void vinlet_tilde_setup(void){
    inlet2_tilde_setup();
}
