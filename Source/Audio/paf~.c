// Porres 2026

#include "m_pd.h"
#include "buffer.h"
#include "magic.h"
#include <stdlib.h>

#define MAXLEN 1024

typedef struct _paf{
    t_object    x_obj;
    t_inlet    *x_inlet_cf;
    t_inlet    *x_inlet_bw;
    t_int       x_n;
    float      *x_f0_list;
    float      *x_cf_list;
    float      *x_bw_list;
    t_int       x_f0_list_size;
    t_int       x_cf_list_size;
    t_int       x_bw_list_size;
    int         x_nchs;
    int         x_ch1;
    int         x_ch2;
    int         x_ch3;
    t_int       x_sig1;
    t_int       x_sig2;
    t_int       x_sig3;
    t_int       x_sr;
    float       x_isr;
    double     *x_held_cf_ratio;
    double     *x_held_bw_ratio;
    double     *x_phase;
    double      x_cf_phase_shift;
    t_glist    *x_glist;
    t_float    *x_sigscalar1;
    t_float    *x_sigscalar2;
    t_float    *x_sigscalar3;
    t_symbol   *x_ignore;
}t_paf;

static t_class *paf_class;

static void paf_f0(t_paf *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    if(ac > MAXLEN)
        ac = MAXLEN;
    for(int i = 0; i < ac; i++)
        x->x_f0_list[i] = atom_getfloat(av+i);
    if(x->x_f0_list_size != ac){
        x->x_f0_list_size = ac;
        canvas_update_dsp();
    }
    else_magic_setnan(x->x_sigscalar1);
}

static void paf_cf(t_paf *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    if(ac > MAXLEN)
        ac = MAXLEN;
    for(int i = 0; i < ac; i++)
        x->x_cf_list[i] = atom_getfloat(av+i);
    if(x->x_cf_list_size != ac){
        x->x_cf_list_size = ac;
        canvas_update_dsp();
    }
    else_magic_setnan(x->x_sigscalar2);
}

static void paf_bw(t_paf *x, t_symbol *s, int ac, t_atom *av){
    x->x_ignore = s;
    if(ac == 0)
        return;
    if(ac > MAXLEN)
        ac = MAXLEN;
    for(int i = 0; i < ac; i++)
        x->x_bw_list[i] = atom_getfloat(av+i);
    if(x->x_bw_list_size != ac){
        x->x_bw_list_size = ac;
        canvas_update_dsp();
    }
    else_magic_setnan(x->x_sigscalar3);
}

static void paf_phase(t_paf *x, t_floatarg f1, t_floatarg f2){
    double phase = f1 < 0 ? 0 : f1 >= 1 ? 0 : f1;
    double phase_shift = f2 < 0 ? 0 : f2 >= 1 ? 0 : f2;
    x->x_cf_phase_shift = phase_shift;
    for(int i = 0; i < x->x_nchs; i++)
        x->x_phase[i] = phase + 1.0;
}

double paf_get_ratio(float f0, float hz){
    return(f0 == 0 ? 0.0f : fabs((double)hz / (double)f0));
}

