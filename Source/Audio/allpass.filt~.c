// porres

#include <m_pd.h>
#include <stdlib.h>
#include <math.h>
#include "magic.h"

#define MAX_SECTIONS  64   // max number of biquad sections

#define PI 3.14159265358979323846
#define MAXLEN 1024

static t_class *allpass_filt_class;

typedef struct _allpass_filt{
    t_object    x_obj;
    double     *x_xnm1;
    double     *x_xnm2;
    double     *x_ynm1;
    double     *x_ynm2;
    float      *x_freq_list;
    float      *x_reson_list;
    t_inlet    *x_inlet_freq;
    t_inlet    *x_inlet_q;
    t_inlet    *x_inlet_fb;
    t_outlet   *x_out;
    t_int       x_f_list_size;
    t_int       x_q_list_size;
    t_int       x_sig1;
    t_int       x_sig2;
    t_int       x_sig3;
    t_int       x_ch1;
    t_int       x_ch2;
    t_int       x_ch3;
    t_int       x_ch4;
    double     *x_fb;
    int         x_bypass;
    int         x_n;
    int         x_nchs;
    int         x_n_sections; // number of biquad filters (2nd order sections)
    t_float     x_nyq;
    double      x_a0;
    double      x_a1;
    double      x_a2;
    double      x_b1;
    double      x_b2;
    double      x_f;
    double      x_reson;
    double      x_radcoeff;
    t_glist    *x_glist;
    t_float    *x_sigscalar1;
    t_float    *x_sigscalar2;
}t_allpass_filt;

void allpass_filt_clear(t_allpass_filt *x){
    for(int i = 0; i < x->x_n_sections * x->x_nchs; i++)
        x->x_xnm1[i] = x->x_xnm2[i] = x->x_ynm1[i] = x->x_ynm2[i] = 0.;
    for(int i = 0; i < x->x_nchs; i++)
        x->x_fb[i] = 0;
}

static void update_coeffs(t_allpass_filt *x, double f, double reson){
    x->x_f = f;
    double q = x->x_reson = reson;
    double omega = f * x->x_radcoeff;
    if(q < 0.000001){ // force bypass: y[n] = x[n] exactly
        x->x_a0 = 1, x->x_a1 = x->x_a2 = x->x_b1 = x->x_b2 = 0;
    }
    else{
        double alphaQ = sin(omega) / (2*q);
        double cos_w = cos(omega);
        double b0 = alphaQ + 1;
        x->x_a0 = (1 - alphaQ) / b0;
        x->x_a1 = -2*cos_w / b0;
        x->x_a2 = 1;
        x->x_b1 = -x->x_a1;
        x->x_b2 = (alphaQ - 1) / b0;
    }
}

static void allpass_filt_freq(t_allpass_filt *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(ac == 0)
        return;
    if(ac > MAXLEN)
        ac = MAXLEN;
    for(int i = 0; i < ac; i++)
        x->x_freq_list[i] = atom_getfloat(av+i);
    if(x->x_f_list_size != ac){
        x->x_f_list_size = ac;
        canvas_update_dsp();
    }
    else_magic_setnan(x->x_sigscalar1);
}

static void allpass_filt_reson(t_allpass_filt *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(ac == 0)
        return;
    if(ac > MAXLEN)
        ac = MAXLEN;
    for(int i = 0; i < ac; i++)
        x->x_reson_list[i] = atom_getfloat(av+i);
    if(x->x_q_list_size != ac){
        x->x_q_list_size = ac;
        canvas_update_dsp();
    }
    else_magic_setnan(x->x_sigscalar2);
}

void allpass_filt_bypass(t_allpass_filt *x, t_floatarg f){
    x->x_bypass = f != 0;
}

