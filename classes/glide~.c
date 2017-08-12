// porres 2017

#include "m_pd.h"
#include "math.h"

typedef struct _glide{
    t_object x_obj;
    t_float  x_in;
    t_inlet  *x_inlet_ms;
    int      x_n;
    double   x_coef;
    t_float  x_last;
    t_float  x_target;
    t_float  x_sr_khz;
    double   x_incr;
    int      x_nleft;
    int      x_reset;
} t_glide;


static void glide_reset(t_glide *x){
    x->x_reset = 1;
}

static t_class *glide_class;

static t_int *glide_perform(t_int *w){
    t_glide *x = (t_glide *)(w[1]);
    int nblock = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    t_float last = x->x_last;
    t_float target = x->x_target;
    double incr = x->x_incr;
    int nleft = x->x_nleft;
    while (nblock--){
        t_float f = *in1++;
        t_float ms = *in2++;
        if (ms < 0)
            ms = 0;
        x->x_n = roundf(ms * x->x_sr_khz);
        double coef;
        if (x->x_n == 0)
            coef = 0.;
        else
            coef = 1. / (float)x->x_n;
        
        if(x->x_reset){
            incr = 0.;
            nleft = 0;
            *out++ = last = target = f;
            x->x_reset = 0;
        }
        
        else if(coef != x->x_coef){
            x->x_coef = coef;
            if (f != last){
                if (x->x_n > 1){
                    incr = (f - last) * x->x_coef;
                    nleft = x->x_n;
                    *out++ = (last += incr);
                    continue;
                    }
                }
            incr = 0.;
            nleft = 0;
            *out++ = last = f;
            }
        
        else if (f != target){
            target = f;
            if (f != last){
                if (x->x_n > 1){
                    incr = (f - last) * x->x_coef;
                    nleft = x->x_n;
                    *out++ = (last += incr);
                    continue;
                }
            }
	    incr = 0.;
	    nleft = 0;
	    *out++ = last = f;
        }
        
        else if (nleft > 0){
            *out++ = (last += incr);
            if (--nleft == 1){
                incr = 0.;
                last = target;
                }
            }
        else *out++ = target;
        };
    x->x_last = (PD_BIGORSMALL(last) ? 0. : last);
    x->x_target = (PD_BIGORSMALL(target) ? 0. : target);
    x->x_incr = incr;
    x->x_nleft = nleft;
    return (w + 6);
}

static void glide_dsp(t_glide *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    dsp_add(glide_perform, 5, x, sp[0]->s_n, sp[0]->s_vec,
        sp[1]->s_vec, sp[2]->s_vec);
}

static void *glide_new(t_floatarg ms){
    t_glide *x = (t_glide *)pd_new(glide_class);
    x->x_sr_khz = sys_getsr() * 0.001;
    x->x_last = 0.;
    x->x_target = 0.;
    x->x_incr = 0.;
    x->x_nleft = 0;
    x->x_reset = 0;
    x->x_coef = 0.;
    x->x_inlet_ms = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_ms, ms);
    outlet_new((t_object *)x, &s_signal);
    return (x);
}

void glide_tilde_setup(void){
    glide_class = class_new(gensym("glide~"), (t_newmethod)glide_new, 0,
				 sizeof(t_glide), 0, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(glide_class, t_glide, x_in);
    class_addmethod(glide_class, (t_method) glide_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(glide_class, (t_method)glide_reset, gensym("reset"), 0);
}
