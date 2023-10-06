// Porres 2017-2023

#include "m_pd.h"
#include "buffer.h"

typedef struct _pm{
    t_object    x_obj;
    double     *x_phase1;
    double     *x_phase2;
    int         x_nchans;
    int         x_ratio;
    t_float     x_freq;
    t_inlet    *x_inlet_mod;
    t_inlet    *x_inlet_idx;
    t_inlet    *x_inlet_phase;
    t_outlet   *x_outlet;
    int         x_n;
    int         x_ch2;
    int         x_ch3;
    int         x_ch4;
    double      x_sr_rec;
}t_pm;

static t_class *pm_class;

double pm_wrap_phase(double phase){
    while(phase >= 1)
        phase -= 1.;
    while(phase < 0)
        phase += 1.;
    return(phase);
}

static t_int *pm_perform(t_int *w){
    t_pm *x = (t_pm *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *in3 = (t_float *)(w[4]);
    t_float *in4 = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);
    int n = x->x_n, ch2 = x->x_ch2, ch3 = x->x_ch3, ch4 = x->x_ch4;
    double *phase1 = x->x_phase1;
    double *phase2 = x->x_phase2;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            double carrier = in1[j*n + i];
            double mod = ch2 == 1 ? in2[i] : in2[j*n + i];
            if(x->x_ratio)
                mod *= carrier;
            double index = ch3 == 1 ? in3[i] : in3[j*n + i];
            double phase_offset = ch4 == 1 ? in4[i] : in4[j*n + i];
            
            double modulator = read_sintab(pm_wrap_phase(phase2[j] + phase_offset)) * index;
            out[j*n + i] = read_sintab(pm_wrap_phase(phase1[j] + modulator));
            
            phase1[j] = pm_wrap_phase(phase1[j] + carrier * x->x_sr_rec);
            phase2[j] = pm_wrap_phase(phase2[j] + mod * x->x_sr_rec);
        }
    }
    x->x_phase1 = phase1;
    x->x_phase2 = phase2;
    return(w+7);
}

static void pm_ratio(t_pm *x, t_floatarg f){
    x->x_ratio = f != 0;
}

static void pm_dsp(t_pm *x, t_signal **sp){
    x->x_sr_rec = 1.0 / (double)sp[0]->s_sr;
    int chs = sp[0]->s_nchans;
    int ch2 = sp[1]->s_nchans, ch3 = sp[2]->s_nchans, ch4 = sp[3]->s_nchans;
    x->x_n = sp[0]->s_n;
    signal_setmultiout(&sp[4], chs);
    if(x->x_nchans != chs){
        x->x_phase1 = (double *)resizebytes(x->x_phase1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_phase2 = (double *)resizebytes(x->x_phase2,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_nchans = chs;
    }
    if((ch2 > 1 && ch2 != x->x_nchans) || (ch3 > 1 && ch3 != x->x_nchans)){
        dsp_add_zero(sp[4]->s_vec, chs*x->x_n);
        pd_error(x, "[pm~]: channel sizes mismatch");
        return;
    }
    x->x_ch2 = ch2;
    x->x_ch3 = ch3;
    x->x_ch4 = ch4;
    dsp_add(pm_perform, 6, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec,
        sp[3]->s_vec, sp[4]->s_vec);
}

static void *pm_free(t_pm *x){
    inlet_free(x->x_inlet_mod);
    inlet_free(x->x_inlet_idx);
    inlet_free(x->x_inlet_phase);
    outlet_free(x->x_outlet);
    freebytes(x->x_phase1, x->x_nchans * sizeof(*x->x_phase1));
    freebytes(x->x_phase2, x->x_nchans * sizeof(*x->x_phase2));
    return(void *)x;
}

static void *pm_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_pm *x = (t_pm *)pd_new(pm_class);
    t_float f1 = 0, f2 = 0, f3 = 0, f4 = 0;
    x->x_ratio = 0;
    x->x_phase1 = (double *)getbytes(sizeof(*x->x_phase1));
    x->x_phase2 = (double *)getbytes(sizeof(*x->x_phase2));
    if(ac && av->a_type == A_SYMBOL){
        if(atom_getsymbol(av) == gensym("-ratio"))
            x->x_ratio = 1;
        ac--, av++;
    }
    if(ac && av->a_type == A_FLOAT){
        f1 = av->a_w.w_float;
        ac--, av++;
        if(ac && av->a_type == A_FLOAT){
            f2 = av->a_w.w_float;
            ac--, av++;
            if(ac && av->a_type == A_FLOAT){
                f3 = av->a_w.w_float;
                ac--, av++;
                if(ac && av->a_type == A_FLOAT){
                    f4 = av->a_w.w_float;
                    ac--, av++;
                }
            }
        }
    }
    t_float init_freq = f1;
    t_float init_mod = f2;
    t_float init_idx = f3;
    t_float init_phase = f4;
    init_phase = init_phase  < 0 ? 0 : init_phase >= 1 ? 0 : init_phase; // clipping phase input
    if(init_phase == 0 && init_freq > 0)
        x->x_phase1[0] = 1.;
    else
        x->x_phase1[0] = init_phase;
    init_sine_table();
    x->x_freq = init_freq;
    x->x_inlet_mod = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_mod, init_mod);
    x->x_inlet_idx = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_idx, init_idx);
    x->x_inlet_phase = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_phase, init_phase);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void pm_tilde_setup(void){
    pm_class = class_new(gensym("pm~"), (t_newmethod)pm_new, (t_method)pm_free,
        sizeof(t_pm), CLASS_MULTICHANNEL, A_GIMME, 0);
    CLASS_MAINSIGNALIN(pm_class, t_pm, x_freq);
    class_addmethod(pm_class, (t_method)pm_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(pm_class, (t_method)pm_ratio, gensym("ratio"), A_FLOAT, 0);
}
