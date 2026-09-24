// Porres 2018

#include <m_pd.h>
#include <buffer.h>
#include <math.h>
#include <stdlib.h>

#define FBD_STACK   48000
#define FBD_MAXD    4294967294

static t_class *fbdelay_class;

typedef struct _fbdelay{
    t_object        x_obj;
    t_inlet        *x_dellet;
    t_inlet        *x_alet;
    t_outlet       *x_outlet;
    t_float         x_sr_khz;
    t_int           x_alloc;
    t_float         x_maxdel;
    double         *x_ybuf;
    double          x_fbstack[FBD_STACK];
    unsigned int    x_sz;
    unsigned int   *x_wp;
    unsigned int    x_freeze;
    int             x_nchans;
    int             x_delchans;
    int             x_fbchans;
}t_fbdelay;

static void fbdelay_clear(t_fbdelay *x){
    for(int j = 0; j < x->x_nchans; j++){
        for(unsigned int i = 0; i < x->x_sz; i++)
            x->x_ybuf[j*x->x_sz+i] = 0.;
        x->x_wp[j] = 0;
    }
}

static void fbdelay_sz(t_fbdelay *x){
    unsigned int newsz = (unsigned int)ceil((double)x->x_maxdel *
        (double)x->x_sr_khz);
    newsz++;
    if(newsz < 1)
        newsz = 1;
    else if(newsz > FBD_MAXD)
        newsz = FBD_MAXD;
    x->x_ybuf = (double *)resizebytes(x->x_ybuf,
        x->x_nchans * x->x_sz * sizeof(*x->x_ybuf),
        x->x_nchans * newsz * sizeof(*x->x_ybuf));
    x->x_sz = newsz;
    fbdelay_clear(x);
}

static void fbdelay_size(t_fbdelay *x, t_floatarg f1){
    if(f1 < 0)
        f1 = 0;
    x->x_maxdel = f1;
    fbdelay_sz(x);
}

static t_int *fbdelay_perform(t_int *w){
    t_fbdelay *x = (t_fbdelay *)(w[1]);
    t_int n = (int)(w[2]);
    t_float *input = (t_float *)(w[3]);
    t_float *din = (t_float *)(w[4]);
    t_float *ain = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);

    for(int j = 0; j < x->x_nchans; j++){
        t_float *in = input + j*n;
        t_float *delin = x->x_delchans == 1 ? din : din + j*n;
        t_float *fbin = x->x_fbchans == 1 ? ain : ain + j*n;
        t_float *outch = out + j*n;
        double *delay = x->x_ybuf + j*x->x_sz;
        unsigned int wp = x->x_wp[j];

        for(t_int i = 0; i < n; i++){
            double input_sample = (double)in[i];
            t_float del = delin[i];

            if(del > x->x_maxdel)
                del = x->x_maxdel;

            del *= x->x_sr_khz;
            if(del < 1)
                del = 1;

            double fb = (double)fbin[i];
            double output;

            if((del - floor(del)) == 0){
                double rp = (double)wp + ((double)x->x_sz - del);
                while(rp >= x->x_sz)
                    rp -= (double)x->x_sz;

                unsigned int ndx = (unsigned int)rp;
                if(ndx >= x->x_sz - 1)
                    ndx = x->x_sz - 1;

                output = delay[ndx];
            }
            else{
                double rp = (double)wp + ((double)x->x_sz - (del + 1));
                while(rp >= x->x_sz)
                    rp -= (double)x->x_sz;

                double frac = 1 - ((double)del - floor(del));
                unsigned int ndxm1 = (unsigned int)rp;
                unsigned int ndx = ndxm1 + 1;
                unsigned int ndx1 = ndxm1 + 2;
                unsigned int ndx2 = ndxm1 + 3;

                if(ndx > x->x_sz - 1)
                    ndx = x->x_sz - 1;
                if(ndx1 > x->x_sz - 1)
                    ndx1 = x->x_sz - 1;
                if(ndx2 > x->x_sz - 1)
                    ndx2 = x->x_sz - 1;

                double a = delay[ndxm1];
                double b = delay[ndx];
                double c = delay[ndx1];
                double d = delay[ndx2];

                output = interp_spline(frac, a, b, c, d);
            }

            if(!x->x_freeze)
                delay[wp] = input_sample + output * fb;

            wp = (wp + 1) % x->x_sz;
            outch[i] = (t_float)output;
        }

        x->x_wp[j] = wp;
    }

    return(w+7);
}

static void fbdelay_freeze(t_fbdelay *x, t_float f){
    x->x_freeze = (unsigned int)(f != 0);
}

