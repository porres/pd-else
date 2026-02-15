// Porres 2017-2025

#include <m_pd.h>
#include <stdlib.h>
#include <math.h>
#include "magic.h"

#define MAXLEN 1024

typedef struct _latoocarfian{
    t_object    x_obj;
    double     *x_phase;
    double     *x_yn;
    double     *x_xn;
    double      x_init_yn;
    double      x_init_xn;
    double      x_a;
    double      x_b;
    double      x_c;
    double      x_d;
    int         x_nchans;
    int         x_ch;
    t_int       x_n;
    t_int       x_sig;
    float      *x_freq_list;
    t_int       x_list_size;
    t_symbol   *x_ignore;
    t_outlet   *x_outlet;
    double      x_sr_rec;
    t_glist    *x_glist;
}t_latoocarfian;

static t_class *latoocarfian_class;

static void latoocarfian_coeffs(t_latoocarfian *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    int argnum = 0; // current argument
    while(ac){
        if(av->a_type != A_FLOAT)
            pd_error(x, "latoocarfian~: coefficients must be floats");
        else{
            t_float curf = atom_getfloatarg(0, ac, av);
            switch(argnum){
                case 0:
                    x->x_a = curf;
                    break;
                case 1:
                    x->x_b = curf;
                    break;
                case 2:
                    x->x_c = curf;
                    break;
                case 3:
                    x->x_d = curf;
                    break;
                case 4:
                    x->x_init_xn = curf;
                    break;
                case 5:
                    x->x_init_yn = curf;
                    break;
                default:
                    break;
            };
            argnum++;
        };
        ac--;
        av++;
    };
    for(int i = 0; i < x->x_nchans; i++){
        x->x_phase[i] = x->x_freq_list[i] >= 0;
        x->x_yn[i] = x->x_init_yn;
        x->x_xn[i] = x->x_init_xn;
    }
}

static t_int *latoocarfian_perform(t_int *w){
    t_latoocarfian *x = (t_latoocarfian *)(w[1]);
    int chs = (t_int)(w[2]); // number of channels in main input signal (density)
    t_float *freq = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    double a = x->x_a;
    double b = x->x_b;
    double c = x->x_c;
    double d = x->x_d;
    double *phase = x->x_phase;
    double *yn = x->x_yn;
    double *xn = x->x_xn;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0, n = x->x_n; i < n; i++){
            t_float hz, output;
            if(x->x_sig)
                hz = chs == 1 ? freq[i] : freq[j*n + i];
            else{
                if(chs == 1)
                    hz = x->x_freq_list[0];
                else
                    hz = x->x_freq_list[j];
            }
            double step = hz * x->x_sr_rec; // phase step
            step = step > 1 ? 1 : step < -1 ? -1 : step; // clipped phase_step
            int trig;
            if(hz >= 0){
                trig = phase[j] >= 1.;
                if(trig)
                    phase[j] -= 1;
            }
            else{
                trig = (phase[j] <= 0.);
                if(trig)
                    phase[j] += 1.;
            }
            if(trig){ // update
                output = sin(yn[j] * b) + c * sin(xn[j] * b);
                yn[j] = sin(xn[j] * a) + d * sin(yn[j] * a);
                xn[j] = output;
            }
            else
                output = xn[j];
            out[j*x->x_n + i] = output;
            phase[j] += step;
        }
    }
    x->x_phase = phase;
    x->x_yn = yn;
    x->x_xn = xn;
    return(w+5);
}

