// Porres 2017

#include <m_pd.h>
#include <math.h>

#define PI 3.14159265358979323846

typedef struct _resonator{
    t_object    x_obj;
    t_int       x_n;
    t_inlet    *x_inlet_freq;
    t_inlet    *x_inlet_q;
    t_outlet   *x_out;
    t_float     x_nyq;
    double      x_xnm1;
    double      x_xnm2;
    double      x_ynm1;
    double      x_ynm2;
    double      x_f;
    double      x_reson;
    double      x_a0;
    double      x_a2;
    double      x_b1;
    double      x_b2;
}t_resonator;

static t_class *resonator_class;

static void update_coeffs(t_resonator *x, double f, double reson){
    x->x_f = f;
    x->x_reson = reson;
    double omega = f * PI/x->x_nyq;
    double q = f * (PI * reson/1000) / log(1000);
    if(q < 0.000001) // force bypass
        x->x_a0 = 1, x->x_a2 = x->x_b1 = x->x_b2 = 0;
    else{
        double alphaQ = sin(omega) / (2*q);
        double b0 = alphaQ + 1;
        x->x_a0 = alphaQ*q / b0;
        x->x_a2 = -x->x_a0;
        x->x_b1 = 2*cos(omega) / b0;
        x->x_b2 = (alphaQ - 1) / b0;
    }
}

static t_int *resonator_perform(t_int *w){
    t_resonator *x = (t_resonator *)(w[1]);
    int nblock = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *in3 = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);
    double xnm1 = x->x_xnm1;
    double xnm2 = x->x_xnm2;
    double ynm1 = x->x_ynm1;
    double ynm2 = x->x_ynm2;
    t_float nyq = x->x_nyq;
    while(nblock--){
        double xn = *in1++, f = *in2++, reson = *in3++, yn;
        if(f < 0.000001)
            f = 0.000001;
        if(f > nyq - 0.000001)
            f = nyq - 0.000001;
        if(f != x->x_f || reson != x->x_reson)
            update_coeffs(x, (double)f, (double)reson);
        yn = x->x_a0 * xn + x->x_a2 * xnm2 + x->x_b1 * ynm1 + x->x_b2 * ynm2;
        *out++ = yn;
        xnm2 = xnm1;
        xnm1 = xn;
        ynm2 = ynm1;
        ynm1 = yn;
    }
    x->x_xnm1 = xnm1;
    x->x_xnm2 = xnm2;
    x->x_ynm1 = ynm1;
    x->x_ynm2 = ynm2;
    return(w+7);
}

static void resonator_dsp(t_resonator *x, t_signal **sp){
    t_float nyq = sp[0]->s_sr / 2;
    if(nyq != x->x_nyq){
        x->x_nyq = nyq;
        update_coeffs(x, x->x_f, x->x_reson);
    }
    dsp_add(resonator_perform, 6, x, sp[0]->s_n, sp[0]->s_vec,sp[1]->s_vec, sp[2]->s_vec,
            sp[3]->s_vec);
}

static void resonator_clear(t_resonator *x){
    x->x_xnm1 = x->x_xnm2 = x->x_ynm1 = x->x_ynm2 = 0.;
}

static void *resonator_new(t_symbol *s, int argc, t_atom *argv){
    s = NULL;
    t_resonator *x = (t_resonator *)pd_new(resonator_class);
    float freq = 0.000001;
    float reson = 0;
    int argnum = 0;
    while(argc > 0){
        if(argv->a_type == A_FLOAT){ //if current argument is a float
            t_float argval = atom_getfloat(argv);
            switch(argnum){
                case 0:
                    freq = argval;
                    break;
                case 1:
                    reson = argval;
                    break;
                default:
                    break;
            };
            argnum++;
            argc--, argv++;
        }
        else if(argv->a_type == A_SYMBOL && !argnum){
            goto errstate;
        }
        else
            goto errstate;
    };
    x->x_nyq = sys_getsr()/2;
    update_coeffs(x, (double)freq, (double)reson);
    x->x_inlet_freq = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_freq, freq);
    x->x_inlet_q = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_q, reson);
    x->x_out = outlet_new((t_object *)x, &s_signal);
    return(x);
errstate:
    pd_error(x, "[resonator~]: improper args");
    return(NULL);
}

void resonator_tilde_setup(void){
    resonator_class = class_new(gensym("resonator~"), (t_newmethod)resonator_new, 0,
        sizeof(t_resonator), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(resonator_class, (t_method)resonator_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(resonator_class, nullfn, gensym("signal"), 0);
    class_addmethod(resonator_class, (t_method)resonator_clear, gensym("clear"), 0);
}
