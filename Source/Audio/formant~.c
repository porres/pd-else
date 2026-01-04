// based on SC's Formant

#include "m_pd.h"
#include "buffer.h"
#include "magic.h"
#include <math.h>
#include <stdlib.h>

#define MAXLEN 1024

static t_class *formant_class;

typedef struct _formant {
    t_object    x_obj;
    float      *x_f0_list;
    float      *x_cf_list;
    float      *x_bw_list;
    t_int       x_f0_list_size;
    t_int       x_cf_list_size;
    t_int       x_bw_list_size;
    int         x_n;
    int         x_nchs;
    int         x_ch1;
    int         x_ch2;
    int         x_ch3;
    t_int       x_sig1;
    t_int       x_sig2;
    t_int       x_sig3;
    double     *x_fund_phase;
    double     *x_cf_phase;
    double     *x_bw_phase;
    float       x_sr_inv;
    t_glist    *x_glist;
    t_float    *x_sigscalar1;
    t_float    *x_sigscalar2;
    t_float    *x_sigscalar3;
    t_symbol   *x_ignore;
}t_formant;

static void formant_f0(t_formant *x, t_symbol *s, int ac, t_atom *av){
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

static void formant_cf(t_formant *x, t_symbol *s, int ac, t_atom *av){
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

static void formant_bw(t_formant *x, t_symbol *s, int ac, t_atom *av){
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

double wrap_phase(double phase){
    return(phase -= floor(phase));
}

static t_int *formant_perform(t_int *w){
	t_formant *x = (t_formant *)(w[1]);
	t_sample *in1 = (t_sample *)(w[2]);
	t_sample *in2 = (t_sample *)(w[3]);
    t_sample *in3 = (t_sample *)(w[4]);
	t_sample *out = (t_sample *)(w[5]);
    int n = x->x_n;
	float sr_inv = x->x_sr_inv;
    double *cf_phase = x->x_cf_phase;
    double *fund_phase = x->x_fund_phase;
    double *bw_phase = x->x_bw_phase;
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
            float fund;
            if(x->x_ch1 == 1)
                fund = x->x_sig1 ? in1[i] : x->x_f0_list[0];
            else
                fund = x->x_sig1 ? in1[j*n + i] : x->x_f0_list[j];
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
            float output = 0.0f;
            if(fund < 0)
                fund = 0;
            if(bw < fund)
                bw = fund;
            double fund_inc = fund*sr_inv;
            double cf_inc = cf*sr_inv;
            double bw_inc = bw*sr_inv;
            if(fund_inc > 1)
                fund_inc = 1;
            if(cf_inc > 1)
                cf_inc = 1;
            if(bw_inc > 1)
                bw_inc = 1;
            if(cf_phase[j] > 1)
                cf_phase[j] -= floor(cf_phase[j]);
            if(fund_phase[j] > 1){
                fund_phase[j] -= floor(fund_phase[j]);
                cf_phase[j] = fund_phase[j] * cf_inc / fund_inc;
                bw_phase[j] = fund_phase[j] * bw_inc / fund_inc;
            }
            if(bw_phase[j] < 1){
                float hann = read_sintab(wrap_phase(bw_phase[j] + 0.75)) + 1 * 0.5;
                output = read_sintab(cf_phase[j]) * hann;
                bw_phase[j] += bw_inc;
            }
            else
                output = 0.0f;
            out[j*n + i] = output;
            fund_phase[j] += fund_inc;
            cf_phase[j] += cf_inc;
        }
    }
	x->x_cf_phase = cf_phase;
	x->x_fund_phase = fund_phase;
    x->x_bw_phase = bw_phase;
	return(w+6);
}

static void formant_dsp(t_formant *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    x->x_sr_inv = 1./sp[0]->s_sr;
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
        x->x_fund_phase = (double *)resizebytes(x->x_fund_phase,
            x->x_nchs * sizeof(double), chs * sizeof(double));
        x->x_cf_phase = (double *)resizebytes(x->x_cf_phase,
            x->x_nchs * sizeof(double), chs * sizeof(double));
        x->x_bw_phase = (double *)resizebytes(x->x_bw_phase,
            x->x_nchs * sizeof(double), chs * sizeof(double));
        x->x_nchs = chs;
        for(int i = 0; i < x->x_nchs; i++)
            x->x_fund_phase[i] = 1.0;
    }
    signal_setmultiout(&sp[3], x->x_nchs);
    if((x->x_ch1 > 1 && x->x_ch1 != x->x_nchs)
    || (x->x_ch2 > 1 && x->x_ch2 != x->x_nchs)
    || (x->x_ch3 > 1 && x->x_ch3 != x->x_nchs)){
        dsp_add_zero(sp[3]->s_vec, x->x_nchs*x->x_n);
        pd_error(x, "[formant~]: channel sizes mismatch");
        return;
    }
	dsp_add(formant_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *formant_free(t_formant *x){
    freebytes(x->x_fund_phase, x->x_nchs * sizeof(*x->x_fund_phase));
    freebytes(x->x_cf_phase, x->x_nchs * sizeof(*x->x_cf_phase));
    freebytes(x->x_bw_phase, x->x_nchs * sizeof(*x->x_bw_phase));
    free(x->x_f0_list);
    free(x->x_cf_list);
    free(x->x_bw_list);
    return(void *)x;
}

static void *formant_new(t_symbol *s, int ac, t_atom *av){
	t_formant *x = (t_formant *)pd_new(formant_class);
    x->x_ignore = s;
    x->x_sr_inv = 0.0;
    x->x_f0_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_cf_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_bw_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_f0_list_size = x->x_cf_list_size = x->x_bw_list_size = x->x_nchs = 1;
    x->x_f0_list[0] = x->x_cf_list[0] = x->x_bw_list[0] = 0;
    if(ac){
        x->x_f0_list[0] = atom_getfloat(av);
        ac--, av++;
        if(ac){
            x->x_cf_list[0] = atom_getfloat(av);
            ac--, av++;
            if(ac){
                x->x_bw_list[0] = atom_getfloat(av);
                ac--, av++;
            }
        }
    }
    x->x_fund_phase = (double *)getbytes(sizeof(*x->x_fund_phase));
    x->x_cf_phase = (double *)getbytes(sizeof(*x->x_cf_phase));
    x->x_bw_phase = (double *)getbytes(sizeof(*x->x_bw_phase));
    inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
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

void formant_tilde_setup(void){
    formant_class = class_new(gensym("formant~"), (t_newmethod)(void*)formant_new,
        (t_method)formant_free, sizeof(t_formant), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(formant_class, nullfn, gensym("signal"), 0);
    class_addmethod(formant_class, (t_method)formant_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(formant_class, formant_f0);
    class_addmethod(formant_class, (t_method)formant_cf, gensym("cf"), A_GIMME, 0);
    class_addmethod(formant_class, (t_method)formant_bw, gensym("bw"), A_GIMME, 0);
    init_sine_table();
}
