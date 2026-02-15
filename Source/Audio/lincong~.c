// Porres 2017-2025

#include <m_pd.h>
#include <stdlib.h>
#include <math.h>
#include "magic.h"

#define MAXLEN 1024

typedef struct _lincong{
    t_object    x_obj;
    double     *x_phase;
    double     *x_ynm1;
    double     *x_scaled;
    double      x_init_ynm1;
    double      x_a;
    double      x_c;
    double      x_m;
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
}t_lincong;

static t_class *lincong_class;

static void lincong_coeffs(t_lincong *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    int argnum = 0; // current argument
    while(ac){
        if(av->a_type != A_FLOAT)
            pd_error(x, "lincong~: coefficients must be floats");
        else{
            t_float curf = atom_getfloatarg(0, ac, av);
            switch(argnum){
                case 0:
                    x->x_a = curf;
                    break;
                case 1:
                    x->x_c = curf;
                    break;
                case 2:
                    x->x_m = curf;
                    break;
                case 3:
                    x->x_init_ynm1 = curf;
                    break;
            };
            argnum++;
        };
        ac--;
        av++;
    };
    for(int i = 0; i < x->x_nchans; i++){
        x->x_phase[i] = x->x_freq_list[i] >= 0;
        x->x_ynm1[i] = x->x_init_ynm1;
    }
}

static t_int *lincong_perform(t_int *w){
    t_lincong *x = (t_lincong *)(w[1]);
    int chs = (t_int)(w[2]); // number of channels in main input signal (density)
    t_float *freq = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    double a = x->x_a;
    double c = x->x_c;
    double m = x->x_m;
    double *phase = x->x_phase;
    double *ynm1 = x->x_ynm1;
    double *scaled = x->x_scaled;
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
                ynm1[j] = fmod((ynm1[j] * a) + c, m);
                scaled[j] = (ynm1[j] * 2 / m) - 1.f; // rescale
            }
            out[j*x->x_n + i] = scaled[j];
            phase[j] += step;
        }
    }
    x->x_phase = phase;
    x->x_ynm1 = ynm1;
    x->x_scaled = scaled;
    return(w+5);
}

static void lincong_dsp(t_lincong *x, t_signal **sp){
    x->x_n = sp[0]->s_n, x->x_sr_rec = 1.0 / (double)sp[0]->s_sr;
    x->x_sig = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    int chs = x->x_sig ? sp[0]->s_nchans : x->x_list_size;
    int nchans = chs;
    if(chs == 1)
        chs = x->x_ch;
    if(x->x_nchans != chs){
        x->x_phase = (double *)resizebytes(x->x_phase,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_ynm1 = (double *)resizebytes(x->x_ynm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_scaled = (double *)resizebytes(x->x_scaled,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_nchans = chs;
        for(int i = 0; i < x->x_nchans; i++){
            if(x->x_freq_list[i] >= 0)
                x->x_phase[i] = 1;
            x->x_ynm1[i] = x->x_init_ynm1;
        }
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(lincong_perform, 4, x, nchans, sp[0]->s_vec, sp[1]->s_vec);
}

static void lincong_list(t_lincong *x, t_symbol *s, int ac, t_atom * av){
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

static void lincong_set(t_lincong *x, t_symbol *s, int ac, t_atom *av){
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

static void *lincong_free(t_lincong *x){
    outlet_free(x->x_outlet);
    freebytes(x->x_phase, x->x_nchans * sizeof(*x->x_phase));
    freebytes(x->x_ynm1, x->x_nchans * sizeof(*x->x_ynm1));
    freebytes(x->x_scaled, x->x_nchans * sizeof(*x->x_scaled));
    return(void *)x;
}

static void *lincong_new(t_symbol *s, int ac, t_atom *av){
    t_lincong *x = (t_lincong *)pd_new(lincong_class);
    x->x_ignore = s;
    x->x_list_size = 1;
    x->x_nchans = 1;
    x->x_ch = 1;
    x->x_scaled = (double *)getbytes(sizeof(*x->x_scaled));
    x->x_freq_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_freq_list[0] = sys_getsr() * 0.5;
    double a = 1.1, c = 0.13, m = 1;
    x->x_init_ynm1 = 0;
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
            a = av->a_w.w_float;
            ac--; av++;
            if(ac && av->a_type == A_FLOAT) {
                c = av->a_w.w_float;
                ac--; av++;
                if(ac && av->a_type == A_FLOAT) {
                    m = av->a_w.w_float;
                    if(ac && av->a_type == A_FLOAT) {
                        x->x_init_ynm1 = av->a_w.w_float;
                    }
                }
            }
        }
    }
    
    x->x_phase = (double *)getbytes(sizeof(*x->x_phase) * x->x_list_size);
    x->x_ynm1 = (double *)getbytes(sizeof(*x->x_ynm1) * x->x_list_size);
    x->x_phase[0] = x->x_ynm1[0] = x->x_scaled[0] = 0;
    
    x->x_a = a, x->x_c = c, x->x_m = m;
    for(int i = 0; i < x->x_list_size; i++){
        if(x->x_freq_list[i] >= 0)
            x->x_phase[i] = 1;
        x->x_ynm1[i] = x->x_init_ynm1;
    }
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    x->x_glist = canvas_getcurrent();
    return(x);
errstate:
    post("[lincong~]: improper args");
    return(NULL);
}

void lincong_tilde_setup(void){
    lincong_class = class_new(gensym("lincong~"), (t_newmethod)lincong_new, (t_method)lincong_free, sizeof(t_lincong), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(lincong_class, nullfn, gensym("signal"), 0);
    class_addmethod(lincong_class, (t_method)lincong_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(lincong_class, lincong_list);
    class_addmethod(lincong_class, (t_method)lincong_set, gensym("set"), A_GIMME, 0);
    class_addmethod(lincong_class, (t_method)lincong_coeffs, gensym("coeffs"), A_GIMME, 0);
}
