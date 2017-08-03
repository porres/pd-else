// Porres 2017
// Adapted from cyclone's allpass

#include <math.h>
#include <stdlib.h>
#include "m_pd.h"

#define apass2_STACK 48000 // stack buf size, 1 sec at 48k
#define apass2_MAXD 4294967294 // max delay = 2**32 - 2

static t_class *apass2_class;

typedef struct _apass2
{
    t_object        x_obj;
    t_inlet         *x_dellet;
    t_inlet         *x_alet;
    t_outlet        *x_outlet;
    int             x_sr;
// pointers to the delay bufs
    double          * x_ybuf;
    double          x_ffstack[apass2_STACK];
    double          * x_xbuf;
    double          x_fbstack[apass2_STACK];
    int             x_alloc; // if we are using allocated bufs
    unsigned int    x_sz; // actual size of each delay buffer
    t_float         x_maxdel;  // maximum delay in ms
    unsigned int    x_wh;     // writehead
} t_apass2;

static void apass2_clear(t_apass2 *x){
    unsigned int i;
    for(i=0; i<x->x_sz; i++){
        x->x_xbuf[i] = 0.;
        x->x_ybuf[i] = 0.;
    };
    x->x_wh = 0;
}

static void apass2_sz(t_apass2 *x){
    // helper function to deal with allocation issues if needed
    // ie if wanted size x->x_maxdel is bigger than stack, allocate
    
    // convert ms to samps
    unsigned int newsz = (unsigned int)ceil((double)x->x_maxdel*0.001*(double)x->x_sr);
    newsz++; // add a sample for good measure since say bufsize is 2048 and
    // you want a delay of 2048 samples,.. problem!
    
    int alloc = x->x_alloc;
    unsigned int cursz = x->x_sz; //current size
    
    if(newsz < 0)
        newsz = 0;
    else if(newsz > apass2_MAXD)
        newsz = apass2_MAXD;
    if(!alloc && newsz > apass2_STACK){
        x->x_xbuf = (double *)malloc(sizeof(double)*newsz);
        x->x_ybuf = (double *)malloc(sizeof(double)*newsz);
        x->x_alloc = 1;
        x->x_sz = newsz;
    }
    else if(alloc && newsz > cursz){
        x->x_xbuf = (double *)realloc(x->x_xbuf, sizeof(double)*newsz);
        x->x_ybuf = (double *)realloc(x->x_ybuf, sizeof(double)*newsz);
        x->x_sz = newsz;
    }
    else if(alloc && newsz < apass2_STACK){
        free(x->x_xbuf);
        free(x->x_ybuf);
        x->x_sz = apass2_STACK;
        x->x_xbuf = x->x_ffstack;
        x->x_ybuf = x->x_fbstack;
        x->x_alloc = 0;
    };
    apass2_clear(x);
}

static double apass2_getlin(double tab[], unsigned int sz, double idx){
    // linear interpolated reader, copied from Derek Kwan's library
    double output;
    unsigned int tabphase1 = (unsigned int)idx;
    unsigned int tabphase2 = tabphase1 + 1;
    double frac = idx - (double)tabphase1;
    if(tabphase1 >= sz - 1){
        tabphase1 = sz - 1; // checking to see if index falls within bounds
        output = tab[tabphase1];
    }
    else if(tabphase1 < 0){
        tabphase1 = 0;
        output = tab[tabphase1];
    }
    else{
        double yb = tab[tabphase2]; // linear interp
        double ya = tab[tabphase1];
        output = ya+((yb-ya)*frac);
    };
    return output;
}

static double apass2_readmsdelay(t_apass2 *x, double arr[], t_float ms){
    //helper func, basically take desired ms delay, convert to samp, read from arr[]
    
    //eventual reading head
    double rh = (double)ms*((double)x->x_sr*0.001); //converting ms to samples
    //bounds checking for minimum delay in samples
    if(rh < 0)
        rh = 0;
    rh = (double)x->x_wh+((double)x->x_sz-rh); //essentially subracting from writehead to find proper position in buffer
    //wrapping into length of delay buffer
    while(rh >= x->x_sz){
        rh -= (double)x->x_sz;
    };
    //now to read from the buffer!
    double output = apass2_getlin(arr, x->x_sz, rh);
    return output;
}

