
#include <stdlib.h>
#include <math.h>
#include "m_pd.h"

#define RMS_STACK    44100 //stack value
#define RMS_MAXBUF  882000 //max buffer size
#define RMS_DEFNPOINTS  1024

typedef struct _rms
{
    t_object    x_obj;
    t_inlet    *x_inlet1;
    int         x_mode;
    double      (*x_sumfn)(double, double, int);
    unsigned int         x_count; //number of samples seen so far, will no go beyond x_npoints + 1
    unsigned int         x_npoints; //number of samples for moving rms
    double     x_accum; //sum
    double      *x_buf; //buffer pointer
    double     x_stack[RMS_STACK]; //buffer
    int         x_alloc; //if x_buf is allocated or stack
    unsigned int    x_sz; //allocated size for x_buf
    unsigned int    x_bufrd; //readhead for buffer
    unsigned int         x_max; //max size of buffer as specified by argt
} t_rms;

static t_class *rms_class;

static void rms_zerobuf(t_rms *x)
{
    unsigned int i;
    for(i=0; i < x->x_sz; i++)
    {
        x->x_buf[i] = 0.;
    };
}

static void rms_reset(t_rms * x)
{
    x->x_count = 0;
    x->x_accum = 0;
    x->x_bufrd = 0;
    rms_zerobuf(x);
};

static void rms_sz(t_rms *x, unsigned int newsz){
    //helper function to deal with allocation issues if needed
    
    int alloc = x->x_alloc;
    unsigned int cursz = x->x_sz; //current size

    //requested size
    if(newsz < 0){
        newsz = 0;
    }
    else if(newsz > RMS_MAXBUF){
        newsz = RMS_MAXBUF;
    };
    if(!alloc && newsz > RMS_STACK){
        x->x_buf = (double *)malloc(sizeof(double)*newsz);
        x->x_alloc = 1;
        x->x_sz = newsz;
    }
    else if(alloc && newsz > cursz){
        x->x_buf = (double *)realloc(x->x_buf, sizeof(double)*newsz);
        x->x_sz = newsz;
    }
    else if(alloc && newsz < RMS_STACK){
        free(x->x_buf);
        x->x_sz = RMS_STACK;
        x->x_buf = x->x_stack;
        x->x_alloc = 0;
    };
    rms_reset(x);
}


static double rms_sum(double input, double accum, int add)
{
    if(add){
        accum += (input * input);
    }
    else{
        accum -= (input * input);
    };
    return (accum);
}


static void rms_float(t_rms *x, t_float f)
{
    unsigned int i = (unsigned int)f;  /* CHECKME noninteger */
    if (i > 0)  /* CHECKME */
    {
        //clip at max
        if(i >= x->x_max){
            i = x->x_max;
        };

	x->x_npoints = i;
        rms_reset(x);
    }
}

static t_int *rms_perform(t_int *w)
{
    t_rms *x = (t_rms *)(w[1]);
    int nblock = (int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    double (*sumfn)(double, double, int) = rms_sum;
    int i;
    unsigned int npoints = x->x_npoints;
    for(i=0; i <nblock; i++)
    {
        double result; //eventual result
        double input = (double)in[i];
        if(npoints > 1)
            {
            unsigned int bufrd = x->x_bufrd;
            //add input to accumulator
            x->x_accum = (*sumfn)(input, x->x_accum, 1);
            unsigned int count = x->x_count;
            if(count < npoints)
                {
                count++;
                x->x_count = count;
                }
            else
                {
                x->x_accum = (*sumfn)(x->x_buf[bufrd], x->x_accum, 0);

                };
            //overwrite/store current input value into buf
            x->x_buf[bufrd] = input;
 
            //calculate result
            result = sqrt(x->x_accum/(double)npoints);
            
            //incrementation step
            bufrd++;
            if(bufrd >= npoints)
                {
                bufrd = 0;
                };
            x->x_bufrd = bufrd;

            }
        else{
            //npoints = 1, just pass through (or absolute){
                result = fabs(input);
            };
        out[i] = result;
    };
    return (w + 5);
}

static void rms_dsp(t_rms *x, t_signal **sp)
{
    dsp_add(rms_perform, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
}

static void rms_free(t_rms *x)
{
    if(x->x_alloc)
    {
        free(x->x_buf);
    };
}

static void *rms_new(t_floatarg f)
{
    t_rms *x = (t_rms *)pd_new(rms_class);
    int maxbuf = f;
    x->x_npoints = (maxbuf > 0 ? maxbuf : RMS_DEFNPOINTS);
    //default to stack for now...
    x->x_buf = x->x_stack; 
    x->x_alloc = 0;
    x->x_sz = RMS_STACK;
    x->x_max = x->x_npoints; //designated max of rms buffer
    //now allocate x_buf if necessary
    rms_sz(x, x->x_npoints);
    /* CHECKME if not x->x_phase = 0 */
    outlet_new((t_object *)x, &s_signal);
    return (x);
}

void rms_tilde_setup(void)
{
    rms_class = class_new(gensym("rms~"), (t_newmethod)rms_new,
            (t_method)rms_free, sizeof(t_rms), 0, A_DEFFLOAT, 0);
    class_addmethod(rms_class, (t_method) rms_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(rms_class, nullfn, gensym("signal"), 0);
    class_addmethod(rms_class, (t_method)rms_reset, gensym("reset"), 0);
    class_addfloat(rms_class, (t_method)rms_float);
}
