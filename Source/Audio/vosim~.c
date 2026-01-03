// porres basically stole this from shadylib
// this is still experimental, I changed a lot of things, added some others, and will still check out other algorithms and the theory

#include "m_pd.h"
#include "magic.h"
#include <math.h>
#include <stdlib.h>

#define MAXLEN 1024

#define SINSQRSIZE 512

static t_sample *sinsqr_tbl;
static t_class *vosim_class;

typedef struct _vosim{
    t_object    x_obj;
    t_float     x_f;
    int         x_n;
    int         x_nchs;
    int         x_ch1;
    int         x_ch2;
    t_int       x_sig1;
    t_int       x_sig2;
    float      *x_f0_list;
    float      *x_cf_list;
    t_int       x_f0_list_size;
    t_int       x_cf_list_size;
    double     *x_fund_phase;
    double     *x_cf_phase;
    double     *x_res;
    double     *x_curdec;        // current decay coefficient
    float       x_duty;          // duty cycle
    float       x_decay;         // decay coefficient
    float       x_sr_inv;
    t_symbol   *x_ignore;
    t_glist    *x_glist;
    t_float    *x_sigscalar1;
    t_float    *x_sigscalar2;
}t_vosim;

static void vosim_maketab(void){
	t_sample incr = M_PI/(SINSQRSIZE - 1);
	sinsqr_tbl = getbytes((SINSQRSIZE) * sizeof(t_sample));
	for(int i = 0; i < SINSQRSIZE; i++)
		sinsqr_tbl[i] = pow(sin(incr*i), 2);
}

static t_sample vosim_readtab(t_sample idx){ // read with linear interpolation
	int index = (int)idx;
    int next  = index + 1;
    if(next >= SINSQRSIZE)
        next = 0;
	t_sample frac = idx - index;
    t_sample v1 = sinsqr_tbl[index], v2 = sinsqr_tbl[next];
	return(v1 + frac*(v2-v1));
}