static void latoocarfian_dsp(t_latoocarfian *x, t_signal **sp){
    x->x_n = sp[0]->s_n, x->x_sr_rec = 1.0 / (double)sp[0]->s_sr;
    x->x_sig = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    int chs = x->x_sig ? sp[0]->s_nchans : x->x_list_size;
    int nchans = chs;
    if(chs == 1)
        chs = x->x_ch;
    if(x->x_nchans != chs){
        x->x_phase = (double *)resizebytes(x->x_phase,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_yn = (double *)resizebytes(x->x_yn,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_xn = (double *)resizebytes(x->x_xn,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_nchans = chs;
        for(int i = 0; i < x->x_nchans; i++){
            if(x->x_freq_list[i] >= 0)
                x->x_phase[i] = 1;
            x->x_yn[i] = x->x_init_yn;
        }
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(latoocarfian_perform, 4, x, nchans, sp[0]->s_vec, sp[1]->s_vec);
}

static void latoocarfian_list(t_latoocarfian *x, t_symbol *s, int ac, t_atom * av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    for(int i = 0; i < ac; i++)
        x->x_freq_list[i] = atom_getfloat(av+i);
    if(x->x_list_size != ac){
        x->x_list_size = ac;
        canvas_update_dsp();
    }
}

static void latoocarfian_set(t_latoocarfian *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac != 2)
        return;
    int i = atom_getint(av);
    float f = atom_getint(av+1);
    if(i >= x->x_list_size)
        i = x->x_list_size;
    if(i <= 0)
        i = 1;
    i--;
    x->x_freq_list[i] = f;
}

static void *latoocarfian_free(t_latoocarfian *x){
    outlet_free(x->x_outlet);
    freebytes(x->x_phase, x->x_nchans * sizeof(*x->x_phase));
    freebytes(x->x_xn, x->x_nchans * sizeof(*x->x_xn));
    freebytes(x->x_yn, x->x_nchans * sizeof(*x->x_yn));
    return(void *)x;
}

static void *latoocarfian_new(t_symbol *s, int ac, t_atom *av){
    t_latoocarfian *x = (t_latoocarfian *)pd_new(latoocarfian_class);
    x->x_ignore = s;
    x->x_list_size = 1;
    x->x_nchans = 1;
    x->x_ch = 1;
    x->x_freq_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_freq_list[0] = sys_getsr() * 0.5;
    double a = 1, b = 3, c = 0.5, d = 0.5;
    x->x_init_xn = x->x_init_yn = 0.5;
    while(ac && av->a_type == A_SYMBOL){
        if(atom_getsymbol(av) == gensym("-mc")){
            ac--, av++;
            if(!ac || av->a_type != A_FLOAT)
                goto errstate;
            int n = 0;
            while(ac && av->a_type == A_FLOAT){
                x->x_freq_list[n] = atom_getfloat(av);
                ac--, av++, n++;
            }
            x->x_list_size = n;
        }
        else
            goto errstate;
    }
    if(ac && av->a_type == A_FLOAT){
        x->x_freq_list[0] = av->a_w.w_float;
        ac--; av++;
        if(ac && av->a_type == A_FLOAT)
            a = av->a_w.w_float;
            ac--; av++;
            if(ac && av->a_type == A_FLOAT)
                b = av->a_w.w_float;
                ac--; av++;
                if(ac && av->a_type == A_FLOAT)
                    c = av->a_w.w_float;
                    ac--; av++;
                    if(ac && av->a_type == A_FLOAT)
                        d = av->a_w.w_float;
                        ac--; av++;
                        if(ac && av->a_type == A_FLOAT)
                            x->x_init_xn = av->a_w.w_float;
                            if(ac && av->a_type == A_FLOAT)
                                x->x_init_yn = av->a_w.w_float;
    }
    x->x_a = a, x->x_b = b, x->x_c = c, x->x_d = d;
    
    x->x_phase = (double *)getbytes(sizeof(*x->x_phase) * x->x_list_size);
    x->x_xn = (double *)getbytes(sizeof(*x->x_xn) * x->x_list_size);
    x->x_yn = (double *)getbytes(sizeof(*x->x_yn) * x->x_list_size);
    x->x_phase[0] = x->x_yn[0] = 0;
    
    for(int i = 0; i < x->x_list_size; i++){
        if(x->x_freq_list[i] >= 0)
            x->x_phase[i] = 1;
        x->x_xn[i] = x->x_init_xn;
        x->x_yn[i] = x->x_init_yn;
    }
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    x->x_glist = canvas_getcurrent();
    return(x);
errstate:
    post("[latoocarfian~]: improper args");
    return(NULL);
}

void latoocarfian_tilde_setup(void){
    latoocarfian_class = class_new(gensym("latoocarfian~"), (t_newmethod)latoocarfian_new, (t_method)latoocarfian_free, sizeof(t_latoocarfian), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(latoocarfian_class, nullfn, gensym("signal"), 0);
    class_addmethod(latoocarfian_class, (t_method)latoocarfian_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(latoocarfian_class, latoocarfian_list);
    class_addmethod(latoocarfian_class, (t_method)latoocarfian_set, gensym("set"), A_GIMME, 0);
    class_addmethod(latoocarfian_class, (t_method)latoocarfian_coeffs, gensym("coeffs"), A_GIMME, 0);
}
