// Porres 2017-2025

#include <m_pd.h>
#include <stdlib.h>
#include <math.h>
#include "magic.h"

#define MAXLEN 1024

#define ONESIXTH 0.1666666666666667

typedef struct _lorenz{
    t_object    x_obj;
    double     *x_phase;
    double     *x_dx;
    double     *x_xn;
    double     *x_yn;
    double     *x_zn;
    double     *x_ynm1;
    double     *x_xnm1;
    double     *x_znm1;
    double      x_init_yn;
    double      x_init_xn;
    double      x_init_zn;
    double      x_s;
    double      x_r;
    double      x_b;
    double      x_h;
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
}t_lorenz;

static t_class *lorenz_class;

static void lorenz_coeffs(t_lorenz *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    int argnum = 0; // current argument
    while(ac){
        if(av->a_type != A_FLOAT)
            pd_error(x, "lorenz~: coefficients must be floats");
        else{
            t_float curf = atom_getfloatarg(0, ac, av);
            switch(argnum){
                case 0:
                    x->x_s = curf;
                break;
                case 1:
                    x->x_r = curf;
                break;
                case 2:
                    x->x_b = curf;
                break;
                case 3:
                    x->x_h = curf;
                break;
                case 4:
                    x->x_init_xn = curf;
                break;
                case 5:
                    x->x_init_yn = curf;
                break;
                case 6:
                    x->x_init_zn = curf;
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
        x->x_xn[i] = x->x_init_xn;
        x->x_yn[i] = x->x_init_yn;
        x->x_zn[i] = x->x_init_zn;
    }
}

static t_int *lorenz_perform(t_int *w){
    t_lorenz *x = (t_lorenz *)(w[1]);
    int chs = (t_int)(w[2]); // number of channels in main input signal (density)
    t_float *freq = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);

    double s = x->x_s;
    double b = x->x_b;
    double r = x->x_r;
    double h = x->x_h;
    
    double *phase = x->x_phase;
    double *dx = x->x_dx;
    double *yn = x->x_yn;
    double *xn = x->x_xn;
    double *zn = x->x_zn;
    double *ynm1 = x->x_ynm1;
    double *xnm1 = x->x_xnm1;
    double *znm1 = x->x_znm1;
    
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0, n = x->x_n; i < n; i++){
            t_float hz;
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
                xnm1[j] = xn[j];
                ynm1[j] = yn[j];
                znm1[j] = zn[j];
                double k1x, k2x, k3x, k4x,
                k1y, k2y, k3y, k4y,
                k1z, k2z, k3z, k4z,
                kxHalf, kyHalf, kzHalf;
                double hTimesS = h*s;
            // 4th order Runge-Kutta
                k1x = hTimesS * (ynm1[j] - xnm1[j]);
                k1y = h * (xnm1[j] * (r - znm1[j]) - ynm1[j]);
                k1z = h * (xnm1[j] * ynm1[j] - b * znm1[j]);
                kxHalf = k1x * 0.5;
                kyHalf = k1y * 0.5;
                kzHalf = k1z * 0.5;
                    
                k2x = hTimesS * (ynm1[j] + kyHalf - xnm1[j] - kxHalf);
                k2y = h * ((xnm1[j] + kxHalf) * (r - znm1[j] - kzHalf) - (ynm1[j] + kyHalf));
                k2z = h * ((xnm1[j] + kxHalf) * (ynm1[j] + kyHalf) - b * (znm1[j] + kzHalf));
                kxHalf = k2x * 0.5;
                kyHalf = k2y * 0.5;
                kzHalf = k2z * 0.5;
                    
                k3x = hTimesS * (ynm1[j] + kyHalf - xnm1[j] - kxHalf);
                k3y = h * ((xnm1[j] + kxHalf) * (r - znm1[j] - kzHalf) - (ynm1[j] + kyHalf));
                k3z = h * ((xnm1[j] + kxHalf) * (ynm1[j] + kyHalf) - b * (znm1[j] + kzHalf));
                    
                k4x = hTimesS * (ynm1[j] + k3y - xnm1[j] - k3x);
                k4y = h * ((xnm1[j] + k3x) * (r - znm1[j] - k3z) - (ynm1[j] + k3y));
                k4z = h * ((xnm1[j] + k3x) * (ynm1[j] + k3y) - b * (znm1[j] + k3z));
                    
                xn[j] = xn[j] + (k1x + 2.0*(k2x + k3x) + k4x) * ONESIXTH;
                yn[j] = yn[j] + (k1y + 2.0*(k2y + k3y) + k4y) * ONESIXTH;
                zn[j] = zn[j] + (k1z + 2.0*(k2z + k3z) + k4z) * ONESIXTH;
                    
                dx[j] = xn[j] - xnm1[j];
            }
            out[j*x->x_n + i] = (xnm1[j] + (dx[j] * phase[j])) * 0.04;
            phase[j] += step;
        }
    }
    x->x_phase = phase;
    x->x_yn = yn, x->x_xn = xn, x->x_zn = zn;
    x->x_ynm1 = ynm1, x->x_xnm1 = xnm1, x->x_znm1 = znm1;
    return(w+5);
}

