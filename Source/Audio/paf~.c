// based on Miller's paf, heavily modified, simplified and with features added.

#include "m_pd.h"
#include "buffer.h"

typedef struct _paf{
    t_object    x_obj;
    t_float     x_f;
    t_inlet    *x_inlet_cf;
    t_inlet    *x_inlet_bw;
    t_int       x_n;
    t_int       x_sr;
    float       x_isr;
    double      x_held_cf_ratio;
    double      x_phase;
    double      x_cf_phase_shift;
    int         x_trigger;
    t_symbol   *x_ignore;
    //    int         x_cauchy;
}t_paf;

static t_class *paf_class;

static void paf_phase(t_paf *x, t_floatarg phase, t_floatarg cf_phase_shift){
    x->x_phase = phase, x->x_cf_phase_shift = cf_phase_shift;
    x->x_trigger = 1;
}

/*static void paf_cauchy(t_paf *x, t_floatarg f){
    x->x_cauchy = (f != 0);
}*/

float paf_get_ratio(float f0, float freq){
    float ratio = f0 <= 0 ? 1 : fabsf(freq / f0);
    if(ratio < 1)
        ratio = 1;
    return(ratio);
}

static t_int *paf_perform(t_int *w){
    int n = (int)(w[1]);
    t_paf *x = (t_paf *)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *in3 = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);
    double phase = x->x_phase, held_cf_ratio = x->x_held_cf_ratio;
    for(int i = 0; i < n; i++){
        float freqval = in1[i];
        float cfval = in2[i];
        float bwval = in3[i];
        if(x->x_trigger || phase >= 1.0 || phase < 0.0){ // update
            if(phase >= 1.0)
                phase -= 1.0;
            else if(phase < 0.0)
                phase += 1.0;
            held_cf_ratio = paf_get_ratio(freqval, cfval);
            x->x_trigger = 0;
        }
// carrier oscillator
        double held_int_cf = (int)held_cf_ratio;
        double held_frac_cf = held_cf_ratio - held_int_cf;
        double carphase1 = phase * held_int_cf + x->x_cf_phase_shift; // harmonic
        carphase1 -= floor(carphase1); // wrap
        double carphase2 = carphase1 + phase; // next harmonic
        carphase2 -= floor(carphase2); // wrap
        float sine1 = read_sintab((double)carphase1);
        float sine2 = read_sintab((double)carphase2);
        float carrier = sine1 + held_frac_cf * (sine2 - sine1);
// Bandwidth AM shaping
        float bw_ratio = paf_get_ratio(freqval, bwval);
        float fphase = 2.0f * (float)phase - 1.0f;
        float index = bw_ratio * (1.0f - fphase * fphase);
        if(index >= (float)(0.997 * GAUSS_TABRANGE))
            index = (float)(0.997 * GAUSS_TABRANGE);
        float AM = read_gausstab(index); // or cauchy
        out[i] = carrier * AM;
        phase += (freqval * x->x_isr); // f0 phase
    }
    x->x_phase = phase;
    x->x_held_cf_ratio = held_cf_ratio;
    return(w+7);
}

static void paf_dsp(t_paf *x, t_signal **sp){
    int n = sp[0]->s_n, sr = sp[0]->s_sr;
    if(sr != x->x_sr){
        x->x_isr = (float)(1./sr);
        sr = x->x_sr;
    }
    dsp_add(paf_perform, 6, n, x, sp[0]->s_vec,
        sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *paf_free(t_paf *x){
    inlet_free(x->x_inlet_cf);
    inlet_free(x->x_inlet_bw);
    return(void *)x;
}

static void *paf_new(t_symbol *s, int ac, t_atom *av){
    t_paf *x = (t_paf *)pd_new(paf_class);
    x->x_ignore = s;
    float cf = 0, bw = 0, phase = 0, shift = 0;
    if(ac){
        x->x_f = atom_getfloat(av);
        ac--, av++;
        if(ac){
            cf = atom_getfloat(av);
            ac--, av++;
            if(ac){
                bw = atom_getfloat(av);
                ac--, av++;
                if(ac){
                    phase = atom_getfloat(av);
                    ac--, av++;
                    if(ac){
                        shift = atom_getfloat(av);
                        ac--, av++;
                    }
                }
            }
        }
    }
    x->x_trigger = 1;
    x->x_held_cf_ratio = 1.f;
    x->x_phase = phase;
    x->x_cf_phase_shift = shift;
    x->x_inlet_cf = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_cf, cf);
    x->x_inlet_bw = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_bw, bw);
    outlet_new(&x->x_obj, gensym("signal"));
    init_sine_table();
    init_gauss_table();
//    x->x_cauchy = 0;
//    init_cauchy_table();
    return(x);
}

void paf_tilde_setup(void){
    paf_class = class_new(gensym("paf~"), (t_newmethod)(void*)paf_new,
        (t_method)paf_free, sizeof(t_paf), 0, A_GIMME, 0);
    CLASS_MAINSIGNALIN(paf_class, t_paf, x_f);
    class_addmethod(paf_class, (t_method)paf_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(paf_class, (t_method)paf_phase, gensym("phase"), A_FLOAT, A_DEFFLOAT, 0);
//    class_addmethod(paf_class, (t_method)paf_cauchy, gensym("cauchy"), A_FLOAT, 0);
}