static t_int *paf_perform(t_int *w){
    t_paf *x = (t_paf *)(w[1]);
    t_float *in1 = (t_float *)(w[2]);
    t_float *in2 = (t_float *)(w[3]);
    t_float *in3 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    int n = x->x_n;
    double *phase = x->x_phase;
    double *held_cf_ratio = x->x_held_cf_ratio;
    double *held_bw_ratio = x->x_held_bw_ratio;
    if(!x->x_sig1){
        t_float *scalar = x->x_sigscalar1;
        if(!else_magic_isnan(*x->x_sigscalar1)){
            t_float f0 = *scalar;
            for(int j = 0; j < x->x_nchs; j++)
                x->x_f0_list[j] = f0;
            else_magic_setnan(x->x_sigscalar1);
        }
    }
    if(!x->x_sig2){
        t_float *scalar = x->x_sigscalar2;
        if(!else_magic_isnan(*x->x_sigscalar2)){
            t_float cf = *scalar;
            for(int j = 0; j < x->x_nchs; j++)
                x->x_cf_list[j] = cf;
            else_magic_setnan(x->x_sigscalar2);
        }
    }
    if(!x->x_sig3){
        t_float *scalar = x->x_sigscalar3;
        if(!else_magic_isnan(*x->x_sigscalar3)){
            t_float bw = *scalar;
            for(int j = 0; j < x->x_nchs; j++)
                x->x_bw_list[j] = bw;
            else_magic_setnan(x->x_sigscalar3);
        }
    }
    for(int i = 0; i < n; i++){
        for(int j = 0; j < x->x_nchs; j++){
            float f0;
            if(x->x_ch1 == 1)
                f0 = x->x_sig1 ? in1[i] : x->x_f0_list[0];
            else
                f0 = x->x_sig1 ? in1[j*n + i] : x->x_f0_list[j];
            double phase_step = ((double)f0 * x->x_isr);
            if(phase_step > 1)
                phase_step = 1;
            if(phase_step < -1)
                phase_step = -1;
            float cf;
            if(x->x_ch2 == 1)
                cf = x->x_sig2 ? in2[i] : x->x_cf_list[0];
            else
                cf = x->x_sig2 ? in2[j*n + i] : x->x_cf_list[j];
            float bw;
            if(x->x_ch3 == 1)
                bw = x->x_sig3 ? in3[i] : x->x_bw_list[0];
            else
                bw = x->x_sig3 ? in3[j*n + i] : x->x_bw_list[j];
            if(bw < 0)
                bw = 0;
            if(phase[j] >= 1.0 || phase[j] < 0.0){ // update
                phase[j] -= floor(phase[j]);
                held_cf_ratio[j] = paf_get_ratio(f0, cf);
                held_bw_ratio[j] = paf_get_ratio(f0, bw);
            }
    // Carrier Signal
            float saw = (2.0f * (float)phase[j]) - 1.0f; // rescale to -1 to 1
            float parabolic = 1.0f - (saw * saw);        // parabolic signal
            float idx = parabolic * held_bw_ratio[j];    // index for waveshaping
            float carrier = read_gausstab(idx);          // waveshaping
    // Ring Modulator Signal
            double held_int_cf = (int)held_cf_ratio[j];
            double held_frac_cf = held_cf_ratio[j] - held_int_cf; // wrap
            double modphase = phase[j] + x->x_cf_phase_shift;
            double phase1 = modphase * held_int_cf; // harmonic
            phase1 -= floor(phase1); // wrap
            double phase2 = modphase * (held_int_cf + 1); // next harmonic
            phase2 -= floor(phase2); // wrap
            float cos1 = read_costab((double)phase1);
            float cos2 = read_costab((double)phase2);
            float RM = cos1 + held_frac_cf * (cos2 - cos1);
    // Voilà!!!
            out[j*n + i] = carrier * RM;  // PAF
    // update f0 phase for next round
            phase[j] += phase_step;
        }
    }
    x->x_phase = phase;
    x->x_held_cf_ratio = held_cf_ratio;
    x->x_held_bw_ratio = held_bw_ratio;
    return(w+6);
}