static void lorenz_dsp(t_lorenz *x, t_signal **sp){
    x->x_n = sp[0]->s_n, x->x_sr_rec = 1.0 / (double)sp[0]->s_sr;
    x->x_sig = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    int chs = x->x_sig ? sp[0]->s_nchans : x->x_list_size;
    int nchans = chs;
    if(chs == 1)
        chs = x->x_ch;
    if(x->x_nchans != chs){
        x->x_phase = (double *)resizebytes(x->x_phase,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_dx = (double *)resizebytes(x->x_dx,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_xn = (double *)resizebytes(x->x_xn,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_yn = (double *)resizebytes(x->x_yn,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_zn = (double *)resizebytes(x->x_zn,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_xnm1 = (double *)resizebytes(x->x_xnm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_ynm1 = (double *)resizebytes(x->x_ynm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_znm1 = (double *)resizebytes(x->x_znm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_nchans = chs;
        for(int i = 0; i < x->x_nchans; i++){
            if(x->x_freq_list[i] >= 0)
                x->x_phase[i] = 1;
            x->x_xn[i] = x->x_init_xn;
            x->x_yn[i] = x->x_init_yn;
            x->x_zn[i] = x->x_init_zn;
        }
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(lorenz_perform, 4, x, nchans, sp[0]->s_vec, sp[1]->s_vec);
}

static void lorenz_list(t_lorenz *x, t_symbol *s, int ac, t_atom * av){
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

static void lorenz_set(t_lorenz *x, t_symbol *s, int ac, t_atom *av){
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

static void *lorenz_free(t_lorenz *x){
    outlet_free(x->x_outlet);
    freebytes(x->x_phase, x->x_nchans * sizeof(*x->x_phase));
    freebytes(x->x_dx, x->x_nchans * sizeof(*x->x_dx));
    freebytes(x->x_xn, x->x_nchans * sizeof(*x->x_xn));
    freebytes(x->x_yn, x->x_nchans * sizeof(*x->x_yn));
    freebytes(x->x_zn, x->x_nchans * sizeof(*x->x_zn));
    freebytes(x->x_xnm1, x->x_nchans * sizeof(*x->x_xnm1));
    freebytes(x->x_ynm1, x->x_nchans * sizeof(*x->x_ynm1));
    freebytes(x->x_znm1, x->x_nchans * sizeof(*x->x_znm1));
    return(void *)x;
}

static void *lorenz_new(t_symbol *sym, int ac, t_atom *av){
    t_lorenz *x = (t_lorenz *)pd_new(lorenz_class);
    x->x_ignore = sym;
    x->x_list_size = 1;
    x->x_nchans = 1;
    x->x_ch = 1;
    x->x_dx = (double *)getbytes(sizeof(*x->x_dx));
    x->x_xnm1 = (double *)getbytes(sizeof(*x->x_xnm1));
    x->x_ynm1 = (double *)getbytes(sizeof(*x->x_ynm1));
    x->x_znm1 = (double *)getbytes(sizeof(*x->x_znm1));
    x->x_freq_list = (float*)malloc(MAXLEN * sizeof(float));
    double s = 10, r = 28, b = 2.667, h = 0.05;
    x->x_init_xn = 0.1, x->x_init_yn = 0., x->x_init_zn = 0.;
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
    if(ac > 0 && av->a_type == A_FLOAT){
        x->x_freq_list[0] = av->a_w.w_float;
        ac--; av++;
        if(ac && av->a_type == A_FLOAT) {
            s = av->a_w.w_float;
            ac--; av++;
            if(ac && av->a_type == A_FLOAT) {
                r = av->a_w.w_float;
                ac--; av++;
                if(ac && av->a_type == A_FLOAT) {
                    b = av->a_w.w_float;
                    ac--; av++;
                    if(ac && av->a_type == A_FLOAT) {
                        h = av->a_w.w_float;
                        ac--; av++;
                        if(ac && av->a_type == A_FLOAT) {
                            x->x_init_xn = av->a_w.w_float;
                            if(ac && av->a_type == A_FLOAT) {
                                x->x_init_yn = av->a_w.w_float;
                                if(ac && av->a_type == A_FLOAT) {
                                    x->x_init_zn = av->a_w.w_float;
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    x->x_s = s, x->x_r = r, x->x_b = b, x->x_h = h;
    
    x->x_phase = (double *)getbytes(sizeof(*x->x_phase) * x->x_list_size);
    x->x_xn = (double *)getbytes(sizeof(*x->x_xn) * x->x_list_size);
    x->x_yn = (double *)getbytes(sizeof(*x->x_yn) * x->x_list_size);
    x->x_zn = (double *)getbytes(sizeof(*x->x_zn) * x->x_list_size);
    
    x->x_freq_list[0] = sys_getsr() * 0.5;
    x->x_phase[0] = x->x_yn[0] = 0;
    for(int i = 0; i < x->x_list_size; i++){
        if(x->x_freq_list[i] >= 0)
            x->x_phase[i] = 1;
        x->x_xn[i] = x->x_init_xn;
        x->x_yn[i] = x->x_init_yn;
        x->x_zn[i] = x->x_init_zn;
    }
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    x->x_glist = canvas_getcurrent();
    return(x);
errstate:
    post("[lorenz~]: improper args");
    return(NULL);
}

void lorenz_tilde_setup(void){
    lorenz_class = class_new(gensym("lorenz~"), (t_newmethod)lorenz_new, (t_method)lorenz_free, sizeof(t_lorenz), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(lorenz_class, nullfn, gensym("signal"), 0);
    class_addmethod(lorenz_class, (t_method)lorenz_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(lorenz_class, lorenz_list);
    class_addmethod(lorenz_class, (t_method)lorenz_set, gensym("set"), A_GIMME, 0);
    class_addmethod(lorenz_class, (t_method)lorenz_coeffs, gensym("coeffs"), A_GIMME, 0);
}