void allpass_filt_n(t_allpass_filt *x, t_floatarg f){
    int n = (int)f / 2;
    if(n <= 0)
        n = 1;
    if(n > MAX_SECTIONS)
        n = MAX_SECTIONS;
    if(n != x->x_n_sections){
        x->x_xnm1 = (double *)resizebytes(x->x_xnm1,
            x->x_n_sections * x->x_nchs * sizeof(*x->x_xnm1),
            n * x->x_nchs * sizeof(*x->x_xnm1));
        x->x_xnm2 = (double *)resizebytes(x->x_xnm2,
            x->x_n_sections * x->x_nchs * sizeof(*x->x_xnm2),
            n * x->x_nchs * sizeof(*x->x_xnm2));
        x->x_ynm1 = (double *)resizebytes(x->x_ynm1,
            x->x_n_sections * x->x_nchs * sizeof(*x->x_ynm1),
            n * x->x_nchs * sizeof(*x->x_ynm1));
        x->x_ynm2 = (double *)resizebytes(x->x_ynm2,
            x->x_n_sections * x->x_nchs * sizeof(*x->x_ynm2),
            n * x->x_nchs * sizeof(*x->x_ynm2));
        x->x_n_sections = n;
        allpass_filt_clear(x);
    }
}

static t_int *allpass_filt_perform(t_int *w){
    t_allpass_filt *x = (t_allpass_filt *)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *in3 = (t_float *)(w[4]);
    t_float *in4 = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);
    int n = x->x_n;
    int n_sections = x->x_n_sections;
    t_float nyq = x->x_nyq;
    if(!x->x_sig2){
        t_float *scalar = x->x_sigscalar1;
        if(!else_magic_isnan(*scalar)){
            x->x_ch2 = x->x_f_list_size = 1;
            x->x_freq_list[0] = *scalar;
            else_magic_setnan(scalar);
        }
    }
    if(!x->x_sig3){
        t_float *scalar = x->x_sigscalar2;
        if(!else_magic_isnan(*scalar)){
            x->x_ch3 = x->x_q_list_size = 1;
            x->x_reson_list[0] = *scalar;
            else_magic_setnan(scalar);
        }
    }
    for(int j = 0; j < x->x_nchs; j++){
        unsigned int offset = j * n_sections;
        for(int i = 0; i < n; i++){
            double xn, yn;
            double f, reson, fb;
            if(!x->x_sig1)
                xn = 0;
            else if(x->x_ch1 == 1)
                xn = in[i];
            else
                xn = in[j*n + i];
            if(x->x_ch2 == 1)
                f = x->x_sig2 ? in2[i] : x->x_freq_list[0];
            else
                f = x->x_sig2 ? in2[j*n + i] : x->x_freq_list[j];
            if(f < 0.000001)
                f = 0.000001;
            if(f > nyq - 0.000001)
                f = nyq - 0.000001;
            if(x->x_ch3 == 1)
                reson = x->x_sig3 ? in3[i] : x->x_reson_list[0];
            else
                reson = x->x_sig3 ? in3[j*n + i] : x->x_reson_list[j];
            if(x->x_ch4 == 1)
                fb = in4[i];
            else
                fb = in4[j*n + i];
            if(fb > 1)
                fb = 1;
            if(fb < -1)
                fb = -1;
            if(x->x_bypass){
                out[j*n + i] = xn;
                x->x_fb[j] = xn;
                continue;
            }
            xn += x->x_fb[j] * fb;
            if(f != x->x_f || reson != x->x_reson)
                update_coeffs(x, f, reson);
            for(int curfilt = 0; curfilt < n_sections; curfilt++){
                double xnm1 = x->x_xnm1[offset + curfilt];
                double xnm2 = x->x_xnm2[offset + curfilt];
                double ynm1 = x->x_ynm1[offset + curfilt];
                double ynm2 = x->x_ynm2[offset + curfilt];
                yn = x->x_a0*xn + x->x_a1*xnm1 + x->x_a2*xnm2
                   + x->x_b1*ynm1 + x->x_b2*ynm2;
                x->x_xnm2[offset + curfilt] = xnm1;
                x->x_xnm1[offset + curfilt] = xn;
                x->x_ynm2[offset + curfilt] = ynm1;
                x->x_ynm1[offset + curfilt] = yn;
                xn = yn;
            }
            out[j*n + i] = xn;
            x->x_fb[j] = xn;
        }
    }
    return(w+7);
}

