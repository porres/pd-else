// Porres 2017-2023

#include <m_pd.h>
#include <stdlib.h>
#include <math.h>
#include "magic.h"

#define MAXLEN 1024

typedef struct _gbman{
    t_object    x_obj;
    double     *x_phase;
    double     *x_y_nm1;
    double     *x_y_nm2;
    double     *x_x1;
    double     *x_y1;
    int         x_nchans;
    int         x_ch;
    t_int       x_n;
    t_int       x_sig;
    t_float     x_coeff1;
    t_float     x_coeff2;
    float      *x_freq_list;
    t_int       x_list_size;
    t_symbol   *x_ignore;
    t_outlet   *x_outlet;
    double      x_sr_rec;
    t_glist    *x_glist;
}t_gbman;

static t_class *gbman_class;

static void gbman_coeffs(t_gbman *x, t_symbol *s, int ac, t_atom * av){
    x->x_ignore = s;
    if(ac != 2)
        pd_error(x, "[gbman~]: number of coefficients needs to be = 2");
    else{
        int argnum = 0; // current argument
        while(ac){
            if(av -> a_type != A_FLOAT)
                pd_error(x, "[gbman~]: coefficient can't be a symbol");
            else{
                t_float curf = atom_getfloatarg(0, ac, av);
                switch(argnum){
                    case 0:
                        x->x_coeff1 = curf;
                        break;
                    case 1:
                        x->x_coeff2 = curf;
                        break;
                };
                argnum++;
            };
            ac--, av++;
        };
        for(int i = 0; i < x->x_nchans; i++){
            x->x_phase[i] = x->x_freq_list[i] >= 0;
            x->x_y_nm1[i] = x->x_coeff1;
            x->x_y_nm2[i] = x->x_coeff2;
        }
    }
}

static t_int *gbman_perform(t_int *w){
    t_gbman *x = (t_gbman *)(w[1]);
    int chs = (t_int)(w[2]); // number of channels in main input signal (density)
    t_float *freq = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    double *y_nm1 = x->x_y_nm1;
    double *y_nm2 = x->x_y_nm2;
    double *x1 = x->x_x1;
    double *y1 = x->x_y1;
    double *phase = x->x_phase;
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
            t_float output;
            if(hz >= 0){
                trig = phase[j] >= 1.;
                if(phase[j] >= 1.)
                    phase[j] -= 1;
            }
            else{
                trig = (phase[j] <= 0.);
                if(phase[j] <= 0.)
                    phase[j] += 1.;
            }
            if(trig){ // update
                output = 1 + fabs(y_nm1[j]) - y_nm2[j];
                y_nm2[j] = y_nm1[j];
                y_nm1[j] = output;
            }
            else
                output = y_nm1[j]; // last output
            double in = output;
            double dcblock = (in - x1[j]) + 0.9996*y1[j];
            x1[j] = in;
            y1[j] = dcblock;
            out[j*x->x_n + i] = output * 0.182 - 0.455;
            phase[j] += step;
        }
    }
    x->x_phase = phase;
    x->x_y_nm1 = y_nm1;
    x->x_y_nm2 = y_nm2;
    x->x_x1 = x1;
    x->x_y1 = y1;
    return(w+5);
}

static void gbman_dsp(t_gbman *x, t_signal **sp){
    x->x_n = sp[0]->s_n, x->x_sr_rec = 1.0 / (double)sp[0]->s_sr;
    x->x_sig = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    int chs = x->x_sig ? sp[0]->s_nchans : x->x_list_size;
    int nchans = chs;
    if(chs == 1)
        chs = x->x_ch;
    if(x->x_nchans != chs){
        x->x_phase = (double *)resizebytes(x->x_phase,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_y_nm1 = (double *)resizebytes(x->x_y_nm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_y_nm2 = (double *)resizebytes(x->x_y_nm2,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_x1 = (double *)resizebytes(x->x_x1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_y1 = (double *)resizebytes(x->x_y1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_nchans = chs;
        for(int i = 0; i < x->x_nchans; i++){
            if(x->x_freq_list[i] >= 0)
                x->x_phase[i] = 1;
            x->x_y_nm1[i] = x->x_coeff1;
            x->x_y_nm2[i] = x->x_coeff2;
        }
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(gbman_perform, 4, x, nchans, sp[0]->s_vec, sp[1]->s_vec);
}

static void gbman_list(t_gbman *x, t_symbol *s, int ac, t_atom * av){
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

static void gbman_set(t_gbman *x, t_symbol *s, int ac, t_atom *av){
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

static void *gbman_free(t_gbman *x){
    outlet_free(x->x_outlet);
    freebytes(x->x_phase, x->x_nchans * sizeof(*x->x_phase));
    freebytes(x->x_y_nm1, x->x_nchans * sizeof(*x->x_y_nm1));
    freebytes(x->x_y_nm2, x->x_nchans * sizeof(*x->x_y_nm2));
    freebytes(x->x_x1, x->x_nchans * sizeof(*x->x_x1));
    freebytes(x->x_y1, x->x_nchans * sizeof(*x->x_y1));
    return(void *)x;
}

static void *gbman_new(t_symbol *s, int ac, t_atom *av){
    t_gbman *x = (t_gbman *)pd_new(gbman_class);
    x->x_ignore = s;
    x->x_list_size = 1;
    x->x_nchans = 1;
    x->x_ch = 1;
    x->x_freq_list = (float*)malloc(MAXLEN * sizeof(float));
    
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
    t_float y1 = 1.2, y2 = 2.1; // default parameters
    if(ac && av->a_type == A_FLOAT){
        x->x_freq_list[0] = av->a_w.w_float;
        ac--; av++;
        if(ac && av->a_type == A_FLOAT)
            y1 = av->a_w.w_float;
            ac--; av++;
            if(ac && av->a_type == A_FLOAT)
                y2 = av->a_w.w_float;
    }
    
    x->x_phase = (double *)getbytes(sizeof(*x->x_phase) * x->x_list_size);
    x->x_y_nm1 = (double *)getbytes(sizeof(*x->x_y_nm1));
    x->x_y_nm2 = (double *)getbytes(sizeof(*x->x_y_nm2));
    x->x_x1 = (double *)getbytes(sizeof(*x->x_x1));
    x->x_y1 = (double *)getbytes(sizeof(*x->x_y1));
    x->x_freq_list[0] = sys_getsr() * 0.5;
    x->x_phase[0] = x->x_y_nm1[0] = 0;
    x->x_y_nm2[0] = x->x_x1[0] = x->x_y1[0] = 0;

    
    x->x_coeff1 = y1;
    x->x_coeff2 = y2;
    
    for(int i = 0; i < x->x_list_size; i++){
        if(x->x_freq_list[i] >= 0)
            x->x_phase[i] = 1;
        x->x_y_nm1[0] = x->x_coeff1;
        x->x_y_nm2[0] = x->x_coeff2;
    }
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    x->x_glist = canvas_getcurrent();
    return(x);
errstate:
    post("[gbman~]: improper args");
    return(NULL);
}

void gbman_tilde_setup(void){
    gbman_class = class_new(gensym("gbman~"), (t_newmethod)gbman_new, (t_method)gbman_free,
        sizeof(t_gbman), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(gbman_class, nullfn, gensym("signal"), 0);
    class_addmethod(gbman_class, (t_method)gbman_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(gbman_class, gbman_list);
    class_addmethod(gbman_class, (t_method)gbman_set, gensym("set"), A_GIMME, 0);
    class_addmethod(gbman_class, (t_method)gbman_coeffs, gensym("coeffs"), A_GIMME, 0);
}
