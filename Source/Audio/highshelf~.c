// Porres 2017

#include <m_pd.h>
#include <stdlib.h>
#include <math.h>
#include "magic.h"

#define PI 3.14159265358979323846
#define MAXLEN 1024

typedef struct _highshelf{
    t_object    x_obj;
    t_int       x_n;
    int         x_nchans;
    t_int       x_sig2;
    t_int       x_sig3;
    t_int       x_sig4;
    t_int       x_ch1;
    t_int       x_ch2;
    t_int       x_ch3;
    t_int       x_ch4;
    double     *x_xnm1;
    double     *x_xnm2;
    double     *x_ynm1;
    double     *x_ynm2;
    float      *x_freq_list;
    float      *x_slope_list;
    float      *x_db_list;
    t_int       x_f_list_size;
    t_int       x_q_list_size;
    t_int       x_db_list_size;
    t_inlet    *x_inlet_freq;
    t_inlet    *x_inlet_slope;
    t_inlet    *x_inlet_db;
    t_outlet   *x_out;
    t_float     x_nyq;
    int         x_bypass;
    double      x_radcoeff;
    double      x_f;
    double      x_slope;
    double      x_db;
    double      x_a0;
    double      x_a1;
    double      x_a2;
    double      x_b1;
    double      x_b2;
    t_glist    *x_glist;
    t_float    *x_sigscalar1;
    t_float    *x_sigscalar2;
    t_float    *x_sigscalar3;
    t_symbol   *x_ignore;
}t_highshelf;

static t_class *highshelf_class;

static void highshelf_freq(t_highshelf *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    for(int i = 0; i < ac; i++)
        x->x_freq_list[i] = atom_getfloat(av+i);
    if(x->x_f_list_size != ac){
        x->x_f_list_size = ac;
        canvas_update_dsp();
    }
    else_magic_setnan(x->x_sigscalar1);
}

static void highshelf_slope(t_highshelf *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    for(int i = 0; i < ac; i++)
        x->x_slope_list[i] = atom_getfloat(av+i);
    if(x->x_q_list_size != ac){
        x->x_q_list_size = ac;
        canvas_update_dsp();
    }
    else_magic_setnan(x->x_sigscalar2);
}

static void highshelf_db(t_highshelf *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    for(int i = 0; i < ac; i++)
        x->x_db_list[i] = atom_getfloat(av+i);
    if(x->x_db_list_size != ac){
        x->x_db_list_size = ac;
        canvas_update_dsp();
    }
    else_magic_setnan(x->x_sigscalar3);
}

static void update_coeffs(t_highshelf *x, double f, double slope, double db){
    x->x_f = f;
    x->x_slope = slope;
    x->x_db = db;
    double amp = pow(10, db / 40);
    double omega = f * PI/x->x_nyq;
    double alphaS = sin(omega) * sqrt((amp*amp + 1) * (1/slope - 1) + 2*amp);
    double cos_w = cos(omega);
    double b0 = (amp+1) - (amp-1)*cos_w + alphaS;
    x->x_a0 = amp*(amp+1 + (amp-1)*cos_w + alphaS) / b0;
    x->x_a1 = -2*amp*(amp-1 + (amp+1)*cos_w) / b0;
    x->x_a2 = amp*(amp+1 + (amp-1)*cos_w - alphaS) / b0;
    x->x_b1 = -2*(amp-1 - (amp+1)*cos_w) / b0;
    x->x_b2 = -(amp+1 - (amp-1)*cos_w - alphaS) / b0;
}