static void allpass_filt_dsp(t_allpass_filt *x, t_signal **sp){
    t_float nyq = sp[0]->s_sr * 0.5;
    x->x_n = sp[0]->s_n;
    if(nyq != x->x_nyq){
        x->x_nyq = nyq;
        x->x_radcoeff = PI / (sp[0]->s_sr * 0.5);
        update_coeffs(x, x->x_f, x->x_reson);
    }
    x->x_sig1 = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    x->x_sig2 = else_magic_inlet_connection((t_object *)x, x->x_glist, 1, &s_signal);
    x->x_sig3 = else_magic_inlet_connection((t_object *)x, x->x_glist, 2, &s_signal);
    x->x_ch2 = x->x_sig2 ? sp[1]->s_nchans : x->x_f_list_size;
    x->x_ch3 = x->x_sig3 ? sp[2]->s_nchans : x->x_q_list_size;
    x->x_ch4 = sp[3]->s_nchans;
    int chs = x->x_ch1 = sp[0]->s_nchans;
    if(x->x_ch2 > chs)
        chs = x->x_ch2;
    if(x->x_ch3 > chs)
        chs = x->x_ch3;
    if(x->x_ch4 > chs)
        chs = x->x_ch4;
    if(chs != x->x_nchs){
        x->x_xnm1 = (double *)resizebytes(x->x_xnm1,
            x->x_nchs * x->x_n_sections * sizeof(double),
            chs * x->x_n_sections * sizeof(double));
        x->x_xnm2 = (double *)resizebytes(x->x_xnm2,
            x->x_nchs * x->x_n_sections * sizeof(double),
            chs * x->x_n_sections * sizeof(double));
        x->x_ynm1 = (double *)resizebytes(x->x_ynm1,
            x->x_nchs * x->x_n_sections * sizeof(double),
            chs * x->x_n_sections * sizeof(double));
        x->x_ynm2 = (double *)resizebytes(x->x_ynm2,
            x->x_nchs * x->x_n_sections * sizeof(double),
            chs * x->x_n_sections * sizeof(double));
        x->x_fb = (double *)resizebytes(x->x_fb,
            x->x_nchs * sizeof(double),
            chs * sizeof(double));
        x->x_nchs = chs;
        allpass_filt_clear(x);
    }
    signal_setmultiout(&sp[4], x->x_nchs);
    if((x->x_ch1 > 1 && x->x_ch1 != x->x_nchs)
    || (x->x_ch2 > 1 && x->x_ch2 != x->x_nchs)
    || (x->x_ch3 > 1 && x->x_ch3 != x->x_nchs)
    || (x->x_ch4 > 1 && x->x_ch4 != x->x_nchs)){
        dsp_add_zero(sp[4]->s_vec, x->x_nchs*x->x_n);
        pd_error(x, "[allpass.filt~]: channel sizes mismatch");
        return;
    }
    dsp_add(allpass_filt_perform, 6, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
}

static void allpass_filt_free(t_allpass_filt *x){
    inlet_free(x->x_inlet_freq);
    inlet_free(x->x_inlet_q);
    inlet_free(x->x_inlet_fb);
    outlet_free(x->x_out);
    freebytes(x->x_xnm1, x->x_n_sections * x->x_nchs * sizeof(*x->x_xnm1));
    freebytes(x->x_xnm2, x->x_n_sections * x->x_nchs * sizeof(*x->x_xnm2));
    freebytes(x->x_ynm1, x->x_n_sections * x->x_nchs * sizeof(*x->x_ynm1));
    freebytes(x->x_ynm2, x->x_n_sections * x->x_nchs * sizeof(*x->x_ynm2));
    freebytes(x->x_fb, x->x_nchs * sizeof(*x->x_fb));
    free(x->x_freq_list);
    free(x->x_reson_list);
}

void *allpass_filt_new(t_symbol *s, int ac, t_atom *av){
    t_allpass_filt *x = (t_allpass_filt *)pd_new(allpass_filt_class);
    float freq = 0.000001;
    float reson = 0;
    float fb = 0;
    int n = 2;
    int argnum = 0;
    x->x_freq_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_reson_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_f_list_size = x->x_q_list_size = 1;
    while(ac > 0){
        if(av->a_type == A_FLOAT){ //if current argument is a float
            t_float aval = atom_getfloat(av);
            switch(argnum){
                case 0:
                    n = aval;
                    break;
                case 1:
                    freq = aval;
                    break;
                case 2:
                    reson = aval;
                    break;
                default:
                case 3:
                    fb = aval;
                    break;
                    break;
            };
            argnum++;
            ac--, av++;
        }
        else
            goto errstate;
    };
    x->x_bypass = 0;
    x->x_nchs = 1;
    n = n / 2;
    if(n <= 0)
        n = 1;
    if(n > MAX_SECTIONS)
        n = MAX_SECTIONS;
    x->x_n_sections = n;
    if(fb <= -1)
        fb = -1;
    if(fb > 1)
        fb = 1;
    x->x_xnm1 = (double *)getbytes(x->x_n_sections * sizeof(double));
    x->x_xnm2 = (double *)getbytes(x->x_n_sections * sizeof(double));
    x->x_ynm1 = (double *)getbytes(x->x_n_sections * sizeof(double));
    x->x_ynm2 = (double *)getbytes(x->x_n_sections * sizeof(double));
    x->x_fb = (double *)getbytes(sizeof(double));
    x->x_fb[0] = 0;
    allpass_filt_clear(x);
    x->x_nyq = sys_getsr() * 0.5;
    x->x_radcoeff = PI / x->x_nyq;
    x->x_freq_list[0] = freq;
    x->x_reson_list[0] = reson;
    update_coeffs(x, freq, reson);
    x->x_inlet_freq = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_inlet_q = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_inlet_fb = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_fb, fb);
    x->x_out = outlet_new((t_object *)x, &s_signal);
    x->x_glist = canvas_getcurrent();
    x->x_sigscalar1 = obj_findsignalscalar((t_object *)x, 1);
    else_magic_setnan(x->x_sigscalar1);
    x->x_sigscalar2 = obj_findsignalscalar((t_object *)x, 2);
    else_magic_setnan(x->x_sigscalar2);
    return(x);
errstate:
    pd_error(x, "[allpass.filt~]: improper args");
    return(NULL);
}

void setup_allpass0x2efilt_tilde(void){
    allpass_filt_class = class_new(gensym("allpass.filt~"), (t_newmethod)allpass_filt_new,
        (t_method)allpass_filt_free, sizeof(t_allpass_filt), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(allpass_filt_class, nullfn, gensym("signal"), 0);
    class_addmethod(allpass_filt_class, (t_method)allpass_filt_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(allpass_filt_class, (t_method)allpass_filt_clear, gensym("clear"), 0);
    class_addmethod(allpass_filt_class, (t_method)allpass_filt_bypass, gensym("bypass"), A_FLOAT, 0);
    class_addmethod(allpass_filt_class, (t_method)allpass_filt_n, gensym("n"), A_FLOAT, 0);
    class_addmethod(allpass_filt_class, (t_method)allpass_filt_freq, gensym("freq"), A_GIMME, 0);
    class_addmethod(allpass_filt_class, (t_method)allpass_filt_reson, gensym("reson"), A_GIMME, 0);
}
