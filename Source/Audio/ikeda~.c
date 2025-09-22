// Porres 2017-2025

#include <m_pd.h>
#include <stdlib.h>
#include <math.h>
#include "magic.h"

#define MAXLEN 1024

typedef struct _ikeda{
    t_object    x_obj;
    double     *x_phase;
    double     *x_ynm1;
    double     *x_ynm2;
    double      x_init_ynm1;
    double      x_init_ynm2;
    double      x_u;
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
}t_ikeda;

static t_class *ikeda_class;

static void ikeda_coeffs(t_ikeda *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    int argnum = 0; // current argument
    while(ac){
        if(av->a_type != A_FLOAT)
            pd_error(x, "ikeda~: coefficients must be floats");
        else{
            t_float curf = atom_getfloatarg(0, ac, av);
            switch(argnum){
                case 0:
                    x->x_u = curf;
                    break;
                case 2:
                    x->x_init_ynm1 = curf;
                    break;
                case 3:
                    x->x_init_ynm2 = curf;
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
        x->x_ynm2[i] = x->x_init_ynm2;
    }
}

static t_int *ikeda_perform(t_int *w){
    t_ikeda *x = (t_ikeda *)(w[1]);
    int chs = (t_int)(w[2]); // number of channels in main input signal (density)
    t_float *freq = (t_float *)(w[3]);
    t_float *out1 = (t_float *)(w[4]);
    t_float *out2 = (t_float *)(w[5]);
    double u = x->x_u;
    double *phase = x->x_phase;
    double *ynm1 = x->x_ynm1;
    double *ynm2 = x->x_ynm2;
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
                float t = 0.4 - 6/(1 + pow(ynm1[j], 2) + pow(ynm2[j], 2));
                ynm1[j] = 1 + u * (ynm1[j] * cos(t) - ynm2[j] * sin(t));
                ynm2[j] = u * (ynm1[j] * sin(t) + ynm2[j] * cos(t));
            }
            out1[j*x->x_n + i] = ynm1[j];
            out2[j*x->x_n + i] = ynm2[j];
            phase[j] += step;
        }
    }
    x->x_phase = phase;
    x->x_ynm1 = ynm1;
    x->x_ynm2 = ynm2;
    return(w+6);
}

static void ikeda_dsp(t_ikeda *x, t_signal **sp){
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
        x->x_ynm2 = (double *)resizebytes(x->x_ynm2,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_nchans = chs;
        for(int i = 0; i < x->x_nchans; i++){
            if(x->x_freq_list[i] >= 0)
                x->x_phase[i] = 1;
            x->x_ynm1[i] = x->x_init_ynm1;
            x->x_ynm2[i] = x->x_init_ynm2;
        }
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    signal_setmultiout(&sp[2], x->x_nchans);
    dsp_add(ikeda_perform, 5, x, nchans, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void ikeda_list(t_ikeda *x, t_symbol *s, int ac, t_atom * av){
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

static void ikeda_set(t_ikeda *x, t_symbol *s, int ac, t_atom *av){
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

static void *ikeda_free(t_ikeda *x){
    outlet_free(x->x_outlet);
    freebytes(x->x_phase, x->x_nchans * sizeof(*x->x_phase));
    freebytes(x->x_ynm1, x->x_nchans * sizeof(*x->x_ynm1));
    freebytes(x->x_ynm2, x->x_nchans * sizeof(*x->x_ynm2));
    return(void *)x;
}

static void *ikeda_new(t_symbol *s, int ac, t_atom *av){
    t_ikeda *x = (t_ikeda *)pd_new(ikeda_class);
    x->x_ignore = s;
    x->x_list_size = 1;
    x->x_nchans = 1;
    x->x_ch = 1;
    x->x_phase = (double *)getbytes(sizeof(*x->x_phase));
    x->x_ynm1 = (double *)getbytes(sizeof(*x->x_ynm1));
    x->x_ynm2 = (double *)getbytes(sizeof(*x->x_ynm2));
    x->x_freq_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_freq_list[0] = sys_getsr() * 0.5;
    x->x_phase[0] = x->x_ynm1[0] = 0;
    double u = 0.75;
    x->x_init_ynm1 = 0, x->x_init_ynm2 = 0; // default parameters
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
            u = av->a_w.w_float;
            ac--; av++;
            if(ac && av->a_type == A_FLOAT)
                x->x_init_ynm1 = av->a_w.w_float;
                if(ac && av->a_type == A_FLOAT)
                    x->x_init_ynm2 = av->a_w.w_float;
    }
    x->x_u = u;
    for(int i = 0; i < x->x_list_size; i++){
        if(x->x_freq_list[i] >= 0)
            x->x_phase[i] = 1;
        x->x_ynm1[i] = x->x_init_ynm1;
        x->x_ynm2[i] = x->x_init_ynm2;
    }
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    x->x_glist = canvas_getcurrent();
    return(x);
errstate:
    post("[ikeda~]: improper args");
    return(NULL);
}

void ikeda_tilde_setup(void){
    ikeda_class = class_new(gensym("ikeda~"), (t_newmethod)ikeda_new,
        (t_method)ikeda_free, sizeof(t_ikeda), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(ikeda_class, nullfn, gensym("signal"), 0);
    class_addmethod(ikeda_class, (t_method)ikeda_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(ikeda_class, ikeda_list);
    class_addmethod(ikeda_class, (t_method)ikeda_set, gensym("set"), A_GIMME, 0);
    class_addmethod(ikeda_class, (t_method)ikeda_coeffs, gensym("coeffs"), A_GIMME, 0);
}