static void vosim_f0(t_vosim *x, t_symbol *s, int ac, t_atom *av){
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

static void vosim_cf(t_vosim *x, t_symbol *s, int ac, t_atom *av){
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

static void vosim_reset(t_vosim *x){
    for(int i = 0; i < x->x_nchs; i++){
        x->x_fund_phase[i] = x->x_cf_phase[i] = x->x_res[i] = 0.0f;
        x->x_curdec[i] = 1.0;
    }
}

static t_int *vosim_perform(t_int *w){
	t_vosim *x = (t_vosim *)(w[1]);
	t_sample *in1 = (t_sample *)(w[2]);
	t_sample *in2 = (t_sample *)(w[3]);
	t_sample *out = (t_sample *)(w[4]);
	float decay = x->x_decay, duty = x->x_duty;
	float sr_inv = x->x_sr_inv;
    double *fund_phase = x->x_fund_phase;
    double *cf_phase = x->x_cf_phase;
    double *curdec = x->x_curdec;
    double *res = x->x_res;
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
    for(int j = 0; j < x->x_nchs; j++){
        for(int i = 0, n = x->x_n; i < n; i++){
            float fund;
            if(x->x_ch1 == 1)
                fund = x->x_sig1 ? in1[i] : x->x_f0_list[0];
            else
                fund = x->x_sig1 ? in1[j*n + i] : x->x_f0_list[j];
            float fund_inc = fund*sr_inv;
            if(fund_inc > 1)
                fund_inc = 1;
            float cf;
            if(x->x_ch2 == 1)
                cf = x->x_sig2 ? in2[i] : x->x_cf_list[0];
            else
                cf = x->x_sig2 ? in2[j*n + i] : x->x_cf_list[j];
            if(fund < 0)
                fund = 0;
            if(cf < fund)
                cf = fund;
            float cf_inc = cf*sr_inv;
            if(cf_inc > 1)
                cf_inc = 1;
            if(fund_phase[j] >= 1){
                curdec[j] = 1;
                fund_phase[j] -= 1;
                res[j] = fund_phase[j];
                cf_phase[j] = 0;
            }
            else{
                if(cf_phase[j] >= 1){
                    cf_phase[j] -= 1;
                    // rfund_phase is now the limit
                    float ratio = cf_inc <= 0 ? 0 : fund_inc/cf_inc;
                    double rfund_phase = fmin(1 - ratio, duty);
                    rfund_phase = rfund_phase - fund_phase[j] + res[j];
                    if(rfund_phase <= 0)
                        curdec[j] = 0;
                    else{ // a little fading
                        curdec[j] *= fmin(fmax(decay, 0), 1);
                        curdec[j] *= fmin(rfund_phase/ratio, 1.);
                    }
                }
            }
            double phase = cf_phase[j]*(double)(SINSQRSIZE-1);
            out[j*n + i] = vosim_readtab(phase) * curdec[j];
            fund_phase[j] += fund_inc, cf_phase[j] += cf_inc;
        }
    }
	x->x_res = res;
	x->x_curdec = curdec;
	x->x_cf_phase = cf_phase;
	x->x_fund_phase = fund_phase;
	return(w+5);
}

static void vosim_dsp(t_vosim *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
	float isr = 1./sp[0]->s_sr;
	if(x->x_sr_inv != isr){
		x->x_sr_inv = isr;
        vosim_reset(x);
	}
    x->x_sig1 = else_magic_inlet_connection((t_object *)x, x->x_glist, 0, &s_signal);
    x->x_sig2 = else_magic_inlet_connection((t_object *)x, x->x_glist, 1, &s_signal);
    int chs = x->x_ch1 = x->x_sig1 ? sp[0]->s_nchans : x->x_f0_list_size;
    x->x_ch2 = x->x_sig2 ? sp[1]->s_nchans : x->x_cf_list_size;
    if(x->x_ch2 > chs)
        chs = x->x_ch2;
    if(x->x_nchs != chs){
        x->x_fund_phase = (double *)resizebytes(x->x_fund_phase,
            x->x_nchs * sizeof(double), chs * sizeof(double));
        x->x_cf_phase = (double *)resizebytes(x->x_cf_phase,
            x->x_nchs * sizeof(double), chs * sizeof(double));
        x->x_res = (double *)resizebytes(x->x_res,
            x->x_nchs * sizeof(double), chs * sizeof(double));
        x->x_curdec = (double *)resizebytes(x->x_curdec,
            x->x_nchs * sizeof(double), chs * sizeof(double));
        x->x_nchs = chs;
    }
    signal_setmultiout(&sp[2], x->x_nchs);
    if((x->x_ch1 > 1 && x->x_ch1 != x->x_nchs)
    || (x->x_ch2 > 1 && x->x_ch2 != x->x_nchs)){
        dsp_add_zero(sp[2]->s_vec, x->x_nchs*x->x_n);
        pd_error(x, "[vosim~]: channel sizes mismatch");
        return;
    }
	dsp_add(vosim_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *vosim_free(t_vosim *x){
    freebytes(x->x_fund_phase, x->x_nchs * sizeof(*x->x_fund_phase));
    freebytes(x->x_cf_phase, x->x_nchs * sizeof(*x->x_cf_phase));
    freebytes(x->x_res, x->x_nchs * sizeof(*x->x_res));
    freebytes(x->x_curdec, x->x_nchs * sizeof(*x->x_curdec));
    free(x->x_f0_list);
    free(x->x_cf_list);
    return(void *)x;
}

static void *vosim_new(t_symbol *s, int ac, t_atom *av){
	t_vosim *x = (t_vosim *)pd_new(vosim_class);
    x->x_ignore = s;
    x->x_f = x->x_decay = x->x_sr_inv = 0.0;
    x->x_duty = 1;
    x->x_nchs = 1;
    float cf = 0.0f;
    x->x_f0_list_size = x->x_cf_list_size = x->x_nchs = 1;
    x->x_f0_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_cf_list = (float*)malloc(MAXLEN * sizeof(float));
    x->x_fund_phase = (double *)getbytes(sizeof(*x->x_fund_phase));
    x->x_cf_phase = (double *)getbytes(sizeof(*x->x_cf_phase));
    x->x_res = (double *)getbytes(sizeof(*x->x_res));
    x->x_curdec = (double *)getbytes(sizeof(*x->x_curdec));
    x->x_fund_phase[0] = x->x_cf_phase[0] = x->x_res[0] = x->x_curdec[0] = 0.0f;
    x->x_f0_list[0] = x->x_cf_list[0] = 0;
    if(ac){
        x->x_f0_list[0] = atom_getfloat(av);
        ac--, av++;
        if(ac){
            x->x_cf_list[0] = atom_getfloat(av);
            ac--, av++;
            if(ac){
                x->x_decay = atom_getfloat(av);
                ac--, av++;
                if(ac){
                    x->x_duty = atom_getfloat(av);
                    ac--, av++;
                }
            }
        }
    }
    signalinlet_new(&x->x_obj, cf);
	floatinlet_new(&x->x_obj, &x->x_decay);
	floatinlet_new(&x->x_obj, &x->x_duty);
	outlet_new(&x->x_obj, &s_signal);
    x->x_glist = canvas_getcurrent();
    x->x_sigscalar1 = obj_findsignalscalar((t_object *)x, 0);
    else_magic_setnan(x->x_sigscalar1);
    x->x_sigscalar2 = obj_findsignalscalar((t_object *)x, 1);
    else_magic_setnan(x->x_sigscalar2);
	return(x);
}

void vosim_tilde_setup(void){
	vosim_maketab();
    vosim_class = class_new(gensym("vosim~"), (t_newmethod)(void*)vosim_new,
        (t_method)vosim_free, sizeof(t_vosim), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(vosim_class, nullfn, gensym("signal"), 0);
    class_addmethod(vosim_class, (t_method)vosim_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(vosim_class, vosim_f0);
    class_addmethod(vosim_class, (t_method)vosim_cf, gensym("cf"), A_GIMME, 0);
    class_addmethod(vosim_class, (t_method)vosim_reset, gensym("reset"), 0);
}
