// based on msp's [env~] object: outputs both linear and dBFS rms

#include <m_pd.h>
#include "math.h"

#define MAXOVERLAP  32
#define INITVSTAKEN 64
#define LOGTEN      2.302585092994

typedef struct sigrms{
    t_object    x_obj;
    void       *x_outlet;
    void       *x_clock;
    t_sample   *x_hann;                 // a Hanning window
    t_float    *x_result;               // result to output
    int        *x_phase;                // number of points since last output
//    t_sample   x_sumbuf[MAXOVERLAP];   // summing buffer
    t_sample   *x_sumbuf; // summing buffer
    int         x_period;               // requested period of output
    int         x_realperiod;           // period rounded up to vecsize multiple
    int         x_npoints;              // analysis window size in samples
    int         x_allocforvs;           // extra buffer for DSP vector size
    int         x_nblock;               // block size
    int         x_nchs;
    int         x_db;
}t_sigrms;

t_class *rms_tilde_class;

static float pow2db(t_float f){
    if(f <= 0)
        return(-999);
    else if(f == 1)
        return(0);
    else{
        float val = log(f) * 10./LOGTEN ;
        return(val < -999 ? -999 : val);
    }
}

static void rms_tilde_tick(t_sigrms *x){ // clock callback function
    if(x->x_nchs == 1){
        float r = x->x_result[0];
        outlet_float(x->x_outlet, x->x_db ? pow2db(r) : sqrtf(r));
    }
    else{
        t_atom at[x->x_nchs];
        for(int j = 0; j < x->x_nchs; j++){
            float r = x->x_result[j];
            if(x->x_db)
                r =  pow2db(r);
            else
                r =  sqrtf(r);
            SETFLOAT(at+j, r);
        }
        outlet_list(x->x_outlet, &s_list, x->x_nchs, at);
    }
}

static void rms_set(t_sigrms *x, t_floatarg f1, t_floatarg f2){
    t_sample *buf;
    int i;
    int size = f1;
    if(size < 1) size = 1024;
    else if(size < x->x_nblock)
        size = x->x_nblock;
    int hop = f2;
    if(hop < 1)
        hop = size/2;
    if(hop < size / MAXOVERLAP + 1)
        hop = size / MAXOVERLAP + 1;
    if(hop < x->x_nblock)
        hop = x->x_nblock;
    if(!(buf = getbytes(sizeof(t_sample) * (size + INITVSTAKEN))))
        pd_error(x, "[rms~]: couldn't allocate buffer");
    x->x_hann = buf;
    for(int j = 0; j < x->x_nchs; j++)
        x->x_phase[j] = 0;
    x->x_npoints = size;
    x->x_period = hop;
// from dsp part 1
    if(x->x_period % x->x_nblock)
        x->x_realperiod = x->x_period + x->x_nblock - (x->x_period % x->x_nblock);
    else
        x->x_realperiod = x->x_period;
// buffer
    for(i = 0; i < x->x_nchs * MAXOVERLAP; i++)
        x->x_sumbuf[i] = 0;
    for(i = 0; i < size; i++)
        buf[i] = (1. - cos((2 * 3.14159 * i) / size))/size;
    for(; i < size+INITVSTAKEN; i++)
        buf[i] = 0;
}

static void rms_linear(t_sigrms *x, t_floatarg f){
    x->x_db = f == 0;
}

static t_int *rms_tilde_perform(t_int *w){
    t_sigrms *x = (t_sigrms *)(w[1]);
    t_sample *in = (t_sample *)(w[2]); // input
    int n = x->x_nblock; // block
    int count;
    int do_output = 0;
    t_sample *sump; // defined sum variable
    in += n;
    for(int j = 0; j < x->x_nchs; j++){
        for(count = x->x_phase[j],
            sump = x->x_sumbuf + j * MAXOVERLAP;
            count < x->x_npoints;
            count += x->x_realperiod, sump++){
            t_sample *hp = x->x_hann + count;
            t_sample *fp = in + j * n;
            t_sample sum = *sump;
            for(int i = 0; i < n; i++){
                fp--;
                sum += *hp++ * (*fp * *fp);
            }
            *sump = sum;
        }
        sump[0] = 0;
        x->x_phase[j] -= n;
        if(x->x_phase[j] < 0){
            x->x_result[j] = x->x_sumbuf[j * MAXOVERLAP];
            for(count = x->x_realperiod,
                sump = x->x_sumbuf + j * MAXOVERLAP;
                count < x->x_npoints;
                count += x->x_realperiod, sump++){
                sump[0] = sump[1];
            }
            sump[0] = 0;
            x->x_phase[j] = x->x_realperiod - n;
            do_output = 1;
        }
    }
    if(do_output)
        clock_delay(x->x_clock, 0L);
    return(w+3);
}

