// porres

#include <math.h>
#include <stdlib.h>
#include <m_pd.h>

#define COMB_STACK  44100
#define COMB_DELAY  1000
#define COMB_MIND   1
#define COMB_MAXD   4294967294

#define COMB_MINMS  0.

#define COMB_DEFFF  0.
#define COMB_DEFFB  0.

static t_class *comb_class;

typedef struct _comb{
    t_object        x_obj;
    t_inlet        *x_dellet;
    t_inlet        *x_alet;
    t_inlet        *x_blet;
    t_inlet        *x_clet;
    t_outlet       *x_outlet;
    int             x_sr;
    int             x_gain;
    double         *x_ybuf;
    double          x_ffstack[COMB_STACK];
    double         *x_xbuf;
    double          x_fbstack[COMB_STACK];
    int             x_alloc;
    unsigned int    x_sz;
    t_float         x_maxdel;
    unsigned int   *x_wh;
    int             x_nchans;
    int             x_delchans;
    int             x_coeffchans;
}t_comb;

static void comb_clear(t_comb *x){
    for(int j = 0; j < x->x_nchans; j++){
        double *xbuf = x->x_xbuf + j*x->x_sz;
        double *ybuf = x->x_ybuf + j*x->x_sz;
        for(unsigned int i = 0; i < x->x_sz; i++)
            xbuf[i] = ybuf[i] = 0.;
        x->x_wh[j] = 0;
    }
}

static void comb_gain(t_comb *x, t_floatarg f1){
    x->x_gain = f1 != 0;
}

static void comb_sz(t_comb *x){
    unsigned int newsz = (unsigned int)ceil((double)x->x_maxdel*0.001*(double)x->x_sr);
    newsz++;
    if(newsz < 1)
        newsz = 1;
    else if(newsz > COMB_MAXD)
        newsz = COMB_MAXD;

    x->x_xbuf = (double *)resizebytes(x->x_xbuf,
        x->x_nchans*x->x_sz*sizeof(*x->x_xbuf),
        x->x_nchans*newsz*sizeof(*x->x_xbuf));
    x->x_ybuf = (double *)resizebytes(x->x_ybuf,
        x->x_nchans*x->x_sz*sizeof(*x->x_ybuf),
        x->x_nchans*newsz*sizeof(*x->x_ybuf));

    x->x_sz = newsz;
    comb_clear(x);
}

static double comb_getlin(double tab[], unsigned int sz, double idx){
    double output;
    unsigned int tabphase1 = (unsigned int)idx;
    unsigned int tabphase2 = tabphase1 + 1;
    double frac = idx - (double)tabphase1;
    if(tabphase1 >= sz - 1){
        tabphase1 = sz - 1;
        output = tab[tabphase1];
    }
    else if(tabphase1 < 0){
        tabphase1 = 0;
        output = tab[tabphase1];
    }
    else{
        double yb = tab[tabphase2];
        double ya = tab[tabphase1];
        output = ya+((yb-ya)*frac);
    };
    return(output);
}

static double comb_readmsdelay(t_comb *x, double arr[], t_float ms, unsigned int wh){
    double rh = (double)ms*((double)x->x_sr*0.001);
    if(rh < COMB_MIND)
        rh = COMB_MIND;
    rh = (double)wh+((double)x->x_sz-rh);
    while(rh >= x->x_sz)
        rh -= (double)x->x_sz;
    double output = comb_getlin(arr, x->x_sz, rh);
    return(output);
}

static t_int *comb_perform(t_int *w){
    t_comb *x = (t_comb *)(w[1]);
    int n = (int)(w[2]);
    t_float *input = (t_float *)(w[3]);
    t_float *din = (t_float *)(w[4]);
    t_float *coeffin = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);

    for(int j = 0; j < x->x_nchans; j++){
        t_float *in = input + j*n;
        t_float *delin = x->x_delchans == 1 ? din : din + j*n;
        t_float *coeff = x->x_coeffchans == 1 ? coeffin : coeffin + j*n;
        t_float *outch = out + j*n;
        double *xbuf = x->x_xbuf + j*x->x_sz;
        double *ybuf = x->x_ybuf + j*x->x_sz;
        unsigned int wh = x->x_wh[j];

        for(int i = 0; i < n; i++){
            double input_sample = (double)in[i];
            xbuf[wh] = input_sample;

            t_float freq = delin[i];
            if(freq > x->x_sr)
                freq = x->x_sr;

            t_float delms = freq <= 0 ? 0 : 1000 / freq;

            if(delms > x->x_maxdel)
                delms = x->x_maxdel;

            if(delms == 0)
                outch[i] = input_sample;
            else{
                t_float coef = coeff[i];

                if(!x->x_gain && coef != 0)
                    coef = copysign(exp(log(0.001) * delms/fabs(coef)), coef);

                double delx = comb_readmsdelay(x, xbuf, delms, wh);
                double dely = comb_readmsdelay(x, ybuf, delms, wh);
                double output = input_sample + (double)coef*delx + (double)coef*dely;

                ybuf[wh] = output;
                outch[i] = (t_float)output;
            };

            wh = (wh + 1) % x->x_sz;
        }

        x->x_wh[j] = wh;
    }

    return(w+7);
}

