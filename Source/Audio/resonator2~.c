// Porres 2024-2026

#define LOG001 log(0.001)
#define TWO_PI (2 * 3.14159265358979323846)

#include <m_pd.h>
#include <math.h>

static t_class *resonator2_class;

typedef struct _resonator2{
    t_object    x_obj;
    t_inlet    *x_inlet_excitation;
    t_inlet    *x_inlet_t60;
    t_float     x_freq;
    int         x_n;
    int         x_nchans;
    int         x_ch2;
    int         x_ch3;
    double      x_convert;
    double      x_srkhz;
    double     *x_y1;
    double     *x_y2;
}t_resonator2;

static t_int *resonator2_perform(t_int *w){
    t_resonator2 *x = (t_resonator2 *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);       // hz
    t_float *in2 = (t_float *)(w[3]);       // excitation
    t_float *in3 = (t_float *)(w[4]);       // t60
    t_float *out1 = (t_float *)(w[5]);
    t_float *out2 = (t_float *)(w[6]);
    double *y1 = x->x_y1;
    double *y2 = x->x_y2;
    int n = x->x_n, ch2 = x->x_ch2, ch3 = x->x_ch3;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            double hz = (double)in1[j*n + i];
            double in = ch2 == 1 ? (double)in2[i] : (double)in2[j*n + i];
            double t60 = ch3 == 1 ? (double)in3[i] : (double)in3[j*n + i];
            double rad = hz * x->x_convert;
            double c = exp(LOG001 / (t60 * x->x_srkhz));
            double ar = cos(rad) * c; // do table read instead
            double ai = sin(rad) * c;
            double re = in + ar * y1[j] - ai * y2[j];
            double im = ai * y1[j] + ar * y2[j];
            out1[j*n + i] = y1[j] = re;
            out2[j*n + i] = y2[j] = im;
        }
    }
    x->x_y1 = y1;
    x->x_y2 = y2;
    return(w+7);
}

static void resonator2_dsp(t_resonator2 *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    double sr = (double)sp[0]->s_sr;
    x->x_srkhz = sr * 0.001;
    x->x_convert = TWO_PI / sr;
    int chs = sp[0]->s_nchans;
    int ch2 = sp[1]->s_nchans, ch3 = sp[2]->s_nchans;
    if(x->x_nchans != chs){
        x->x_y1 = (double *)resizebytes(x->x_y1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_y2 = (double *)resizebytes(x->x_y2,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[3], chs);
    signal_setmultiout(&sp[4], chs);
    if((ch2 > 1 && ch2 != x->x_nchans) || (ch3 > 1 && ch3 != x->x_nchans)){
        dsp_add_zero(sp[3]->s_vec, chs*x->x_n);
        dsp_add_zero(sp[4]->s_vec, chs*x->x_n);
        pd_error(x, "[resonator2~]: channel sizes mismatch");
        return;
    }
    dsp_add(resonator2_perform, 6, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
}

void resonator2_clear(t_resonator2 *x){
    x->x_y1 = x->x_y2 = 0;
}

static void *resonator2_free(t_resonator2 *x){
    freebytes(x->x_y1, x->x_nchans * sizeof(*x->x_y1));
    freebytes(x->x_y2, x->x_nchans * sizeof(*x->x_y2));
    inlet_free(x->x_inlet_excitation);
    inlet_free(x->x_inlet_t60);
    return(void *)x;
}

static void *resonator2_new(t_symbol *s, int ac, t_atom *av){
    (void)s;
    t_resonator2 *x = (t_resonator2 *)pd_new(resonator2_class);
    x->x_freq = 1;
    float t60 = 0;
    x->x_y1 = (double *)getbytes(sizeof(*x->x_y1));
    x->x_y2 = (double *)getbytes(sizeof(*x->x_y2));
    x->x_y1[0] = x->x_y2[0] = 0.0;
    if(ac){
        x->x_freq = atom_getfloat(av);
        ac--, av++;
        if(ac){
            t60 = atom_getfloat(av);
            ac--, av++;
        }
    }
    x->x_inlet_excitation = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_inlet_t60 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_t60, t60);
    outlet_new((t_object *)x, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    return(x);
}

void resonator2_tilde_setup(void){
    resonator2_class = class_new(gensym("resonator2~"), (t_newmethod)resonator2_new,
        (t_method)resonator2_free, sizeof(t_resonator2), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(resonator2_class, t_resonator2, x_freq);
    class_addmethod(resonator2_class, (t_method)resonator2_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(resonator2_class, (t_method)resonator2_clear, gensym("clear"), A_NULL);
}
