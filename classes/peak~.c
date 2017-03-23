/* ---------------- peak~ - peakelope follower. ----------------- */
/* based on msp's peak~-object: outputs both linear and dBFS peak */

#include "m_pd.h"
#include "math.h"

#define MAXOVERLAP 32
#define INITVSTAKEN 64

typedef struct sigpeak
{
    t_object x_obj;                 /* header */
    void *x_outlet;                 /* a "float" outlet */
    void *x_clock;                  /* a "clock" object */
    t_sample *x_buf;                   /* a Hanning window */
    int x_phase;                    /* number of points since last output */
    int x_period;                   /* requested period of output */
    int x_realperiod;               /* period rounded up to vecsize multiple */
    int x_npoints;                  /* analysis window size in samples */
    t_float x_result;                 /* result to output */
    t_sample x_sumbuf[MAXOVERLAP];     /* summing buffer */
    int x_allocforvs;               /* extra buffer for DSP vector size */
    int x_block; // block size
    t_float   x_value;
} t_sigpeak;

t_class *peak_tilde_class;
static void peak_tilde_tick(t_sigpeak *x);


static void *peak_tilde_new(t_floatarg fnpoints, t_floatarg fperiod)
{
    int npoints = fnpoints;
    int period = fperiod;
    t_sigpeak *x;
    t_sample *buf;
    int i;
    
    if (npoints < 1) npoints = 1024;
    if (period < 1) period = npoints/2;
    if (period < npoints / MAXOVERLAP + 1)
        period = npoints / MAXOVERLAP + 1;
    if (!(buf = getbytes(sizeof(t_sample) * (npoints + INITVSTAKEN))))
    {
        error("peak: couldn't allocate buffer");
        return (0);
    }
    x = (t_sigpeak *)pd_new(peak_tilde_class);
    x->x_buf = buf;
    x->x_npoints = npoints;
    x->x_phase = 0;
    x->x_value = 0.;
    x->x_period = period;
    for (i = 0; i < MAXOVERLAP; i++) x->x_sumbuf[i] = 0;
    for (i = 0; i < npoints; i++)
        buf[i] = (1. - cos((2 * 3.14159 * i) / npoints))/npoints; // HANNING / npoints
    for (; i < npoints+INITVSTAKEN; i++) buf[i] = 0;
    x->x_clock = clock_new(x, (t_method)peak_tilde_tick);
    x->x_outlet = outlet_new(&x->x_obj, gensym("float"));
    x->x_allocforvs = INITVSTAKEN;
    return (x);
}

static t_int *peak_tilde_perform(t_int *w)
{
    t_sigpeak *x = (t_sigpeak *)(w[1]);
    t_sample *in = (t_sample *)(w[2]); // input
    int n = (int)(w[3]); // block
    int count;
    t_float p = x->x_value; // 'p' for 'peak'
    t_sample *sump; // defined sum variable
    in += n;
    for (count = x->x_phase, sump = x->x_sumbuf; // sum it up
         count < x->x_npoints; count += x->x_realperiod, sump++)
        {
            t_sample *hp = x->x_buf + count;
            t_sample *fp = in;
            t_sample sum = *sump;
            int i;
        
            for (i = 0; i < n; i++)
                {
                fp--;
                sum += *hp++ * (*fp * *fp); // sum = hp * inˆ2
                }
        *sump = sum; // sum
        }
    sump[0] = 0;
    x->x_phase -= n;
    if (x->x_phase < 0) // get result and reset
        {
//        x->x_result = x->x_sumbuf[0];
          x->x_result = 12;
        for (count = x->x_realperiod, sump = x->x_sumbuf;
             count < x->x_npoints; count += x->x_realperiod, sump++)
            sump[0] = sump[1];
            sump[0] = 0;
            x->x_phase = x->x_realperiod - n;
            clock_delay(x->x_clock, 0L); // output?
        }
    
    
    /* { if (*fp > p) p = *fp;
     else if (*fp < -p) p = *fp * -1; } */
    
    x->x_value = p;
    return (w+4);
}

static void peak_tilde_dsp(t_sigpeak *x, t_signal **sp)
{
   x->x_block = sp[0]->s_n;
    if (x->x_period % sp[0]->s_n) x->x_realperiod =
        x->x_period + sp[0]->s_n - (x->x_period % sp[0]->s_n);
    else x->x_realperiod = x->x_period;
    if (sp[0]->s_n > x->x_allocforvs)
        {
        void *xx = resizebytes(x->x_buf,
            (x->x_npoints + x->x_allocforvs) * sizeof(t_sample),
            (x->x_npoints + sp[0]->s_n) * sizeof(t_sample));
        if (!xx)
            {
            error("peak~: out of memory");
            return;
            }
        x->x_buf = (t_sample *)xx;
        x->x_allocforvs = sp[0]->s_n;
        }
    dsp_add(peak_tilde_perform, 3, x, sp[0]->s_vec, sp[0]->s_n);
}

static void peak_tilde_tick(t_sigpeak *x) // clock callback function
{
    outlet_float(x->x_outlet, x->x_result);
}

static void peak_tilde_free(t_sigpeak *x)  // cleanup
{
    clock_free(x->x_clock);
    freebytes(x->x_buf, (x->x_npoints + x->x_allocforvs) * sizeof(*x->x_buf));
}


void peak_tilde_setup(void )
{
    peak_tilde_class = class_new(gensym("peak~"), (t_newmethod)peak_tilde_new,
                                (t_method)peak_tilde_free, sizeof(t_sigpeak), 0, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(peak_tilde_class, nullfn, gensym("signal"), 0);
    class_addmethod(peak_tilde_class, (t_method)peak_tilde_dsp, gensym("dsp"), A_CANT, 0);
}