static t_int *highshelf_perform(t_int *w){
    t_highshelf *x = (t_highshelf *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *in3 = (t_float *)(w[4]);
    t_float *in4 = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);
    double *xnm1 = x->x_xnm1, *xnm2 = x->x_xnm2;
    double *ynm1 = x->x_ynm1, *ynm2 = x->x_ynm2;
    t_float nyq = x->x_nyq;
    if(!x->x_sig2){
        t_float *scalar = x->x_sigscalar1;
        if(!else_magic_isnan(*x->x_sigscalar1)){
            t_float freq = *scalar;
            x->x_ch2 = x->x_f_list_size = 1;
            x->x_freq_list[0] = freq;
            else_magic_setnan(x->x_sigscalar1);
        }
    }
    if(!x->x_sig3){
        t_float *scalar = x->x_sigscalar2;
        if(!else_magic_isnan(*x->x_sigscalar2)){
            t_float slope = *scalar;
            x->x_ch3 = x->x_q_list_size = 1;
            x->x_slope_list[0] = slope;
            else_magic_setnan(x->x_sigscalar2);
        }
    }
    if(!x->x_sig4){
        t_float *scalar = x->x_sigscalar3;
        if(!else_magic_isnan(*x->x_sigscalar3)){
            t_float db = *scalar;
            x->x_ch4 = x->x_db_list_size = 1;
            x->x_db_list[0] = db;
            else_magic_setnan(x->x_sigscalar3);
        }
    }
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0, n = x->x_n; i < n; i++){
            double xn, yn, f, slope, db;
            if(x->x_ch1 == 1)
                xn = in1[i];
            else
                xn = in1[j*n + i];
            if(x->x_ch2 == 1)
                f = x->x_sig2 ? in2[i] : x->x_freq_list[0];
            else
                f = x->x_sig2 ? in2[j*n + i] : x->x_freq_list[j];
            if(f < 0.000001)
                f = 0.000001;
            if(f > nyq - 0.000001)
                f = nyq - 0.000001;
            if(x->x_ch3 == 1)
                slope = x->x_sig3 ? in3[i] : x->x_slope_list[0];
            else
                slope = x->x_sig3 ? in3[j*n + i] : x->x_slope_list[j];
            if(slope < 0.000001)
                slope = 0.000001;
            if(slope > 1)
                slope = 1;
            if(x->x_ch4 == 1)
                db = x->x_sig4 ? in4[i] : x->x_db_list[0];
            else
                db = x->x_sig4 ? in4[j*n + i] : x->x_db_list[j];
            if(x->x_bypass)
                out[j*n + i] = xn;
            else{
                if((f != x->x_f || slope != x->x_slope) || db != x->x_db)
                    update_coeffs(x, f, slope, db);
                yn = x->x_a0 * xn + x->x_a1 * xnm1[j] + x->x_a2 * xnm2[j] + x->x_b1 * ynm1[j] + x->x_b2 * ynm2[j];
                out[j*n + i] = yn;
                xnm2[j] = xnm1[j];
                xnm1[j] = xn;
                ynm2[j] = ynm1[j];
                ynm1[j] = yn;
            }
        }
    }
    x->x_xnm1 = xnm1, x->x_xnm2 = xnm2;
    x->x_ynm1 = ynm1, x->x_ynm2 = ynm2;
    return(w+7);
}