static void paf_dsp(t_paf *x, t_signal **sp){
    int sr = sp[0]->s_sr;
    if(sr != x->x_sr){
        x->x_sr = sr;
        x->x_isr = (float)(1./sr);
    }
    x->x_n = sp[0]->s_n;
    x->x_sig1 = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    x->x_sig2 = else_magic_inlet_connection((t_object *)x, x->x_glist, 1, &s_signal);
    x->x_sig3 = else_magic_inlet_connection((t_object *)x, x->x_glist, 2, &s_signal);
    int chs = x->x_ch1 = x->x_sig1 ? sp[0]->s_nchans : x->x_f0_list_size;
    x->x_ch2 = x->x_sig2 ? sp[1]->s_nchans : x->x_cf_list_size;
    if(x->x_ch2 > chs)
        chs = x->x_ch2;
    x->x_ch3 = x->x_sig3 ? sp[2]->s_nchans : x->x_bw_list_size;
    if(x->x_ch3 > chs)
        chs = x->x_ch3;
    if(x->x_nchs != chs){
        x->x_phase = (double *)resizebytes(x->x_phase,
            x->x_nchs * sizeof(double), chs * sizeof(double));
        x->x_held_cf_ratio = (double *)resizebytes(x->x_held_cf_ratio,
            x->x_nchs * sizeof(double), chs * sizeof(double));
        x->x_held_bw_ratio = (double *)resizebytes(x->x_held_bw_ratio,
            x->x_nchs * sizeof(double), chs * sizeof(double));
        x->x_nchs = chs;
        for(int i = 0; i < x->x_nchs; i++)
            x->x_phase[i] = 1.0;
    }
    signal_setmultiout(&sp[3], x->x_nchs);
    if((x->x_ch1 > 1 && x->x_ch1 != x->x_nchs)
    || (x->x_ch2 > 1 && x->x_ch2 != x->x_nchs)
    || (x->x_ch3 > 1 && x->x_ch3 != x->x_nchs)){
        dsp_add_zero(sp[3]->s_vec, x->x_nchs*x->x_n);
        pd_error(x, "[paf~]: channel sizes mismatch");
        return;
    }
    dsp_add(paf_perform, 5, x, sp[0]->s_vec,
        sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *paf_free(t_paf *x){
    inlet_free(x->x_inlet_cf);
    inlet_free(x->x_inlet_bw);
    freebytes(x->x_phase, x->x_nchs * sizeof(*x->x_phase));
    freebytes(x->x_held_cf_ratio, x->x_nchs * sizeof(*x->x_held_cf_ratio));
    freebytes(x->x_held_bw_ratio, x->x_nchs * sizeof(*x->x_held_bw_ratio));
    free(x->x_f0_list);
    free(x->x_cf_list);
    free(x->x_bw_list);
    return(void *)x;
}

static void *paf_new(t_symbol *s, int ac, t_atom *av){
    t_paf *x = (t_paf *)pd_new(paf_class);
    x->x_ignore = s;
    x->x_f0_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_cf_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_bw_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_f0_list_size = x->x_cf_list_size = x->x_bw_list_size = x->x_nchs = 1;
    x->x_f0_list[0] = x->x_cf_list[0] = x->x_bw_list[0] = 0;
    double phase = 0;
    double shift = 0;
    if(ac){
        x->x_f0_list[0] = atom_getfloat(av);
        ac--, av++;
        if(ac){
            x->x_cf_list[0] = atom_getfloat(av);
            ac--, av++;
            if(ac){
                x->x_bw_list[0] = atom_getfloat(av);
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
    x->x_phase = (double *)getbytes(sizeof(*x->x_phase));
    x->x_held_cf_ratio = (double *)getbytes(sizeof(*x->x_held_cf_ratio));
    x->x_held_bw_ratio = (double *)getbytes(sizeof(*x->x_held_bw_ratio));
    x->x_phase[0] = phase;
    x->x_cf_phase_shift = shift;
    x->x_held_cf_ratio[0] = 0.f;
    x->x_held_bw_ratio[0] = 0.f;
    x->x_inlet_cf = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
//        pd_float((t_pd *)x->x_inlet_cf, x->x_cf_list[0]);
    x->x_inlet_bw = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
//        pd_float((t_pd *)x->x_inlet_bw, x->x_bw_list[0]);
    outlet_new(&x->x_obj, gensym("signal"));
    x->x_glist = canvas_getcurrent();
    x->x_sigscalar1 = obj_findsignalscalar((t_object *)x, 0);
    else_magic_setnan(x->x_sigscalar1);
    x->x_sigscalar2 = obj_findsignalscalar((t_object *)x, 1);
    else_magic_setnan(x->x_sigscalar2);
    x->x_sigscalar3 = obj_findsignalscalar((t_object *)x, 2);
    else_magic_setnan(x->x_sigscalar3);
    return(x);
}

void paf_tilde_setup(void){
    paf_class = class_new(gensym("paf~"), (t_newmethod)(void*)paf_new,
        (t_method)paf_free, sizeof(t_paf), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(paf_class, nullfn, gensym("signal"), 0);
    class_addmethod(paf_class, (t_method)paf_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(paf_class, paf_f0);
    class_addmethod(paf_class, (t_method)paf_phase, gensym("phase"), A_FLOAT, A_DEFFLOAT, 0);
    class_addmethod(paf_class, (t_method)paf_cf, gensym("cf"), A_GIMME, 0);
    class_addmethod(paf_class, (t_method)paf_bw, gensym("bw"), A_GIMME, 0);
    init_cosine_table();
    init_gauss_table();
}