static t_int *apass2_perform(t_int *w){
    t_apass2 *x = (t_apass2 *)(w[1]);
    int n = (int)(w[2]);
    t_float *xin = (t_float *)(w[3]);
    t_float *din = (t_float *)(w[4]);
    t_float *ain = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);
    int i;
    for(i=0; i<n;i++){
        int wh = x->x_wh;
        double input = (double)xin[i];
        double output;
// first off, write input to delay buf
        x->x_xbuf[wh] = input;
// get delayed values of x and y
        t_float delms = din[i];
// bounds checking
        if(delms < 0)
            delms = 0;
        else if(delms > x->x_maxdel)
            delms = x->x_maxdel;
// now get those delayed vals
        double delx = apass2_readmsdelay(x, x->x_xbuf, delms);
        double dely = apass2_readmsdelay(x, x->x_ybuf, delms);
        if (ain[i] == 0)
            ain[i] = 0;
        else
            ain[i] = copysign(exp(log(0.001) * delms/fabs(ain[i])), ain[i]);
// y[n] = -a * x[n] + x[n-d] + a * y[n-d]
        if (delms == 0)
            output = input;
        else
            output = (double)ain[i]*-1.*input + delx + (double)ain[i]*dely;
        x->x_ybuf[wh] = output;
        out[i] = output;
        x->x_wh = (wh + 1) % x->x_sz; // increment writehead
    };
    return (w + 7);
}

static void apass2_dsp(t_apass2 *x, t_signal **sp)
{
    int sr = sp[0]->s_sr;
    if(sr != x->x_sr){
// if new sample rate isn't old sample rate, need to realloc
        x->x_sr = sr;
        apass2_sz(x);
    };
    dsp_add(apass2_perform, 6, x, sp[0]->s_n, sp[0]->s_vec,
        sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void apass2_size(t_apass2 *x, t_floatarg f1){
    if(f1 < 0)
        f1 = 0;
    x->x_maxdel = f1;
    apass2_sz(x);
}

static void *apass2_new(t_floatarg f1, t_floatarg f2){
    t_apass2 *x = (t_apass2 *)pd_new(apass2_class);
    x->x_sr = sys_getsr();
    x->x_alloc = 0;
    x->x_sz = apass2_STACK;
// clear out stack bufs, set pointer to stack
    x->x_ybuf = x->x_fbstack;
    x->x_xbuf = x->x_ffstack;
    apass2_clear(x);
    if (f1 < 0)
        f1 = 0;
    x->x_maxdel = f1;
// ship off to the helper method to deal with allocation if necessary
    apass2_sz(x);
// boundschecking
// this is 1/44.1 (1/(sr*0.001) rounded up, good enough?
    
// inlets / outlet
    x->x_dellet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_dellet, f1);
    x->x_alet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_alet, f2);
    x->x_outlet = outlet_new((t_object *)x, &s_signal);
    return (x);
}

static void * apass2_free(t_apass2 *x){
    if(x->x_alloc){
        free(x->x_xbuf);
        free(x->x_ybuf);
    };
    inlet_free(x->x_dellet);
    inlet_free(x->x_alet);
    outlet_free(x->x_outlet);
    return (void *)x;
}

void apass2_tilde_setup(void)
{
    apass2_class = class_new(gensym("apass2~"), (t_newmethod)apass2_new,
        (t_method)apass2_free, sizeof(t_apass2), 0, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(apass2_class, nullfn, gensym("signal"), 0);
    class_addmethod(apass2_class, (t_method)apass2_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(apass2_class, (t_method)apass2_clear, gensym("clear"), 0);
    class_addmethod(apass2_class, (t_method)apass2_size, gensym("size"), A_DEFFLOAT, 0);
}
