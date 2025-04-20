// porres 2017

#include <m_pd.h>
#include "math.h"

typedef struct _smooth{
    t_object    x_obj;
    t_inlet    *x_inlet_ms;
    int         x_reset;
    int         x_nchans;
    int        *x_n;
    int        *x_nleft;
    float      *x_last_in;
    float      *x_last_out;
    float      *x_delta;
    float      *x_a;
    float      *x_b;
    float       x_sr_khz;
    float       x_curve;
    t_float     x_in;
    t_symbol   *x_ignore_symbol;
}t_smooth;

static t_class *smooth_class;

static void smooth_curve(t_smooth *x, t_floatarg f){
    x->x_curve = f * -4;
}

static void smooth_reset(t_smooth *x){
    x->x_reset = 1;
}

static t_int *smooth_perform(t_int *w){
    t_smooth *x = (t_smooth *)(w[1]);
    int n = (int)(w[2]);
    int ch2 = (int)(w[3]);
    t_float *in1 = (t_float *)(w[4]);
    t_float *in2 = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);
    float *last_in = x->x_last_in;
    float *last_out = x->x_last_out;
    for(int i = 0; i < n; i++){
        for(int j = 0; j < x->x_nchans; j++){
            t_float in = in1[j*n + i];
            t_float ms = ch2 == 1 ? in2[i] : in2[j*n + i];
            float output;
            if(ms <= 0)
                ms  = 0;
            x->x_n[j] = (int)roundf(ms * x->x_sr_khz) + 1; // n samples
            if(x->x_n[j] == 1)
                output = last_out[j] = last_in[j] = in;
            else{
                if(x->x_reset){ // reset
                    output = last_out[j] = last_in[j] = in;
                    x->x_nleft[j] = 0;
                    if(j == (x->x_nchans - 1))
                        x->x_reset = 0;
                }
                else if(in != last_in[j]){ // input change, update
                    x->x_delta[j] = (in - last_out[j]);
                    x->x_nleft[j] = x->x_n[j] - 1;
                    x->x_b[j] = x->x_delta[j] / (1 - exp(x->x_curve));
                    x->x_a[j] = last_out[j] + x->x_b[j];
                    output = last_out[j];
                    last_in[j] = in;
                    x->x_nleft[j]--;
                }
                else{
                    if(x->x_nleft[j] > 0){
                        x->x_nleft[j]--;
                        if(fabs(x->x_curve) > 0.001){
                            x->x_b[j] *= exp(x->x_curve / x->x_n[j]); // inc
                            output = last_out[j]  = x->x_a[j] - x->x_b[j];
                        }
                        else
                            output = last_out[j] += (x->x_delta[j] / x->x_n[j]);
                    }
                    else
                        output = last_out[j] = last_in[j] = in;
                }
            }
            out[j*n + i] = output;
        };
    };
    x->x_last_in = last_in;
    x->x_last_out = last_out;
    return(w+7);
}

static void smooth_dsp(t_smooth *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    int chs = sp[0]->s_nchans, ch2 = sp[1]->s_nchans, n = sp[0]->s_n;
    signal_setmultiout(&sp[2], chs);
    if(x->x_nchans != chs){
        x->x_n = (int *)resizebytes(x->x_n,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_nleft = (int *)resizebytes(x->x_nleft,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_last_in = (t_float *)resizebytes(x->x_last_in,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_last_out = (t_float *)resizebytes(x->x_last_out,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_delta = (t_float *)resizebytes(x->x_delta,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_a = (t_float *)resizebytes(x->x_a,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_b = (t_float *)resizebytes(x->x_b,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchans = chs;
    }
    if(ch2 > 1 && ch2 != x->x_nchans){
        dsp_add_zero(sp[2]->s_vec, chs*n);
        pd_error(x, "[smooth~]: channel sizes mismatch");
    }
    else
        dsp_add(smooth_perform, 6, x, n, ch2, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *smooth_free(t_smooth *x){
    inlet_free(x->x_inlet_ms);
    freebytes(x->x_n, x->x_nchans * sizeof(*x->x_n));
    freebytes(x->x_nleft, x->x_nchans * sizeof(*x->x_nleft));
    freebytes(x->x_last_in, x->x_nchans * sizeof(*x->x_last_in));
    freebytes(x->x_last_out, x->x_nchans * sizeof(*x->x_last_out));
    freebytes(x->x_delta, x->x_nchans * sizeof(*x->x_delta));
    freebytes(x->x_a, x->x_nchans * sizeof(*x->x_a));
    freebytes(x->x_b, x->x_nchans * sizeof(*x->x_b));
    return(void *)x;
}

static void *smooth_new(t_symbol *s, int ac, t_atom *av){
    t_smooth *x = (t_smooth *)pd_new(smooth_class);
    x->x_ignore_symbol = s;
    x->x_n = (int *)getbytes(sizeof(*x->x_n));
    x->x_nleft = (int *)getbytes(sizeof(*x->x_nleft));
    x->x_last_in = (t_float *)getbytes(sizeof(*x->x_last_in));
    x->x_last_out = (t_float *)getbytes(sizeof(*x->x_last_out));
    x->x_delta = (t_float *)getbytes(sizeof(*x->x_delta));
    x->x_a = (t_float *)getbytes(sizeof(*x->x_a));
    x->x_b = (t_float *)getbytes(sizeof(*x->x_b));
    float ms = 0;
    x->x_sr_khz = sys_getsr() * 0.001;
    x->x_last_in[0] = x->x_last_out[0] = x->x_delta[0] = 0.;
    x->x_n[0] = x->x_nleft[0] = 0;
    x->x_reset = 0;
    x->x_curve = 0;
    x->x_nchans = 1;
    int arg = 0;
    while(ac > 0){
        if(av->a_type == A_FLOAT){
            ms = atom_getfloatarg(0, ac, av);
            ac--, av++;
            arg = 1;
        }
        else if(av->a_type == A_SYMBOL && !arg){
            if(atom_getsymbolarg(0, ac, av) == gensym("-curve")){
                if(ac >= 2){
                    ac--, av++;
                    x->x_curve = atom_getfloat(av) * -4;
                    ac--, av++;
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    }
    x->x_inlet_ms = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_ms, ms);
    outlet_new((t_object *)x, &s_signal);
    return(x);
errstate:
    pd_error(x, "[smooth~]: improper args");
    return(NULL);
}

void smooth_tilde_setup(void){
    smooth_class = class_new(gensym("smooth~"), (t_newmethod)smooth_new,
        (t_method)smooth_free, sizeof(t_smooth), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(smooth_class, t_smooth, x_in);
    class_addmethod(smooth_class, (t_method)smooth_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(smooth_class, (t_method)smooth_reset, gensym("reset"), 0);
    class_addmethod(smooth_class, (t_method)smooth_curve, gensym("curve"), A_FLOAT, 0);
}

