// porres basically stole this from shadylib
// this is still experimental, I changed a lot of things, added some others, and will still check out other algorithms and the theory

#include "m_pd.h"
#include <math.h>

#define SINSQRSIZE 512

static t_sample *sinsqr_tbl;
static t_class *vosim_class;

typedef struct _vosim {
    t_object    x_obj;
    t_float     x_f;
    int         x_n;
    double      x_fund_phase;
    double      x_cf_phase;
    float       x_res;
    float       x_duty;          // duty cycle
    float       x_decay;         // decay coefficient
    float       x_curdec;        // current decay coefficient
    float       x_sr_inv;
    t_symbol   *x_ignore;
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

static void vosim_reset(t_vosim *x){
    x->x_fund_phase = x->x_cf_phase = x->x_res = 0.0f;
    x->x_curdec = 1.0;
}

static t_int *vosim_perform(t_int *w){
	t_vosim *x = (t_vosim *)(w[1]);
	t_sample *in1 = (t_sample *)(w[2]);
	t_sample *in2 = (t_sample *)(w[3]);
	t_sample *out = (t_sample *)(w[4]);
    int n = x->x_n;
	float decay = x->x_decay, duty = x->x_duty;
	float curdec = x->x_curdec, sr_inv = x->x_sr_inv;
	double cf_phase = x->x_cf_phase, fund_phase = x->x_fund_phase;
	float res = x->x_res;
    for(int i = 0; i < n; i++){
        float fund = in1[i];
        float cf = in2[i];
        if(fund < 0)
            fund = 0;
        if(cf < fund)
            cf = fund;
        float fund_inc = fund*sr_inv;
        float cf_inc = cf*sr_inv;
        if(fund_inc > 1)
            fund_inc = 1;
        if(cf_inc > 1)
            cf_inc = 1;
        if(fund_phase >= 1){
            curdec = 1;
            fund_phase -= 1;
            res = fund_phase;
            cf_phase = 0;
        }
        else{
            if(cf_phase >= 1){
                cf_phase -= 1;
                // rfund_phase is now the limit
                float ratio = cf_inc <= 0 ? 0 : fund_inc/cf_inc;
                double rfund_phase = fmin(1 - ratio, duty);
                rfund_phase = rfund_phase - fund_phase + res;
                if(rfund_phase <= 0)
                    curdec = 0;
                else{ // a little fading
                    curdec *= fmin(fmax(decay, 0), 1);
                    curdec *= fmin(rfund_phase/ratio, 1.);
                }
            }
        }
        double phase = cf_phase*(double)(SINSQRSIZE-1);
        out[i] = vosim_readtab(phase) * curdec;
        fund_phase += fund_inc, cf_phase += cf_inc;
    }
	x->x_res = res;
	x->x_curdec = curdec;
	x->x_cf_phase = cf_phase;
	x->x_fund_phase = fund_phase;
	return(w+5);
}

static void vosim_dsp(t_vosim *x, t_signal **sp){
	float oldsr_inv = x->x_sr_inv;
	float newsr_inv = 1./sp[0]->s_sr;
    x->x_n = sp[0]->s_n;
	if(oldsr_inv != newsr_inv){
		x->x_sr_inv = newsr_inv;
        vosim_reset(x);
	}
	dsp_add(vosim_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *vosim_new(t_symbol *s, int ac, t_atom *av){
	t_vosim *x = (t_vosim *)pd_new(vosim_class);
    x->x_ignore = s;
    x->x_f = x->x_decay = x->x_sr_inv = 0.0;
    x->x_duty = 1;
    float cf = 0.0f;
    if(ac){
        x->x_f = atom_getfloat(av);
        ac--, av++;
        if(ac){
            cf = atom_getfloat(av);
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
	return(x);
}

void vosim_tilde_setup(void){
	vosim_maketab();
    vosim_class = class_new(gensym("vosim~"), (t_newmethod)(void*)vosim_new,
        0, sizeof(t_vosim), 0, A_GIMME, 0);
    CLASS_MAINSIGNALIN(vosim_class, t_vosim, x_f);
    class_addmethod(vosim_class, (t_method)vosim_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(vosim_class, (t_method)vosim_reset, gensym("reset"), 0);
}
