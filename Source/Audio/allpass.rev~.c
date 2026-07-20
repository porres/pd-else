// Porres 2017 - 2026

#include <math.h>
#include <stdlib.h>
#include <m_pd.h>

#define ALLPASS_REV_MAXD 4294967294 // max delay = 2**32 - 2

static t_class *allpass_rev_class;

typedef struct _allpass_rev{
    t_object        x_obj;
    t_inlet         *x_dellet;
    t_inlet         *x_alet;
    t_outlet        *x_outlet;
    int             x_sr;
    int             x_gain;
    double         *x_xbuf;
    double         *x_ybuf;
    unsigned int    x_sz;
    t_float         x_maxdel;
    unsigned int    *x_wh;   // changed
    int             x_n;
    int             x_nchans;
}t_allpass_rev;

static void allpass_rev_clear(t_allpass_rev *x){
    for(unsigned int i = 0; i < x->x_sz * x->x_nchans; i++){
        x->x_xbuf[i] = 0.;
        x->x_ybuf[i] = 0.;
    }
    for(int ch = 0; ch < x->x_nchans; ch++)
        x->x_wh[ch] = 0;
}

static void allpass_rev_sz(t_allpass_rev *x){
    unsigned int newsz = (unsigned int)ceil((double)x->x_maxdel * x->x_sr * 0.001) + 1;
    if(newsz < 64)
        newsz = 64;
    else if(newsz > ALLPASS_REV_MAXD)
        newsz = ALLPASS_REV_MAXD;
    x->x_xbuf = (double *)realloc(x->x_xbuf,
        sizeof(double) * newsz * x->x_nchans);
    x->x_ybuf = (double *)realloc(x->x_ybuf,
        sizeof(double) * newsz * x->x_nchans);
    x->x_sz = newsz;
    x->x_maxdel = (double)newsz / ((double)x->x_sr * 0.001);
    allpass_rev_clear(x);
}

static double allpass_rev_getlin(double tab[], unsigned int sz, double idx){
    // linear interpolated reader, copied from Derek Kwan's library
    double output;
    unsigned int tabphase1 = (unsigned int)idx;
    unsigned int tabphase2 = tabphase1 + 1;
    double frac = idx - (double)tabphase1;
    if(tabphase1 >= sz - 1){
        tabphase1 = sz - 1; // checking to see if index falls within bounds
        output = tab[tabphase1];
    }
    else{
        double yb = tab[tabphase2]; // linear interp
        double ya = tab[tabphase1];
        output = ya+((yb-ya)*frac);
    };
    return output;
}

static t_int *allpass_rev_perform(t_int *w){
    t_allpass_rev *x = (t_allpass_rev *)(w[1]);
    t_float *xin = (t_float *)(w[2]);
    t_float *din = (t_float *)(w[3]);
    t_float *ain = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    int n = x->x_n;
    for(int ch = 0; ch < x->x_nchans; ch++){
        unsigned int offset = ch * x->x_sz;
        t_float *in_ch = xin + ch * n;
        t_float *out_ch = out + ch * n;
        for(int i = 0; i < n; i++){
            unsigned int wh = x->x_wh[ch];
            double input = in_ch[i];
            x->x_xbuf[offset + wh] = input;
            t_float delms = din[i];
            if(delms < 0)
                delms = 0;
            else if(delms > x->x_maxdel)
                delms = x->x_maxdel;
            double rh = wh + x->x_sz -
                delms * x->x_sr * 0.001;
            while(rh >= x->x_sz)
                rh -= x->x_sz;
            double delx = allpass_rev_getlin(
                x->x_xbuf + offset, x->x_sz, rh);
            double dely = allpass_rev_getlin(
                x->x_ybuf + offset, x->x_sz, rh);
            double a = ain[i];
            if(!x->x_gain){
                if(a != 0)
                    a = copysign(exp(log(0.001) *
                    delms / fabs(a)), a);
            }
            double output;
            if(delms == 0)
                output = input;
            else
                output = -a * input + delx + a * dely;
            x->x_ybuf[offset + wh] = output;
            out_ch[i] = output;
            x->x_wh[ch] = (wh + 1) % x->x_sz;
        }
    }
    return(w+6);
}