static void rms_tilde_dsp(t_sigrms *x, t_signal **sp){
    x->x_nblock = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    if(x->x_nchs != chs){
        x->x_phase = (int *)resizebytes(x->x_phase,
            x->x_nchs * sizeof(int), chs * sizeof(int));
        x->x_result = (t_float *)resizebytes(x->x_result,
            x->x_nchs * sizeof(t_float), chs * sizeof(t_float));
        x->x_sumbuf = (t_sample *)resizebytes(
            x->x_sumbuf,
            x->x_nchs * MAXOVERLAP * sizeof(*x->x_sumbuf),
            chs * MAXOVERLAP * sizeof(*x->x_sumbuf));
        x->x_nchs = chs;
        for(int j = 0; j < x->x_nchs; j++){
            x->x_phase[j] = 0;
            x->x_result[j] = 0.;
        }
        for(int i = x->x_nchs * MAXOVERLAP; i < chs * MAXOVERLAP; i++)
            x->x_sumbuf[i] = 0;
    }
    if(x->x_period % sp[0]->s_n)
        x->x_realperiod = x->x_period + sp[0]->s_n - (x->x_period % sp[0]->s_n);
    else
        x->x_realperiod = x->x_period;
    if(sp[0]->s_n > x->x_allocforvs){
        void *xx = resizebytes(x->x_hann,
            (x->x_npoints + x->x_allocforvs) * sizeof(t_sample),
            (x->x_npoints + sp[0]->s_n) * sizeof(t_sample));
        if(!xx){
            pd_error(x, "[rms~]: out of memory");
            return;
        }
        x->x_hann = (t_sample *)xx;
        x->x_allocforvs = sp[0]->s_n;
    }
    dsp_add(rms_tilde_perform, 2, x, sp[0]->s_vec);
}
                              
static void rms_tilde_free(t_sigrms *x){  // cleanup
    freebytes(x->x_phase, x->x_nchs * sizeof(*x->x_phase));
    freebytes(x->x_result, x->x_nchs * sizeof(*x->x_result));
    clock_free(x->x_clock);
    freebytes(x->x_hann, (x->x_npoints + x->x_allocforvs) * sizeof(*x->x_hann));
}

static void *rms_tilde_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_sigrms *x = (t_sigrms *)pd_new(rms_tilde_class);
    int npoints = 0, period = 0, dbstate = 1;
    x->x_nchs = 1;
    x->x_phase = (int *)getbytes(sizeof(*x->x_phase));
    x->x_phase[0] = 0;
    x->x_result = (t_float *)getbytes(sizeof(*x->x_result));
    x->x_result[0] = 0.;
    x->x_sumbuf = (t_sample *)getbytes(MAXOVERLAP * sizeof(*x->x_sumbuf));
    
/////////////////////////////////////////////////////////////////////////////////////
    int argnum = 0;
    while(ac > 0){
        if(av -> a_type == A_FLOAT){ //if current argument is a float
            t_float aval = atom_getfloatarg(0, ac, av);
            switch(argnum){
                case 0:
                    npoints = aval;
                    break;
                case 1:
                    period = aval;
                    break;
                default:
                    break;
            };
            argnum++;
            ac--, av++;
        }
        else if(av->a_type == A_SYMBOL && !argnum){
            if(atom_getsymbolarg(0, ac, av) == gensym("-lin")){
                dbstate = 0;
                ac--, av++;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    };
/////////////////////////////////////////////////////////////////////////////////////
    t_sample *buf;
    int i;
    if(npoints < 1)
        npoints = 1024;
    if(period < 1)
        period = npoints/2;
    if(period < npoints / MAXOVERLAP + 1)
        period = npoints / MAXOVERLAP + 1;
    if(!(buf = getbytes(sizeof(t_sample) * (npoints + INITVSTAKEN)))){
        pd_error(x, "[rms]: couldn't allocate buffer");
        return(NULL);
    }
    x->x_hann = buf;
    x->x_npoints = npoints;
    x->x_db = dbstate;
    x->x_period = period;
    x->x_nblock = 64;
    for(i = 0; i < MAXOVERLAP; i++)
        x->x_sumbuf[i] = 0;
    for(i = 0; i < npoints; i++)
        buf[i] = (1. - cos((2 * 3.14159 * i) / npoints))/npoints; // HANNING / npoints
    for(; i < npoints+INITVSTAKEN; i++)
        buf[i] = 0;
    x->x_clock = clock_new(x, (t_method)rms_tilde_tick);
    x->x_outlet = outlet_new(&x->x_obj, gensym("float"));
    x->x_allocforvs = INITVSTAKEN;
    return(x);
errstate:
    pd_error(x, "[rms~]: improper args");
    return(NULL);
}

void rms_tilde_setup(void ){
    rms_tilde_class = class_new(gensym("rms~"), (t_newmethod)rms_tilde_new,
        (t_method)rms_tilde_free, sizeof(t_sigrms), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(rms_tilde_class, nullfn, gensym("signal"), 0);
    class_addmethod(rms_tilde_class, (t_method)rms_tilde_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(rms_tilde_class, (t_method)rms_set, gensym("set"), A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(rms_tilde_class, (t_method)rms_linear, gensym("lin"), A_DEFFLOAT, 0);
}
