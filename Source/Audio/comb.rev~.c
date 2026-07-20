// based on the one we did for cyclone

#include <math.h>
#include <stdlib.h>
#include <m_pd.h>
#include <buffer.h>

#define COMB_MIND       64          // minimum delay in samples
#define COMB_MAXD       4294967294  // max delay = 2**32 - 2

static t_class *comb_class;

typedef struct _comb{
    t_object        x_obj;
    t_inlet        *x_dellet;
    t_inlet        *x_alet;
    t_inlet        *x_blet;
    t_inlet        *x_clet;
    t_outlet       *x_outlet;
    int             x_sr;
    double          x_sr_khz;
    double         *x_ybuf;
    double         *x_xbuf;
    unsigned int    x_sz;
    t_float         x_maxdel;
    unsigned int   *x_wh;
    int             x_n;
    int             x_nchans;
}t_comb;

static void comb_clear(t_comb *x){
    for(unsigned int i = 0; i < x->x_sz * x->x_nchans; i++)
        x->x_xbuf[i] = x->x_ybuf[i] = 0.;
    for(int ch = 0; ch < x->x_nchans; ch++)
        x->x_wh[ch] = 0;
}

static void comb_sz(t_comb *x){
    unsigned int newsz = (unsigned int)ceil((double)x->x_maxdel * x->x_sr_khz) + 1;
    if(newsz < COMB_MIND) newsz = COMB_MIND;
    else if(newsz > COMB_MAXD) newsz = COMB_MAXD;
    x->x_xbuf = (double *)realloc(x->x_xbuf, sizeof(double) * newsz * x->x_nchans);
    x->x_ybuf = (double *)realloc(x->x_ybuf, sizeof(double) * newsz * x->x_nchans);
    x->x_sz = newsz;
    x->x_maxdel = newsz / x->x_sr_khz;
    comb_clear(x);
}

static void comb_size(t_comb *x, t_floatarg f1){
    x->x_maxdel = f1 < COMB_MIND / x->x_sr_khz  ? COMB_MIND / x->x_sr_khz : f1;
    comb_sz(x);
}

static double comb_read_delay(t_comb *x, double arr[], unsigned int offset, double rh){
    unsigned int a = (unsigned int)rh;
    unsigned int b = a + 1;
    double frac = rh - (double)a;
    double output;
    if(a >= x->x_sz - 1) // checking to see if index falls within bounds
        output = arr[offset + x->x_sz - 1];
    else{ // linear interpolation
        double ya = arr[offset + a], yb = arr[offset + b];
        output = ya + ((yb - ya) * frac);
    };
    return(output);
}

static t_int *comb_perform(t_int *w){
    t_comb *x = (t_comb *)(w[1]);
    t_float *xin = (t_float *)(w[2]);
    t_float *din = (t_float *)(w[3]);
    t_float *ain = (t_float *)(w[4]);
    t_float *bin = (t_float *)(w[5]);
    t_float *cin = (t_float *)(w[6]);
    t_float *out = (t_float *)(w[7]);
    unsigned int n = x->x_n;
    for(int j = 0; j < x->x_nchans; j++){
        unsigned int offset = j * x->x_sz;
        t_float *xin_ch = xin + j * n;
        t_float *out_ch = out + j * n;
        for(unsigned int i = 0; i < n; i++){
            double xn = (double)xin_ch[i];
            x->x_xbuf[offset + x->x_wh[j]] = xn;
            t_float delms = din[i];
            if(delms < 0)
                delms = 0;
            else if(delms > x->x_maxdel)
                delms = x->x_maxdel;
            double delsamps = (double)delms * x->x_sr_khz;

            double a = (double)ain[i], b = (double)bin[i], c = (double)cin[i];
            double rh = (double)x->x_wh[j] + (double)x->x_sz - delsamps;
            while(rh >= x->x_sz)
                rh -= (double)x->x_sz;
            double delx = comb_read_delay(x, x->x_xbuf, offset, rh);
            double dely = comb_read_delay(x, x->x_ybuf, offset, rh);
            double yn = a * xn + b * delx + c * dely;
            out_ch[i] = x->x_ybuf[offset + x->x_wh[j]] = yn;
            x->x_wh[j] = (x->x_wh[j] + 1) % x->x_sz;
        }
    }
    return(w+8);
}

