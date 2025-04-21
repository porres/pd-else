// porres 2017

#include <m_pd.h>
#include "math.h"

typedef struct _smooth2{
    t_object  x_obj;
    t_inlet  *x_inlet_ms_up;
    t_inlet  *x_inlet_ms_down;
    int      *x_n_up;
    int      *x_nleft_up;
    int      *x_n_down;
    int      *x_nleft_down;
    int       x_reset;
    int       x_nchans;
    float    *x_last_in;
    float    *x_last_out;
    float    *x_delta;
    float    *x_a;
    float    *x_b;
    float     x_sr_khz;
    float     x_curve;
    t_float     x_in;
    t_symbol   *x_ignore_symbol;
}t_smooth2;

static t_class *smooth2_class;

static void smooth2_curve(t_smooth2 *x, t_floatarg f){
    x->x_curve = f * -4;
}

static void smooth2_reset(t_smooth2 *x){
    x->x_reset = 1;
}

static t_int *smooth2_perform(t_int *w){
    t_smooth2 *x = (t_smooth2 *)(w[1]);
    int n = (int)(w[2]);
    int ch2 = (int)(w[3]);
    int ch3 = (int)(w[4]);
    t_float *in1 = (t_float *)(w[5]);
    t_float *in2 = (t_float *)(w[6]);
    t_float *in3 = (t_float *)(w[7]);
    t_float *out = (t_float *)(w[8]);
    float *last_in = x->x_last_in;
    float *last_out = x->x_last_out;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            t_float in = in1[j*n + i];
            t_float ms_up = ch2 == 1 ? in2[i] : in2[j*n + i];
            t_float ms_down = ch3 == 1 ? in3[i] : in3[j*n + i];
            if(ms_up <= 0)
                ms_up  = 0;
            if(ms_down <= 0)
                ms_down  = 0;
            x->x_n_up[j] = (int)roundf(ms_up * x->x_sr_khz); // n samples
            x->x_n_down[j] = (int)roundf(ms_down * x->x_sr_khz); // n samples
            if(x->x_reset){ // reset
                x->x_nleft_up[j] = x->x_nleft_down[j] = 0;
                out[j*n + i] = last_out[j] = last_in[j] = in;
                if(j == (x->x_nchans - 1))
                    x->x_reset = 0;
            }
            else if(in != last_in[j]){ // input change, update
                x->x_delta[j] = (in - last_out[j]);
                x->x_b[j] = x->x_delta[j] / (1 - exp(x->x_curve));
                x->x_a[j] = last_out[j] + x->x_b[j];
                if(x->x_delta[j] > 0){ // ramp up
                    x->x_nleft_up[j] = x->x_n_up[j];
                    if(x->x_n_up[j] == 0)
                        out[j*n + i] = last_out[j] = last_in[j] = in;
                    else{
                        out[j*n + i] = last_out[j];
                        last_in[j] = in;
                        x->x_nleft_up[j]--;
                    }
                }
                else{ // ramp down
                    x->x_nleft_down[j] = x->x_n_down[j];
                    if(x->x_n_down[j] == 0)
                        out[j*n + i] = last_out[j] = last_in[j] = in;
                    else{
                        out[j*n + i] = last_out[j];
                        last_in[j] = in;
                        x->x_nleft_down[j]--;
                    }
                }
            }
            else{
                if(x->x_delta[j] > 0){
                    if(x->x_nleft_up[j] > 0){
                        x->x_nleft_up[j]--;
                        if(fabs(x->x_curve) > 0.001){
                            x->x_b[j] *= exp(x->x_curve / x->x_n_up[j]); // inc
                            out[j*n + i] = last_out[j]  = x->x_a[j] - x->x_b[j];
                        }
                        else
                            out[j*n + i] = last_out[j] += (x->x_delta[j] / x->x_n_up[j]);
                    }
                    else{
                        out[j*n + i] = last_out[j] = last_in[j] = in;
                    }
                }
                else{
                    if(x->x_nleft_down[j] > 0){
                        x->x_nleft_down[j]--;
                        if(fabs(x->x_curve) > 0.001){
                            x->x_b[j] *= exp(x->x_curve / x->x_n_down[j]); // inc
                            out[j*n + i]  = last_out[j]  = x->x_a[j] - x->x_b[j];
                        }
                        else
                            out[j*n + i]  = last_out[j] += (x->x_delta[j] / x->x_n_down[j]);
                    }
                    else
                        out[j*n + i] = last_out[j] = last_in[j] = in;
                }
            }
        };
    };
    x->x_last_in = last_in;
    x->x_last_out = last_out;
    return(w+9);
}