static void comb_dsp(t_comb *x, t_signal **sp){
    int sr = sp[0]->s_sr;
    x->x_delchans = sp[1]->s_nchans;
    x->x_coeffchans = sp[2]->s_nchans;

    if(sr != x->x_sr){
        x->x_sr = sr;
        comb_sz(x);
    };

    if(x->x_nchans != sp[0]->s_nchans){
        int oldn = x->x_nchans;
        int newn = sp[0]->s_nchans;

        x->x_xbuf = (double *)resizebytes(x->x_xbuf,
            oldn*x->x_sz*sizeof(*x->x_xbuf),
            newn*x->x_sz*sizeof(*x->x_xbuf));
        x->x_ybuf = (double *)resizebytes(x->x_ybuf,
            oldn*x->x_sz*sizeof(*x->x_ybuf),
            newn*x->x_sz*sizeof(*x->x_ybuf));
        x->x_wh = (unsigned int *)resizebytes(x->x_wh,
            oldn*sizeof(*x->x_wh),
            newn*sizeof(*x->x_wh));

        for(int j = oldn; j < newn; j++){
            x->x_wh[j] = 0;
            double *xbuf = x->x_xbuf + j*x->x_sz;
            double *ybuf = x->x_ybuf + j*x->x_sz;
            for(unsigned int i = 0; i < x->x_sz; i++)
                xbuf[i] = ybuf[i] = 0.;
        }

        x->x_nchans = newn;
    };

    signal_setmultiout(&sp[3], x->x_nchans);

    if((x->x_delchans > 1 && x->x_delchans != x->x_nchans)
    || (x->x_coeffchans > 1 && x->x_coeffchans != x->x_nchans)){
        dsp_add_zero(sp[3]->s_vec, x->x_nchans*sp[3]->s_n);
        pd_error(x, "[comb.filt~]: channel sizes mismatch");
        return;
    }

    dsp_add(comb_perform, 6, x, sp[0]->s_n,
        sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *comb_new(t_symbol *s, int argc, t_atom *argv){
    s = NULL;
    t_comb *x = (t_comb *)pd_new(comb_class);
    t_float init_hz = 0;
    t_float coeff = COMB_DEFFF;

    x->x_sr = sys_getsr();
    x->x_alloc = 1;
    x->x_gain = 0;
    x->x_sz = COMB_STACK;
    x->x_nchans = 1;
    x->x_delchans = 1;
    x->x_coeffchans = 1;

    x->x_xbuf = (double *)getbytes(x->x_sz*sizeof(*x->x_xbuf));
    x->x_ybuf = (double *)getbytes(x->x_sz*sizeof(*x->x_ybuf));
    x->x_wh = (unsigned int *)getbytes(sizeof(*x->x_wh));
    x->x_wh[0] = 0;
    comb_clear(x);

/////////////////////////////////////////////////////////////////////////////////
    int argnum = 0;
    while(argc > 0){
        if(argv->a_type == A_SYMBOL && !argnum){
            if(atom_getsymbolarg(0, argc, argv) == gensym("-gain")){
                x->x_gain = 1;
                argc--, argv++;
            }
            else
                goto errstate;
        }
        else if(argv->a_type == A_FLOAT){
            t_float argval = atom_getfloatarg(0, argc, argv);
            switch(argnum){
                case 0:
                    init_hz = (argval < 0 ? 0 : argval);
                    break;
                case 1:
                    coeff = argval;
                    break;
                case 2:
                    x->x_gain = argval != 0;
                    break;
                default:
                    break;
            };
            argc--, argv++;
            argnum++;
        }
        else
            goto errstate;
    };
/////////////////////////////////////////////////////////////////////////////////
    x->x_maxdel = COMB_DELAY;
    comb_sz(x);

    x->x_dellet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_dellet, init_hz);
    x->x_blet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_blet, coeff);
    x->x_outlet = outlet_new((t_object *)x, &s_signal);
    return(x);

errstate:
    pd_error(x, "[comb.filt~]: improper args");
    return(NULL);
}

static void *comb_free(t_comb *x){
    freebytes(x->x_xbuf, x->x_nchans*x->x_sz*sizeof(*x->x_xbuf));
    freebytes(x->x_ybuf, x->x_nchans*x->x_sz*sizeof(*x->x_ybuf));
    freebytes(x->x_wh, x->x_nchans*sizeof(*x->x_wh));
    inlet_free(x->x_dellet);
    inlet_free(x->x_blet);
    outlet_free(x->x_outlet);
    return(void *)x;
}

void setup_comb0x2efilt_tilde(void){
    comb_class = class_new(gensym("comb.filt~"), (t_newmethod)comb_new,
        (t_method)comb_free, sizeof(t_comb), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(comb_class, nullfn, gensym("signal"), 0);
    class_addmethod(comb_class, (t_method)comb_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(comb_class, (t_method)comb_clear, gensym("clear"), 0);
    class_addmethod(comb_class, (t_method)comb_gain, gensym("gain"), A_DEFFLOAT, 0);
}