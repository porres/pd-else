// Porres 2017-2026

#include <m_pd.h>
#include <math.h>

#define LOG001 log(0.001)

typedef struct _decay{
    t_object    x_obj;
    t_inlet    *x_inlet_ms;
    t_outlet   *x_out;
    t_float     x_sr_khz;
    t_int       x_flag;
    int         x_nchans;
    int         x_nblock;
    int         x_norm;
    double     *x_xnm1;
    double     *x_ynm1;
    double      x_f;
    t_symbol   *x_ignore;
}t_decay;

static t_class *decay_class;

static void decay_bang(t_decay *x){
    x->x_flag = 1;
}

static void decay_float(t_decay *x, t_float f){
    x->x_f = x->x_norm ? (double)f : copysign(1, f);
    x->x_flag = 1;
}

static void decay_norm(t_decay *x, t_floatarg f){
    x->x_norm = f != 0;
}

static t_int *decay_perform(t_int *w){
    t_decay *x = (t_decay *)(w[1]);
    int ch2 = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    double *xnm1 = x->x_xnm1;
    double *ynm1 = x->x_ynm1;
    t_float sr_khz = x->x_sr_khz;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < x->x_nblock; i++){
            double in = in1[j*x->x_nblock + i];
            double xn;
            if((in != 0 && xnm1[j] == 0)){
                x->x_ynm1[j] = 0.;
                xn = x->x_norm ? copysign(1, in) : in;
            }
            else
                xn = 0;
            double ms = ch2 == 1 ? in2[i] : in2[j*x->x_nblock + i];
            if(x->x_flag){
                xn = x->x_f;
                x->x_ynm1[j] = 0.;
                x->x_flag = 0;
            }
            if(ms <= 0)
                out[j*x->x_nblock + i] = xn;
            else{
                double a = exp(LOG001 / (ms * sr_khz));
                double yn = xn + a * ynm1[j];
                out[j*x->x_nblock + i] = yn;
                ynm1[j] = yn;
            }
            xnm1[j] = in;
        }
    }
    x->x_xnm1 = xnm1;
    x->x_ynm1 = ynm1;
    return(w+6);
}

static void decay_dsp(t_decay *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    x->x_nblock = sp[0]->s_n;
    int chs = sp[0]->s_nchans, ch2 = sp[1]->s_nchans;
    if(x->x_nchans != chs){
       x->x_xnm1 = (double *)resizebytes(x->x_xnm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_ynm1 = (double *)resizebytes(x->x_ynm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[2], chs);
    if((ch2 > 1 && ch2 != x->x_nchans)){
        dsp_add_zero(sp[2]->s_vec, chs*x->x_nblock);
        pd_error(x, "[decay~]: channel sizes mismatch");
    }
    dsp_add(decay_perform, 5, x, ch2, sp[0]->s_vec,
        sp[1]->s_vec, sp[2]->s_vec);
}

static void decay_clear(t_decay *x){
    for(int i = 0; i < x->x_nchans; i++){
        x->x_xnm1[i] = x->x_ynm1[i] = 0.;
    }
}

static void *decay_free(t_decay *x){
    freebytes(x->x_xnm1, x->x_nchans * sizeof(*x->x_xnm1));
    freebytes(x->x_ynm1, x->x_nchans * sizeof(*x->x_ynm1));
    return(void *)x;
}

static void *decay_new(t_symbol *s, int ac, t_atom *av){
    t_decay *x = (t_decay *)pd_new(decay_class);
    (void)s;
    float ms = 1000;
    x->x_f = 1.;
    x->x_norm = 0;
    x->x_xnm1 = (double *)getbytes(sizeof(*x->x_xnm1));
    x->x_ynm1 = (double *)getbytes(sizeof(*x->x_ynm1));
    x->x_xnm1[0] = x->x_ynm1[0] = 0;
/////////////////////////////////////////////////////////////////////////////////////
    int argnum = 0;
    while(ac > 0){
        if(av->a_type == A_FLOAT){
            float aval = atom_getfloat(av);
            switch(argnum){
                case 0:
                    ms = aval;
                    break;
                case 1:
                default:
                    break;
            };
            argnum++;
            ac--;
            av++;
        }
        else if(av->a_type == A_SYMBOL && !argnum){
            if(atom_getsymbol(av) == gensym("-norm")){
                x->x_norm = 1;
                ac--, av++;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    }
/////////////////////////////////////////////////////////////////////////////////////
    x->x_inlet_ms = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_ms, ms);
    x->x_out = outlet_new((t_object *)x, &s_signal);
    return(x);
    errstate:
        pd_error(x, "[decay~]: improper args");
        return NULL;
}

void decay_tilde_setup(void){
    decay_class = class_new(gensym("decay~"), (t_newmethod)decay_new,
        (t_method)decay_free, sizeof(t_decay), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(decay_class, (t_method)decay_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(decay_class, nullfn, gensym("signal"), 0);
    class_addbang(decay_class, (t_method)decay_bang);
    class_addfloat(decay_class, (t_method)decay_float);
    class_addmethod(decay_class, (t_method)decay_clear, gensym("clear"), 0);
    class_addmethod(decay_class, (t_method)decay_norm, gensym("norm"), A_FLOAT, 0);
}
