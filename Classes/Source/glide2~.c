// porres 2019

#include "m_pd.h"
#include "math.h"

typedef struct _glide2{
    t_object x_obj;
    t_inlet  *glide2;
    t_float  x_in;
    int      x_nup;
    int      x_ndown;
    double   x_upcoef;
    double   x_downcoef;
    t_float  x_last;
    t_float  x_target;
    t_float  x_sr_khz;
    double   x_incr;
    int      x_nleft;
    int      x_change; // reset coeffs
}t_glide2;

static t_class *glide2_class;

static t_int *glide2_perform(t_int *w){
    t_glide2 *x = (t_glide2 *)(w[1]);
    int n = (int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    t_float last = x->x_last;
    t_float target = x->x_target;
    int change = x->x_change;
    double incr = x->x_incr;
    int nleft = x->x_nleft;
    while(n--){
        t_float f = *in++;
        if(f != target || change){
            target = f;
            change = 0;
            if(f > last){
                if(x->x_nup > 1){
                    incr = (f - last) * x->x_upcoef;
                    nleft = x->x_nup;
                    *out++ = (last += incr);
                    continue;
                }
            }
            else if(f < last){
                if(x->x_ndown > 1){
                    incr = (f - last) * x->x_downcoef;
                    nleft = x->x_ndown;
                    *out++ = (last += incr);
                    continue;
                }
            }
            incr = 0.;
            nleft = 0;
            *out++ = last = f;
        }
        else if(nleft > 0){
            *out++ = (last += incr);
            if(--nleft == 1){
                incr = 0.;
                last = target;
            }
        }
        else
            *out++ = target;
    };
    x->x_change = change;
    x->x_last = (PD_BIGORSMALL(last) ? 0. : last);
    x->x_target = (PD_BIGORSMALL(target) ? 0. : target);
    x->x_incr = incr;
    x->x_nleft = nleft;
    return(w+5);
}

static void glide2_dsp(t_glide2 *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    dsp_add(glide2_perform, 4, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec);
}

static void glide2_up(t_glide2 *x, t_floatarg f){
    double upcoef;
    int i = (int)roundf(f * x->x_sr_khz);
    if(i > 1){
        x->x_nup = i;
        upcoef = 1. / (float)i;
    }
    else{
        x->x_nup = 0;
        upcoef = 0.;
    };
    if(upcoef != x->x_upcoef){
        x->x_upcoef = upcoef;
        x->x_change = 1;
    };
}

static void glide2_down(t_glide2 *x, t_floatarg f){
    double downcoef;
    int i = (int)roundf(f * x->x_sr_khz);
    if(i > 1){
        x->x_ndown = i;
        downcoef = 1./(float)i;
    }
    else{
        x->x_ndown = 0;
        downcoef = 0.;
    };
    if(downcoef != x->x_downcoef){
        x->x_downcoef = downcoef;
        x->x_change = 1;
    };
}

static void *glide2_new(t_floatarg up, t_floatarg down){
    t_glide2 *x = (t_glide2 *)pd_new(glide2_class);
    x->x_sr_khz = sys_getsr() * 0.001;
    glide2_up(x, up);
    glide2_down(x, down);
    x->x_change = 1;
    x->x_last = x->x_target = x->x_incr = x->x_nleft = 0;
    inlet_new((t_object *)x, (t_pd *)x, &s_float, gensym("up"));
    inlet_new((t_object *)x, (t_pd *)x, &s_float, gensym("down"));
    outlet_new((t_object *)x, &s_signal);
    return(x);
}

void glide2_tilde_setup(void){
    glide2_class = class_new(gensym("glide2~"), (t_newmethod)glide2_new, 0,
            sizeof(t_glide2), 0, A_DEFFLOAT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(glide2_class, t_glide2, x_in);
    class_addmethod(glide2_class, (t_method) glide2_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(glide2_class, (t_method)glide2_up, gensym("up"), A_FLOAT, 0);
    class_addmethod(glide2_class, (t_method)glide2_down, gensym("down"), A_FLOAT, 0);
}
