// Porres 2024

#define LOG001 log(0.001)
#define TWO_PI (2 * 3.14159265358979323846)

#include <m_pd.h>
#include <math.h>

static t_class *resonator2_class;

typedef struct _resonator2{
    t_object    x_obj;
    t_int       x_n;
    t_inlet    *x_inlet_hz;
    t_inlet    *x_inlet_t60;
    double      x_convert;
    double      x_srkhz;
    double      x_y1;
    double      x_y2;
}t_resonator2;

static t_int *resonator2_perform(t_int *w){
    t_resonator2 *x = (t_resonator2 *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);       // in
    t_float *in2 = (t_float *)(w[3]);       // hz
    t_float *in3 = (t_float *)(w[4]);       // t60
    t_float *out1 = (t_float *)(w[5]);
    t_float *out2 = (t_float *)(w[6]);
    int n = x->x_n;
    double y1 = x->x_y1;
    double y2 = x->x_y2;
    while(n--){
        double in = (double)*in1++;
        double hz = (double)*in2++;
        double t60 = (double)*in3++;
        double rad = hz * x->x_convert;
        double c = exp(LOG001 / (t60 * x->x_srkhz));
        double ar = cos(rad) * c; // read table
        double ai = sin(rad) * c;
        double re = in + ar * y1 - ai * y2;
        double im = ai * y1 + ar * y2;
        *out1++ = y1 = re;
        *out2++ = y2 = im;
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
    dsp_add(resonator2_perform, 6, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
}

void resonator2_clear(t_resonator2 *x){
    x->x_y1 = x->x_y2 = 0;
}

static void *resonator2_free(t_resonator2 *x){
    inlet_free(x->x_inlet_hz);
    inlet_free(x->x_inlet_t60);
    return(void *)x;
}

static void *resonator2_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_resonator2 *x = (t_resonator2 *)pd_new(resonator2_class);
    float hz = 1;
    float t60 = 0;
    if(ac){
        hz = atom_getfloat(av);
        ac--, av++;
        if(ac){
            t60 = atom_getfloat(av);
            ac--, av++;
        }
    }
    x->x_inlet_hz = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_hz, hz);
    x->x_inlet_t60 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_t60, t60);
    outlet_new((t_object *)x, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    return(x);
}

void resonator2_tilde_setup(void){
    resonator2_class = class_new(gensym("resonator2~"), (t_newmethod)resonator2_new,
        (t_method)resonator2_free, sizeof(t_resonator2), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(resonator2_class, nullfn, gensym("signal"), 0);
    class_addmethod(resonator2_class, (t_method)resonator2_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(resonator2_class, (t_method)resonator2_clear, gensym("clear"), A_NULL);
}