static void comb_dsp(t_comb *x, t_signal **sp){
    int sr = sp[0]->s_sr;
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    if(chs != x->x_nchans){
        x->x_wh = (unsigned int *)resizebytes(x->x_wh,
            x->x_nchans * sizeof(unsigned int),
            chs * sizeof(unsigned int));
        x->x_nchans = chs;
        comb_sz(x);
    }
    signal_setmultiout(&sp[5], x->x_nchans);
    for(int i = 1; i < 5; i++){
        if(sp[i]->s_nchans > 1){
            dsp_add_zero(sp[5]->s_vec, x->x_nchans * x->x_n);
            pd_error(x, "[comb.rev~]: secondary inlets must be mono");
            return;
        }
    }
    if(sr != x->x_sr){ // realloc if sample rate changed
        x->x_sr = sr;
        x->x_sr_khz = (double)x->x_sr * 0.001;
        comb_sz(x);
    };
    dsp_add(comb_perform, 7, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec);
}

static void * comb_free(t_comb *x){
    free(x->x_xbuf);
    free(x->x_ybuf);
    inlet_free(x->x_dellet);
    inlet_free(x->x_alet);
    inlet_free(x->x_blet);
    inlet_free(x->x_clet);
    outlet_free(x->x_outlet);
    return(void *)x;
}

static void *comb_new(t_symbol *s, int ac, t_atom * av){
    (void)s;
    t_comb *x = (t_comb *)pd_new(comb_class);
    // defaults
    t_float maxdel = 1000.0;
    t_float initdel = 0., gain = 0., ffcoeff = 0., fbcoeff = 0.;
    x->x_sr = sys_getsr();
    x->x_sr_khz = x->x_sr * 0.001;

    x->x_sz = COMB_MIND;
    x->x_n = 64;
    x->x_nchans = 1;

    x->x_wh = (unsigned int *)getbytes(sizeof(*x->x_wh));
    x->x_xbuf = NULL;
    x->x_ybuf = NULL;
    
    int argnum = 0, size_flag = 0;
    while(ac > 0){
        if(av->a_type == A_FLOAT){
            t_float curf = atom_getfloat(av);
            switch(argnum){
                case 0:
                    initdel = curf;
                    if(!size_flag)
                        maxdel = initdel;
                    break;
                case 1:
                    gain = curf;
                    break;
                case 2:
                    ffcoeff = curf;
                    break;
                case 3:
                    fbcoeff = curf;
                    break;
                default:
                    break;
            };
            argnum++;
            ac--, av++;
        }
        else if(av->a_type == A_SYMBOL && !argnum){
            t_symbol * cursym = atom_getsymbol(av);
            if(cursym == gensym("-size")){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    maxdel = atom_getfloatarg(1, ac, av);
                    size_flag = 1;
                    ac-=2, av+=2;
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    };
    if(maxdel < (double)COMB_MIND / x->x_sr_khz)
        maxdel = (double)COMB_MIND / x->x_sr_khz;
    x->x_maxdel = maxdel;
    comb_sz(x); // set size and deal with allocation if necessary
    // boundschecking
    if(initdel < 0)
        initdel = 0;
    else if(initdel > x->x_maxdel)
        initdel = x->x_maxdel;
    // inlets outlets
    x->x_dellet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_dellet, initdel);
    x->x_alet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_alet, gain);
    x->x_blet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_blet, ffcoeff);
    x->x_clet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_clet, fbcoeff);
    x->x_outlet = outlet_new((t_object *)x, &s_signal);
    return(x);
errstate:
    pd_error(x, "[comb.rev~]: improper args");
    return(NULL);
}

void setup_comb0x2erev_tilde(void){
    comb_class = class_new(gensym("comb.rev~"), (t_newmethod)comb_new,
        (t_method)comb_free, sizeof(t_comb), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(comb_class, nullfn, gensym("signal"), 0);
    class_addmethod(comb_class, (t_method)comb_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(comb_class, (t_method)comb_clear, gensym("clear"), 0);
    class_addmethod(comb_class, (t_method)comb_size, gensym("size"), A_DEFFLOAT, 0);
}
