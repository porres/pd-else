// Porres 2018-2026

#include <m_pd.h>

typedef struct _schmitt{
    t_object x_obj;
    int      x_nchans;
    int      x_ch2;
    int      x_ch3;
    int      x_n;
    t_float *x_last;
    t_inlet  *x_lolet;
    t_inlet  *x_hilet;
}t_schmitt;

static t_class *schmitt_class;

static t_int *schmitt_perform(t_int *w){
    t_schmitt *x = (t_schmitt *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *in3 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    t_float *last = x->x_last;
    int n = x->x_n;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            float in = in1[j*n + i];
            float lo = x->x_ch2 == 1 ? in2[i] : in2[j*n + i];
            float hi = x->x_ch3 == 1 ? in3[i] : in3[j*n + i];
            out[j*n + i] = last[j] = (in > lo && (in >= hi || last[j]));
        }
    }
    x->x_last = last;
    return(w+6);
}

static void schmitt_dsp(t_schmitt *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    x->x_ch2 = sp[1]->s_nchans, x->x_ch3 = sp[2]->s_nchans;
    if(x->x_nchans != chs){
       x->x_last = (t_float *)resizebytes(x->x_last,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[3], x->x_nchans);
    if((x->x_ch2 > 1 && x->x_ch2 != x->x_nchans)
    || (x->x_ch3 > 1 && x->x_ch3 != x->x_nchans)){
        dsp_add_zero(sp[3]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[schmitt~]: channel sizes mismatch");
        return;
    }
    dsp_add(schmitt_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *schmitt_free(t_schmitt *x){
    freebytes(x->x_last, x->x_nchans * sizeof(*x->x_last));
    inlet_free(x->x_lolet);
    inlet_free(x->x_hilet);
    return (void *)x;
}

static void *schmitt_new(t_symbol *s, int argc, t_atom *argv){
    s = NULL;
    t_schmitt *x = (t_schmitt *) pd_new(schmitt_class);
    t_float thlo = 0, thhi = 0;
    x->x_last = (t_float *)getbytes(sizeof(*x->x_last));
    x->x_last[0] = 0;
    int argnum = 0;
    while(argc > 0){
        if(argv -> a_type == A_FLOAT){
            t_float argval = atom_getfloatarg(0,argc,argv);
            switch(argnum){
                case 0:
                    thlo = argval;
                    break;
                case 1:
                    thhi = argval;
                    break;
                default:
                    break;
            };
            argc--;
            argv++;
            argnum++;
        }
        else{
            goto errstate;
        };
    };
    x->x_lolet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_lolet, thlo);
    x->x_hilet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_hilet, thhi);
    outlet_new((t_object *)x, &s_signal);
    return(x);
errstate:
    pd_error(x, "[schmitt~]: improper args");
    return(NULL);
}

void schmitt_tilde_setup(void){
    schmitt_class = class_new(gensym("schmitt~"), (t_newmethod)schmitt_new,
        (t_method)schmitt_free, sizeof(t_schmitt), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(schmitt_class, nullfn, gensym("signal"), 0);
    class_addmethod(schmitt_class, (t_method)schmitt_dsp, gensym("dsp"), A_CANT, 0);
}