static void fbdelay_dsp(t_fbdelay *x, t_signal **sp){
    int chs = sp[0]->s_nchans;
    x->x_delchans = sp[1]->s_nchans;
    x->x_fbchans = sp[2]->s_nchans;

    if(x->x_nchans != chs){
        x->x_ybuf = (double *)resizebytes(x->x_ybuf,
            x->x_nchans * x->x_sz * sizeof(*x->x_ybuf),
            chs * x->x_sz * sizeof(*x->x_ybuf));

        x->x_wp = (unsigned int *)resizebytes(x->x_wp,
            x->x_nchans * sizeof(*x->x_wp),
            chs * sizeof(*x->x_wp));

        for(int j = x->x_nchans; j < chs; j++)
            x->x_wp[j] = 0;

        x->x_nchans = chs;
        fbdelay_clear(x);
    }

    signal_setmultiout(&sp[3], x->x_nchans);

    if((x->x_delchans > 1 && x->x_delchans != x->x_nchans)
    || (x->x_fbchans > 1 && x->x_fbchans != x->x_nchans)){
        dsp_add_zero(sp[3]->s_vec, x->x_nchans * sp[3]->s_n);
        pd_error(x, "[fbdelay2~]: channel sizes mismatch");
        return;
    }

    dsp_add(fbdelay_perform, 6, x, sp[0]->s_n, sp[0]->s_vec,
        sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *fbdelay_new(t_symbol *s, int argc, t_atom *argv){
    t_fbdelay *x = (t_fbdelay *)pd_new(fbdelay_class);
    t_symbol *cursym = s;
    x->x_sr_khz = sys_getsr() * 0.001;
    x->x_alloc = 1;
    x->x_sz = FBD_STACK;
    x->x_nchans = 1;
    x->x_delchans = 1;
    x->x_fbchans = 1;
    x->x_ybuf = (double *)getbytes(x->x_sz * sizeof(*x->x_ybuf));
    x->x_wp = (unsigned int *)getbytes(sizeof(*x->x_wp));
    x->x_wp[0] = 0;
    fbdelay_clear(x);

    float del_time = 0;
    float delsize = 1000;
    float fb = 0;
    x->x_freeze = 0;

    int argnum = 0;
    while(argc > 0){
        if(argv->a_type == A_SYMBOL && !argnum){
            cursym = atom_getsymbolarg(0, argc, argv);
            if(cursym == gensym("-size")){
                if(argc >= 2 && (argv+1)->a_type == A_FLOAT){
                    t_float curfloat = atom_getfloatarg(1, argc, argv);
                    delsize = curfloat < 0 ? 0 : curfloat;
                    argc-=2, argv+=2;
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
        }
        else if(argv->a_type == A_FLOAT){
            t_float argval = atom_getfloatarg(0, argc, argv);
            switch(argnum){
                case 0:
                {
                    del_time = argval < 0 ? 0 : argval;
                    if(del_time > 0)
                        delsize = del_time;
                }
                    break;
                case 1:
                    fb = argval;
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

    x->x_maxdel = delsize;
    fbdelay_sz(x);

    x->x_dellet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_dellet, del_time);
    x->x_alet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_alet, fb);
    x->x_outlet = outlet_new((t_object *)x, &s_signal);

    return(x);

errstate:
    pd_error(x, "[fbdelay2~]: improper args");
    return(NULL);
}

static void *fbdelay_free(t_fbdelay *x){
    freebytes(x->x_ybuf,
        x->x_nchans * x->x_sz * sizeof(*x->x_ybuf));
    freebytes(x->x_wp, x->x_nchans * sizeof(*x->x_wp));
    inlet_free(x->x_dellet);
    inlet_free(x->x_alet);
    outlet_free(x->x_outlet);
    return(void *)x;
}

void fbdelay2_tilde_setup(void){
    fbdelay_class = class_new(gensym("fbdelay2~"), (t_newmethod)fbdelay_new,
        (t_method)fbdelay_free, sizeof(t_fbdelay), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(fbdelay_class, nullfn, gensym("signal"), 0);
    class_addmethod(fbdelay_class, (t_method)fbdelay_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(fbdelay_class, (t_method)fbdelay_clear, gensym("clear"), 0);
    class_addmethod(fbdelay_class, (t_method)fbdelay_freeze, gensym("freeze"), A_FLOAT, 0);
    class_addmethod(fbdelay_class, (t_method)fbdelay_size, gensym("size"), A_FLOAT, 0);
}