static void highshelf_dsp(t_highshelf *x, t_signal **sp){
    t_float nyq = sp[0]->s_sr * 0.5;
    x->x_n = sp[0]->s_n;
    if(nyq != x->x_nyq){
        x->x_nyq = nyq;
        x->x_radcoeff = PI / x->x_nyq;
        update_coeffs(x, x->x_f, x->x_slope, x->x_db);
    }
    x->x_sig2 = else_magic_inlet_connection((t_object *)x, x->x_glist, 1, &s_signal);
    x->x_sig3 = else_magic_inlet_connection((t_object *)x, x->x_glist, 2, &s_signal);
    x->x_sig4 = else_magic_inlet_connection((t_object *)x, x->x_glist, 3, &s_signal);
    int chs = x->x_ch1 = sp[0]->s_nchans;
    x->x_ch2 = x->x_sig2 ? sp[1]->s_nchans : x->x_f_list_size;
    x->x_ch3 = x->x_sig3 ? sp[2]->s_nchans : x->x_q_list_size;
    x->x_ch4 = x->x_sig4 ? sp[3]->s_nchans : x->x_db_list_size;
    if(x->x_ch2 > chs)
        chs = x->x_ch2;
    if(x->x_ch3 > chs)
        chs = x->x_ch3;
    if(x->x_ch4 > chs)
        chs = x->x_ch4;
    if(x->x_nchans != chs){
        x->x_xnm1 = (double *)resizebytes(x->x_xnm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_xnm2 = (double *)resizebytes(x->x_xnm2,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_ynm1 = (double *)resizebytes(x->x_ynm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_ynm2 = (double *)resizebytes(x->x_ynm2,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[4], x->x_nchans);
    if((x->x_ch1 > 1 && x->x_ch1 != x->x_nchans)
    || (x->x_ch2 > 1 && x->x_ch2 != x->x_nchans)
    || (x->x_ch3 > 1 && x->x_ch3 != x->x_nchans)
    || (x->x_ch4 > 1 && x->x_ch4 != x->x_nchans)){
        dsp_add_zero(sp[4]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[highshelf~]: channel sizes mismatch");
        return;
    }
    dsp_add(highshelf_perform, 6, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
}

static void highshelf_clear(t_highshelf *x){
    for(int i = 0; i < x->x_nchans; i++)
        x->x_xnm1[i] = x->x_xnm2[i] = x->x_ynm1[i] = x->x_ynm2[i] = 0.;
}

static void highshelf_bypass(t_highshelf *x, t_floatarg f){
    x->x_bypass = (int)(f != 0);
}

static void *highshelf_free(t_highshelf *x){
    inlet_free(x->x_inlet_freq);
    inlet_free(x->x_inlet_slope);
    inlet_free(x->x_inlet_db);
    outlet_free(x->x_out);
    freebytes(x->x_xnm1, x->x_nchans * sizeof(*x->x_xnm1));
    freebytes(x->x_xnm2, x->x_nchans * sizeof(*x->x_xnm2));
    freebytes(x->x_ynm1, x->x_nchans * sizeof(*x->x_ynm1));
    freebytes(x->x_ynm2, x->x_nchans * sizeof(*x->x_ynm2));
    free(x->x_freq_list);
    free(x->x_slope_list);
    free(x->x_db_list);
    return(void *)x;
}

static void *highshelf_new(t_symbol *s, int ac, t_atom *av){
    t_highshelf *x = (t_highshelf *)pd_new(highshelf_class);
    x->x_ignore = s;
    float freq = 0.000001;
    float slope = 0;
    float db = 0;
    int bw = 0;
    int argnum = 0;
    x->x_xnm1 = (double *)getbytes(sizeof(*x->x_xnm1));
    x->x_xnm2 = (double *)getbytes(sizeof(*x->x_xnm2));
    x->x_ynm1 = (double *)getbytes(sizeof(*x->x_ynm1));
    x->x_ynm2 = (double *)getbytes(sizeof(*x->x_ynm2));
    x->x_freq_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_slope_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_db_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_xnm1[0] = x->x_xnm2[0] = 0;
    x->x_ynm1[0] = x->x_ynm2[0] = 0;
    x->x_f_list_size = x->x_q_list_size = x->x_db_list_size = 1;
    while(ac > 0){
        if(av->a_type == A_FLOAT){ //if current argument is a float
            t_float aval = atom_getfloat(av);
            switch(argnum){
                case 0:
                    freq = aval;
                    break;
                case 1:
                    slope = aval;
                    break;
                case 2:
                    db = aval;
                    break;
                default:
                    break;
            };
            argnum++;
            ac--, av++;
        }
        else
            goto errstate;
    };
    x->x_nyq = sys_getsr() * 0.5;
    x->x_radcoeff = PI / x->x_nyq;
    if(slope < 0.000001)
        slope = 0.000001;
    if(slope > 1)
        slope = 1;
    x->x_freq_list[0] = freq, x->x_slope_list[0] = slope, x->x_db_list[0] = db;
    update_coeffs(x, (double)freq, (double)slope, (double)db);
    x->x_inlet_freq = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_freq, freq);
    x->x_inlet_slope = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_slope, slope);
    x->x_inlet_db = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_slope, db);
    x->x_out = outlet_new((t_object *)x, &s_signal);
    x->x_glist = canvas_getcurrent();
    x->x_sigscalar1 = obj_findsignalscalar((t_object *)x, 1);
    else_magic_setnan(x->x_sigscalar1);
    x->x_sigscalar2 = obj_findsignalscalar((t_object *)x, 2);
    else_magic_setnan(x->x_sigscalar2);
    x->x_sigscalar3 = obj_findsignalscalar((t_object *)x, 3);
    else_magic_setnan(x->x_sigscalar3);
    return(x);
errstate:
    pd_error(x, "[highshelf~]: improper args");
    return(NULL);
}

void highshelf_tilde_setup(void){
    highshelf_class = class_new(gensym("highshelf~"), (t_newmethod)highshelf_new,
        (t_method)highshelf_free, sizeof(t_highshelf), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(highshelf_class, (t_method)highshelf_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(highshelf_class, nullfn, gensym("signal"), 0);
    class_addmethod(highshelf_class, (t_method)highshelf_clear, gensym("clear"), 0);
    class_addmethod(highshelf_class, (t_method)highshelf_bypass, gensym("bypass"), A_DEFFLOAT, 0);
    class_addmethod(highshelf_class, (t_method)highshelf_freq, gensym("freq"), A_GIMME, 0);
    class_addmethod(highshelf_class, (t_method)highshelf_slope, gensym("slope"), A_GIMME, 0);
    class_addmethod(highshelf_class, (t_method)highshelf_db, gensym("db"), A_GIMME, 0);
}