static void smooth2_dsp(t_smooth2 *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    int chs = sp[0]->s_nchans, ch2 = sp[1]->s_nchans, ch3 = sp[2]->s_nchans, n = sp[0]->s_n;
    signal_setmultiout(&sp[3], chs);
    if(x->x_nchans != chs){
        x->x_a = (float *)resizebytes(x->x_a,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_b = (float *)resizebytes(x->x_b,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_n_up = (int *)resizebytes(x->x_n_up,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_n_down = (int *)resizebytes(x->x_n_down,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_nleft_up = (int *)resizebytes(x->x_nleft_up,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_nleft_down = (int *)resizebytes(x->x_nleft_down,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_last_in = (t_float *)resizebytes(x->x_last_in,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_last_out = (t_float *)resizebytes(x->x_last_out,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_delta = (t_float *)resizebytes(x->x_delta,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchans = chs;
    }
    if((ch2 > 1 && ch2 != x->x_nchans) || (ch3 > 1 && ch3 != x->x_nchans)){
        dsp_add_zero(sp[3]->s_vec, chs*n);
        pd_error(x, "[smooth2~]: channel sizes mismatch");
    }
    else
        dsp_add(smooth2_perform, 8, x, n, ch2, ch3, sp[0]->s_vec,
            sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *smooth2_free(t_smooth2 *x){
    inlet_free(x->x_inlet_ms_up);
    inlet_free(x->x_inlet_ms_down);
    freebytes(x->x_a, x->x_nchans * sizeof(*x->x_a));
    freebytes(x->x_b, x->x_nchans * sizeof(*x->x_b));
    freebytes(x->x_n_up, x->x_nchans * sizeof(*x->x_n_up));
    freebytes(x->x_n_down, x->x_nchans * sizeof(*x->x_n_down));
    freebytes(x->x_nleft_up, x->x_nchans * sizeof(*x->x_nleft_up));
    freebytes(x->x_nleft_down, x->x_nchans * sizeof(*x->x_nleft_down));
    freebytes(x->x_last_in, x->x_nchans * sizeof(*x->x_last_in));
    freebytes(x->x_last_out, x->x_nchans * sizeof(*x->x_last_out));
    freebytes(x->x_delta, x->x_nchans * sizeof(*x->x_delta));
    return(void *)x;
}

static void *smooth2_new(t_symbol *s, int ac, t_atom *av){
    t_smooth2 *x = (t_smooth2 *)pd_new(smooth2_class);
    x->x_ignore_symbol = s;
    x->x_a = (float *)getbytes(sizeof(*x->x_a));
    x->x_b = (float *)getbytes(sizeof(*x->x_b));
    x->x_n_up = (int *)getbytes(sizeof(*x->x_n_up));
    x->x_n_down = (int *)getbytes(sizeof(*x->x_n_down));
    x->x_nleft_up = (int *)getbytes(sizeof(*x->x_nleft_up));
    x->x_nleft_down = (int *)getbytes(sizeof(*x->x_nleft_down));
    x->x_last_in = (t_float *)getbytes(sizeof(*x->x_last_in));
    x->x_last_out = (t_float *)getbytes(sizeof(*x->x_last_out));
    x->x_delta = (t_float *)getbytes(sizeof(*x->x_delta));
    x->x_sr_khz = sys_getsr() * 0.001;
    x->x_reset = 0;
    x->x_curve = 0.;
    float ms_up = 0, ms_down = 0.;
    int argnum = 0;
    while(ac > 0){
        if(av->a_type == A_FLOAT){
            float argval = atom_getfloatarg(0, ac, av);
            switch(argnum){
                case 0:
                    ms_up = argval;
                    break;
                case 1:
                    ms_down = argval;
                    break;
                default:
                    break;
            };
            argnum++;
            ac--, av++;
        }
        else if(av->a_type == A_SYMBOL && !argnum){
            if(atom_getsymbol(av) == gensym("-curve")){
                if(ac >= 2){
                    ac--, av++;
                    x->x_curve = atom_getfloat(av);
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
    x->x_inlet_ms_up = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_ms_up, ms_up);
    x->x_inlet_ms_down = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_ms_down, ms_down);
    outlet_new((t_object *)x, &s_signal);
    return(x);
errstate:
    pd_error(x, "[smooth2~]: improper args");
    return NULL;
}

void smooth2_tilde_setup(void){
    smooth2_class = class_new(gensym("smooth2~"), (t_newmethod)smooth2_new,
        (t_method)smooth2_free, sizeof(t_smooth2), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(smooth2_class, t_smooth2, x_in);
    class_addmethod(smooth2_class, (t_method) smooth2_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(smooth2_class, (t_method)smooth2_reset, gensym("reset"), 0);
    class_addmethod(smooth2_class, (t_method)smooth2_curve, gensym("curve"), A_FLOAT, 0);
}
