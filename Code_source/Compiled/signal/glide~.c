// porres 2017

#include "m_pd.h"
#include "math.h"

typedef struct _glide{
    t_object x_obj;
    t_inlet  *x_inlet_ms;
    int     *x_n;
    int     *x_nleft;
    int      x_reset;
    int      x_nchans;
    float    *x_last_in;
    float    *x_last_out;
    float    *x_start;
    float    *x_delta;
    float    x_sr_khz;
    float    x_exp;
}t_glide;

static t_class *glide_class;

static void glide_exp(t_glide *x, t_floatarg f){
    x->x_exp = f;
}

static void glide_reset(t_glide *x){
    x->x_reset = 1;
}

static float glide_get_step(t_glide *x, int j){
    float step = (float)(x->x_n[j] - x->x_nleft[j]) / (float)x->x_n[j];
    if(fabs(x->x_exp) != 1){ // EXPONENTIAL
        if(x->x_exp >= 0){ // positive exponential
            if(x->x_delta[j] > 0)
                step = pow(step, x->x_exp);
            else
                step = 1-pow(1-step, x->x_exp);
        }
        else{ // negative exponential
            if(x->x_delta[j] > 0)
                step = 1-pow(1-step, -x->x_exp);
            else
                step = pow(step, -x->x_exp);
        }
    }
    return(step * x->x_delta[j]);
}

static t_int *glide_perform(t_int *w){
    t_glide *x = (t_glide *)(w[1]);
    int n = (int)(w[2]);
    int ch2 = (int)(w[3]);
    t_float *in1 = (t_float *)(w[4]);
    t_float *in2 = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);
    float *last_in = x->x_last_in;
    float *last_out = x->x_last_out;
    float *start = x->x_start;
    for(int i = 0; i < n; i++){
        for(int j = 0; j < x->x_nchans; j++){
            t_float in = in1[j*n + i];
            t_float ms = ch2 == 1 ? in2[i] : in2[j*n + i];
            float output;
            if(ms <= 0)
                ms  = 0;
            x->x_n[j] = (int)roundf(ms * x->x_sr_khz) + 1; // n samples
            if(x->x_n[j] == 1)
                output = last_out[j] = last_in[j] = in;
            else{
                if(x->x_reset){ // reset
                    output = last_out[j] = last_in[j] = in;
                    x->x_nleft[j] = 0;
                    if(j == (x->x_nchans - 1))
                        x->x_reset = 0;
                }
                else if(in != last_in[j]){ // input change, update
                    start[j] = last_out[j];
                    x->x_delta[j] = (in - last_out[j]);
                    x->x_nleft[j] = x->x_n[j] - 1;
                    float inc = glide_get_step(x, j);
                    output = last_out[j] = (last_out[j] + inc);
                    last_in[j] = in;
                }
                else{
                    if(x->x_nleft[j] > 0){
                        x->x_nleft[j]--;
                        float inc = glide_get_step(x, j);
                        output = last_out[j] = (start[j] + inc);
                    }
                    else
                        output = last_out[j] = last_in[j] = in;
                }
            }
            out[j*n + i] = output;
        };
    };
    x->x_start = start;
    x->x_last_in = last_in;
    x->x_last_out = last_out;
    return(w+7);
}

static void glide_dsp(t_glide *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    int chs = sp[0]->s_nchans, ch2 = sp[1]->s_nchans, n = sp[0]->s_n;
    signal_setmultiout(&sp[2], chs);
    if(x->x_nchans != chs){
        x->x_n = (int *)resizebytes(x->x_n,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_nleft = (int *)resizebytes(x->x_nleft,
            x->x_nchans * sizeof(int), chs * sizeof(int));
        x->x_last_in = (t_float *)resizebytes(x->x_last_in,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_last_out = (t_float *)resizebytes(x->x_last_out,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_start = (t_float *)resizebytes(x->x_start,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_delta = (t_float *)resizebytes(x->x_delta,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchans = chs;
    }
    if(ch2 > 1 && ch2 != x->x_nchans){
        dsp_add_zero(sp[2]->s_vec, chs*n);
        pd_error(x, "[glide~]: channel sizes mismatch");
    }
    else
        dsp_add(glide_perform, 6, x, n, ch2, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *glide_free(t_glide *x){
    inlet_free(x->x_inlet_ms);
    freebytes(x->x_n, x->x_nchans * sizeof(*x->x_n));
    freebytes(x->x_nleft, x->x_nchans * sizeof(*x->x_nleft));
    freebytes(x->x_last_in, x->x_nchans * sizeof(*x->x_last_in));
    freebytes(x->x_last_out, x->x_nchans * sizeof(*x->x_last_out));
    freebytes(x->x_start, x->x_nchans * sizeof(*x->x_start));
    freebytes(x->x_delta, x->x_nchans * sizeof(*x->x_delta));
    return(void *)x;
}

static void *glide_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_glide *x = (t_glide *)pd_new(glide_class);
    x->x_n = (int *)getbytes(sizeof(*x->x_n));
    x->x_nleft = (int *)getbytes(sizeof(*x->x_nleft));
    x->x_last_in = (t_float *)getbytes(sizeof(*x->x_last_in));
    x->x_last_out = (t_float *)getbytes(sizeof(*x->x_last_out));
    x->x_start = (t_float *)getbytes(sizeof(*x->x_start));
    x->x_delta = (t_float *)getbytes(sizeof(*x->x_delta));
    float ms = 0;
    x->x_sr_khz = sys_getsr() * 0.001;
    x->x_last_in[0] = x->x_last_out[0] = x->x_start[0] = x->x_delta[0] = 0.;
    x->x_n[0] = x->x_nleft[0] = 0;
    x->x_reset = 0;
    x->x_exp = 1.;
    x->x_nchans = 1;
    int arg = 0;
    while(ac > 0){
        if(av->a_type == A_FLOAT){
            ms = atom_getfloatarg(0, ac, av);
            ac--, av++;
            arg = 1;
        }
        else if(av->a_type == A_SYMBOL && !arg){
            if(atom_getsymbolarg(0, ac, av) == gensym("-exp")){
                if(ac >= 2){
                    ac--, av++;
                    x->x_exp = atom_getfloatarg(0, ac, av);
                    ac--, av++;
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    }
    x->x_inlet_ms = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_ms, ms);
    outlet_new((t_object *)x, &s_signal);
    return(x);
errstate:
    pd_error(x, "[glide~]: improper args");
    return(NULL);
}

void glide_tilde_setup(void){
    glide_class = class_new(gensym("glide~"), (t_newmethod)glide_new,
        (t_method)glide_free, sizeof(t_glide), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(glide_class, nullfn, gensym("signal"), 0);
    class_addmethod(glide_class, (t_method) glide_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(glide_class, (t_method)glide_reset, gensym("reset"), 0);
    class_addmethod(glide_class, (t_method)glide_exp, gensym("exp"), A_FLOAT, 0);
}

