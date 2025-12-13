// Porres 2017

#include "m_pd.h"
#include "magic.h"
#include <math.h>
#include <stdlib.h>

#define PI 3.14159265358979323846
#define MAXLEN 1024

typedef struct _formlet{
    t_object    x_obj;
    t_int       x_n;
    int         x_nchans;
    t_int       x_sig1;
    t_int       x_sig3;
    t_int       x_sig4;
    t_int       x_ch1;
    t_int       x_ch2;
    t_int       x_ch3;
    t_int       x_ch4;
    t_int       x_f_list_size;
    t_int       x_a_list_size;
    t_int       x_d_list_size;
    float      *x_f_list, *x_a_list, *x_d_list;
    t_inlet    *x_inlet_excitation;
    t_inlet    *x_inlet_t1;
    t_inlet    *x_inlet_t2;
    t_float     x_nyq;
    t_float     x_freq;
    double     *x_x1nm1, *x_x1nm2;
    double     *x_x2nm1, *x_x2nm2;
    double     *x_y1nm1, *x_y1nm2;
    double     *x_y2nm1, *x_y2nm2;
    t_glist    *x_glist;
    t_float    *x_sigscalar1;
    t_float    *x_sigscalar3;
    t_float    *x_sigscalar4;
    t_symbol   *x_ignore;
}t_formlet;

static t_class *formlet_class;

static t_int *formlet_perform(t_int *w){
    t_formlet *x = (t_formlet *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *in3 = (t_float *)(w[4]);
    t_float *in4 = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);
    double *x1nm1 = x->x_x1nm1, *x1nm2 = x->x_x1nm2;
    double *x2nm1 = x->x_x2nm1, *x2nm2 = x->x_x2nm2;
    double *y1nm1 = x->x_y1nm1, *y1nm2 = x->x_y1nm2;
    double *y2nm1 = x->x_y2nm1, *y2nm2 = x->x_y2nm2;
    t_float nyq = x->x_nyq;
    if(!x->x_sig1){
        t_float *scalar = x->x_sigscalar1;
        if(!else_magic_isnan(*x->x_sigscalar1)){
            t_float freq = *scalar;
            x->x_ch1 = x->x_f_list_size = 1;
            x->x_f_list[0] = freq;
            else_magic_setnan(x->x_sigscalar1);
        }
    }
    if(!x->x_sig3){
        t_float *scalar = x->x_sigscalar3;
        if(!else_magic_isnan(*x->x_sigscalar3)){
            t_float a = *scalar;
            x->x_ch3 = x->x_a_list_size = 1;
            x->x_a_list[0] = a;
            else_magic_setnan(x->x_sigscalar3);
        }
    }
    if(!x->x_sig4){
        t_float *scalar = x->x_sigscalar4;
        if(!else_magic_isnan(*x->x_sigscalar4)){
            t_float d = *scalar;
            x->x_ch4 = x->x_d_list_size = 1;
            x->x_d_list[0] = d;
            else_magic_setnan(x->x_sigscalar4);
        }
    }
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0, n = x->x_n; i < n; i++){
            double f, xn, t1, t2;
            if(x->x_ch1 == 1)
                f = x->x_sig1 ? in1[i] : x->x_f_list[0];
            else
                f = x->x_sig1 ? in1[j*n + i] : x->x_f_list[j];
            if(f < 0.000001)
                f = 0.000001;
            if(f > nyq - 0.000001)
                f = nyq - 0.000001;
            if(x->x_ch2 == 1)
                xn = in2[i];
            else
                xn = in2[j*n + i];
            if(x->x_ch3 == 1)
                t1 = x->x_sig3 ? in3[i] : x->x_a_list[0];
            else
                t1 = x->x_sig3 ? in3[j*n + i] : x->x_a_list[j];
            if(x->x_ch4 == 1)
                t2 = x->x_sig4 ? in4[i] : x->x_d_list[0];
            else
                t2 = x->x_sig4 ? in4[j*n + i] : x->x_d_list[j];
//            post("f = %f / t1 = %f / t2 = %f", f, t1, t2);
            double q, omega, alphaQ, cos_w, a0, a2, b0, b1, b2, y1n, y2n;
            double a = 0, b = 0;
            if(f < 0.000001)
                f = 0.000001;
            if(f > nyq - 0.000001)
                f = nyq - 0.000001;
            if(t1 <= 0)
                y1n = 0; // attack = 0
            else{
                a = 1000 * log(1000) / t1;
                q = f * (PI * t1/1000) / log(1000); // t60
                omega = f * PI/nyq;
                alphaQ = sin(omega) / (2*q);
                cos_w = cos(omega);
                b0 = alphaQ + 1;
                a0 = alphaQ*q / b0;
                a2 = -a0;
                b1 = 2*cos_w / b0;
                b2 = (alphaQ - 1) / b0;
                y1n = a0 * xn + a2 * x1nm2[j] + b1 * y1nm1[j] + b2 * y1nm2[j];
            }
            if(t2 <= 0)
                y2n = xn; // no decay
            else{
                b = 1000 * log(1000) / t2;
                q = f * (PI * t2/1000) / log(1000); // t60
                omega = f * PI/nyq;
                alphaQ = sin(omega) / (2*q);
                cos_w = cos(omega);
                b0 = alphaQ + 1;
                a0 = alphaQ*q / b0;
                a2 = -a0;
                b1 = 2*cos_w / b0;
                b2 = (alphaQ - 1) / b0;
                y2n = a0 * xn + a2 * x2nm2[j] + b1 * y2nm1[j] + b2 * y2nm2[j];
            }
            if((t1 > 0 && t2 > 0) && (t1 != t2)){
                double t = log(a/b) / (a-b);
                double amp = fabs(1/(exp(-b*t) - exp(-a*t)));
                out[j*n + i] = (y2n - y1n) * amp; // decay - attack
            }
            else
                out[j*n + i] = y2n - y1n;
            x1nm2[j] = x1nm1[j];
            x1nm1[j] = xn;
            y1nm2[j] = y1nm1[j];
            y1nm1[j] = y1n;
            x2nm2[j] = x2nm1[j];
            x2nm1[j] = xn;
            y2nm2[j] = y2nm1[j];
            y2nm1[j] = y2n;
        }
    }
    x->x_x1nm1 = x1nm1, x->x_x1nm2 = x1nm2;
    x->x_x2nm1 = x2nm1, x->x_x2nm2 = x2nm2;
    x->x_y1nm1 = y1nm1, x->x_y1nm2 = y1nm2;
    x->x_y2nm1 = y2nm1, x->x_y2nm2 = y2nm2;
    return(w+7);
}