static void allpass_rev_dsp(t_allpass_rev *x, t_signal **sp){
    int sr = sp[0]->s_sr;
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    if(chs != x->x_nchans){
        x->x_wh = (unsigned int *)resizebytes(x->x_wh,
            x->x_nchans * sizeof(unsigned int),
            chs * sizeof(unsigned int));
        x->x_nchans = chs;
        allpass_rev_sz(x);
    }
    signal_setmultiout(&sp[3], x->x_nchans);
    if(sp[1]->s_nchans > 1 || sp[2]->s_nchans > 1){
        dsp_add_zero(sp[3]->s_vec, x->x_nchans * x->x_n);
        pd_error(x, "[allpass.rev~]: secondary inlets must be mono");
        return;
    }
    if(sr != x->x_sr){
        x->x_sr = sr;
        allpass_rev_sz(x);
    }
    dsp_add(allpass_rev_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void allpass_rev_size(t_allpass_rev *x, t_floatarg f1){
    if(f1 < 0)
        f1 = 0;
    x->x_maxdel = f1;
    allpass_rev_sz(x);
}

static void allpass_rev_gain(t_allpass_rev *x, t_floatarg f1){
    x->x_gain = f1 != 0;
}

static void *allpass_rev_new(t_symbol *s, int argc, t_atom *argv){
    (void)s;
    t_allpass_rev *x = (t_allpass_rev *)pd_new(allpass_rev_class);
    x->x_sr = sys_getsr();
    x->x_gain = 0;
    x->x_sz = 0;
    x->x_xbuf = NULL;
    x->x_ybuf = NULL;
    x->x_n = 64;
    x->x_nchans = 1;
    x->x_wh = (unsigned int *)getbytes(sizeof(*x->x_wh));
/////////////////////////////////////////////////////////////////////////////////////
    float init_maxdelay = 0.0f;
    float init_coeff = 0.0f;
    int argnum = 0;
    while(argc > 0){
        if(argv -> a_type == A_FLOAT){ //if current argument is a float
            t_float argval = atom_getfloatarg(0, argc, argv);
            switch(argnum){
                case 0:
                    init_maxdelay = argval;
                    break;
                case 1:
                    init_coeff = argval;
                    break;
                case 2:
                    x->x_gain = argval != 0;
                    break;
                default:
                    break;
            };
            argnum++;
            argc--;
            argv++;
        }
        else
            goto errstate;
    }
/////////////////////////////////////////////////////////////////////////////////////
    if (init_maxdelay < 0)
        init_maxdelay = 0;
    x->x_maxdel = init_maxdelay;
// ship off to the helper method to deal with allocation if necessary
    allpass_rev_sz(x);
// boundschecking
// this is 1/44.1 (1/(sr*0.001) rounded up, good enough?
    
// inlets / outlet
    x->x_dellet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_dellet, init_maxdelay);
    x->x_alet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_alet, init_coeff);
    x->x_outlet = outlet_new((t_object *)x, &s_signal);
    return(x);
errstate:
    pd_error(x, "allpass.rev~: improper args");
    return NULL;
}

static void * allpass_rev_free(t_allpass_rev *x){
    free(x->x_xbuf);
    free(x->x_ybuf);
    inlet_free(x->x_dellet);
    inlet_free(x->x_alet);
    outlet_free(x->x_outlet);
    freebytes(x->x_wh, x->x_nchans * sizeof(*x->x_wh));
    return (void *)x;
}

void setup_allpass0x2erev_tilde(void){
    allpass_rev_class = class_new(gensym("allpass.rev~"), (t_newmethod)allpass_rev_new, (t_method)allpass_rev_free, sizeof(t_allpass_rev), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(allpass_rev_class, nullfn, gensym("signal"), 0);
    class_addmethod(allpass_rev_class, (t_method)allpass_rev_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(allpass_rev_class, (t_method)allpass_rev_clear, gensym("clear"), 0);
    class_addmethod(allpass_rev_class, (t_method)allpass_rev_size, gensym("size"), A_DEFFLOAT, 0);
    class_addmethod(allpass_rev_class, (t_method)allpass_rev_gain, gensym("gain"), A_DEFFLOAT, 0);
}