static void formlet_dsp(t_formlet *x, t_signal **sp){
    int nyq = sp[0]->s_sr / 2;
    x->x_n = sp[0]->s_n;
    if(nyq != x->x_nyq){
        x->x_nyq = nyq;
//        x->x_radcoeff = PI / x->x_nyq;
//        update_coeffs(x, x->x_f, x->x_reson);
    }
    x->x_sig1 = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    x->x_sig3 = else_magic_inlet_connection((t_object *)x, x->x_glist, 2, &s_signal);
    x->x_sig4 = else_magic_inlet_connection((t_object *)x, x->x_glist, 3, &s_signal);
    int chs = x->x_ch1 = x->x_sig1 ? sp[0]->s_nchans : x->x_f_list_size;
    x->x_ch2 = sp[1]->s_nchans;
    x->x_ch3 = x->x_sig3 ? sp[2]->s_nchans : x->x_a_list_size;
    x->x_ch4 = x->x_sig4 ? sp[3]->s_nchans : x->x_d_list_size;
    if(x->x_ch2 > chs)
        chs = x->x_ch2;
    if(x->x_ch3 > chs)
        chs = x->x_ch3;
    if(x->x_ch4 > chs)
        chs = x->x_ch4;
    if(x->x_nchans != chs){
        x->x_x1nm1 = (double *)resizebytes(x->x_x1nm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_x1nm2 = (double *)resizebytes(x->x_x1nm2,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_x2nm1 = (double *)resizebytes(x->x_x2nm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_x2nm2 = (double *)resizebytes(x->x_x2nm2,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_y1nm1 = (double *)resizebytes(x->x_y1nm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_y1nm2 = (double *)resizebytes(x->x_y1nm2,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_y2nm1 = (double *)resizebytes(x->x_y2nm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_y2nm2 = (double *)resizebytes(x->x_y2nm2,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[4], x->x_nchans);
    if((x->x_ch1 > 1 && x->x_ch1 != x->x_nchans)
    || (x->x_ch2 > 1 && x->x_ch2 != x->x_nchans)
    || (x->x_ch3 > 1 && x->x_ch3 != x->x_nchans)
    || (x->x_ch4 > 1 && x->x_ch4 != x->x_nchans)){
        dsp_add_zero(sp[4]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[formlet~]: channel sizes mismatch");
        return;
    }
    dsp_add(formlet_perform, 6, x, sp[0]->s_vec,sp[1]->s_vec,
            sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
}

static void formlet_clear(t_formlet *x){
    for(int i = 0; i < x->x_nchans; i++){
        x->x_x1nm1[i] = x->x_x1nm2[i] = x->x_y1nm1[i] = x->x_y1nm2[i] = 0.;
        x->x_x2nm1[i] = x->x_x2nm2[i] = x->x_y2nm1[i] = x->x_y2nm2[i] = 0.;
    }
}

static void formlet_freq(t_formlet *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    for(int i = 0; i < ac; i++)
        x->x_f_list[i] = atom_getfloat(av+i);
    if(x->x_f_list_size != ac){
        x->x_f_list_size = ac;
        canvas_update_dsp();
    }
    else_magic_setnan(x->x_sigscalar1);
}

static void formlet_attack(t_formlet *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    for(int i = 0; i < ac; i++)
        x->x_a_list[i] = atom_getfloat(av+i);
    if(x->x_a_list_size != ac){
        x->x_a_list_size = ac;
        canvas_update_dsp();
    }
    else_magic_setnan(x->x_sigscalar3);
}

static void formlet_decay(t_formlet *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    for(int i = 0; i < ac; i++)
        x->x_d_list[i] = atom_getfloat(av+i);
    if(x->x_d_list_size != ac){
        x->x_d_list_size = ac;
        canvas_update_dsp();
    }
    else_magic_setnan(x->x_sigscalar4);
}

static void *formlet_free(t_formlet *x){
    inlet_free(x->x_inlet_excitation);
    inlet_free(x->x_inlet_t1);
    inlet_free(x->x_inlet_t2);
    freebytes(x->x_x1nm1, x->x_nchans * sizeof(*x->x_x1nm1));
    freebytes(x->x_x1nm2, x->x_nchans * sizeof(*x->x_x1nm2));
    freebytes(x->x_x2nm1, x->x_nchans * sizeof(*x->x_x2nm1));
    freebytes(x->x_x2nm2, x->x_nchans * sizeof(*x->x_x2nm2));
    freebytes(x->x_y1nm1, x->x_nchans * sizeof(*x->x_y1nm1));
    freebytes(x->x_y1nm2, x->x_nchans * sizeof(*x->x_y1nm2));
    freebytes(x->x_y2nm1, x->x_nchans * sizeof(*x->x_y2nm1));
    freebytes(x->x_y2nm2, x->x_nchans * sizeof(*x->x_y2nm2));
    free(x->x_f_list);
    free(x->x_a_list);
    free(x->x_d_list);
    return(void *)x;
}

static void *formlet_new(t_symbol *s, int ac, t_atom *av){
    t_formlet *x = (t_formlet *)pd_new(formlet_class);
    x->x_ignore = s;
    x->x_freq = 0.000001;
    float reson1 = 0, reson2 = 0;
    x->x_x1nm1 = (double *)getbytes(sizeof(*x->x_x1nm1));
    x->x_x1nm2 = (double *)getbytes(sizeof(*x->x_x1nm2));
    x->x_x2nm1 = (double *)getbytes(sizeof(*x->x_x2nm1));
    x->x_x2nm2 = (double *)getbytes(sizeof(*x->x_x2nm2));
    x->x_y1nm1 = (double *)getbytes(sizeof(*x->x_y1nm1));
    x->x_y1nm2 = (double *)getbytes(sizeof(*x->x_y1nm2));
    x->x_y2nm1 = (double *)getbytes(sizeof(*x->x_y2nm1));
    x->x_y2nm2 = (double *)getbytes(sizeof(*x->x_y2nm2));
    x->x_f_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_a_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_d_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_x1nm1[0] = x->x_x1nm2[0] = x->x_x2nm1[0] = x->x_x2nm2[0] = 0.;
    x->x_y1nm1[0] = x->x_y1nm2[0] = x->x_y2nm1[0] = x->x_y2nm2[0] = 0.;
    x->x_f_list_size = x->x_a_list_size = x->x_d_list_size = 1;
/////////////////////////////////////////////////////////////////////////////////////
    int argnum = 0;
    while(ac > 0){
        if(av -> a_type == A_FLOAT){ //if current argument is a float
            t_float aval = atom_getfloat(av);
            switch(argnum){
                case 0:
                    x->x_freq = aval;
                    break;
                case 1:
                    reson1 = aval;
                    break;
                case 2:
                    reson2 = aval;
                    break;
                default:
                    break;
            };
            argnum++;
            ac--;
            av++;
        }
        else
            goto errstate;
    };
/////////////////////////////////////////////////////////////////////////////////////
    x->x_f_list[0] = x->x_freq, x->x_a_list[0] = reson1, x->x_d_list[0] = reson2;
    x->x_inlet_excitation = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_inlet_t1 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_t1, reson1);
    x->x_inlet_t2 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_t2, reson2);
    outlet_new((t_object *)x, &s_signal);
    x->x_glist = canvas_getcurrent();
    x->x_sigscalar1 = obj_findsignalscalar((t_object *)x, 0);
    else_magic_setnan(x->x_sigscalar1);
    x->x_sigscalar3 = obj_findsignalscalar((t_object *)x, 2);
    else_magic_setnan(x->x_sigscalar3);
    x->x_sigscalar4 = obj_findsignalscalar((t_object *)x, 3);
    else_magic_setnan(x->x_sigscalar4);
    return(x);
errstate:
    pd_error(x, "[formlet~]: improper args");
    return(NULL);
}

void formlet_tilde_setup(void){
    formlet_class = class_new(gensym("formlet~"), (t_newmethod)(void*)formlet_new,
        (t_method)formlet_free, sizeof(t_formlet), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(formlet_class, t_formlet, x_freq);
    class_addmethod(formlet_class, (t_method)formlet_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(formlet_class, (t_method)formlet_clear, gensym("clear"), 0);
    class_addmethod(formlet_class, (t_method)formlet_freq, gensym("freq"), A_GIMME, 0);
    class_addmethod(formlet_class, (t_method)formlet_attack, gensym("attack"), A_GIMME, 0);
    class_addmethod(formlet_class, (t_method)formlet_decay, gensym("decay"), A_GIMME, 0);
}
