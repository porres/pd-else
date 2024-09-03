// Porres 2017 - 2023

#include "m_pd.h"
#include <math.h>

typedef struct _rescale{
    t_object  x_obj;
    t_inlet  *x_inlet_minout;
    t_inlet  *x_inlet_maxout;
    t_float   x_exp;
    int       x_log;
    t_int     x_clip;
    t_float   x_minin;
    t_float   x_maxin;
    float    *x_minout;
    float    *x_maxout;
    int       x_nchans;
}t_rescale;

static t_class *rescale_class;

void rescale_exp(t_rescale *x, t_floatarg f){
    x->x_exp = f;
}

static void rescale_in(t_rescale *x, t_floatarg f1, t_floatarg f2){
    x->x_minin = f1;
    x->x_maxin = f2;
}

static void rescale_log(t_rescale *x, t_floatarg f){
    x->x_log = (f != 0);
}

static void rescale_clip(t_rescale *x, t_floatarg f){
    x->x_clip = f != 0;
}

static float rescale_convert(t_rescale *x, float f, float minout, float maxout){
    float minin = x->x_minin;
    float rangein = x->x_maxin - minin;
    float rangeout = maxout - minout;
    if(f == minin)
        return(minout);
    if(f == x->x_maxin)
        return(maxout);
    if(x->x_clip){
        if(rangein < 0){
            if(f > minin)
                return(minout);
            else if(f < x->x_maxin)
                return(maxout);
        }
        else{
            if(f < minin)
                return(minout);
            else if(f > x->x_maxin)
                return(maxout);
        }
    }
    float p = (f-minin)/rangein; // position
    if(x->x_log){ // 'log'
        if((minout <= 0 && maxout >= 0) || (minout >= 0 && maxout <= 0)){
            pd_error(x, "[rescale~: output range cannot contain '0' in log mode");
            return(0);
        }
        return(exp(p * log(maxout / minout)) * minout);
    }
    if(fabs(x->x_exp) == 1 || x->x_exp == 0) // linear
        return(minout + rangeout * p);
    if(x->x_exp > 0) // exponential
        return(pow(p, x->x_exp) * rangeout + minout);
    else // negative exponential
        return((1 - pow(1-p, -x->x_exp)) * rangeout + minout);
}

static t_int *rescale_perform(t_int *w){
    t_rescale *x = (t_rescale *)(w[1]);
    int n = (int)(w[2]);
    int ch2 = (int)(w[3]);
    int ch3 = (int)(w[4]);
    t_float *in1 = (t_float *)(w[5]);
    t_float *in2 = (t_float *)(w[6]);
    t_float *in3 = (t_float *)(w[7]);
    t_float *out = (t_float *)(w[8]);
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            t_float in = in1[j*n + i];
            t_float minout = ch2 == 1 ? in2[i] : in2[j*n + i];
            t_float maxout = ch3 == 1 ? in3[i] : in3[j*n + i];
            
            out[j*n + i] = rescale_convert(x, in, minout, maxout);
        }
    }
    return(w+9);
}

static void rescale_dsp(t_rescale *x, t_signal **sp){
    int chs = sp[0]->s_nchans, ch2 = sp[1]->s_nchans, ch3 = sp[2]->s_nchans, n = sp[0]->s_n;
    signal_setmultiout(&sp[3], chs);
    if(x->x_nchans != chs){
        x->x_minout = (float *)resizebytes(x->x_minout,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_maxout = (float *)resizebytes(x->x_maxout,
            x->x_nchans * sizeof(float), chs * sizeof(float));
        x->x_nchans = chs;
    }
    if((ch2 > 1 && ch2 != x->x_nchans) || (ch3 > 1 && ch3 != x->x_nchans)){
        dsp_add_zero(sp[3]->s_vec, chs*n);
        pd_error(x, "[rescale~]: channel sizes mismatch");
    }
    else
        dsp_add(rescale_perform, 8, x, n, ch2, ch3, sp[0]->s_vec,
            sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *rescale_free(t_rescale *x){
    inlet_free(x->x_inlet_minout);
    inlet_free(x->x_inlet_maxout);
    freebytes(x->x_minout, x->x_nchans * sizeof(*x->x_minout));
    freebytes(x->x_maxout, x->x_nchans * sizeof(*x->x_maxout));
    return(void *)x;
}

static void *rescale_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_rescale *x = (t_rescale *)pd_new(rescale_class);
    x->x_nchans = 1;
    x->x_minout = (float *)getbytes(sizeof(*x->x_minout));
    x->x_maxout = (float *)getbytes(sizeof(*x->x_maxout));
    x->x_minin = -1, x->x_maxin = 1;
    float minout = 0, maxout = 1;
    x->x_exp = 0, x->x_clip = 1;
    t_int numargs = 0;
    while(ac){
        if(av->a_type == A_SYMBOL){
            t_symbol *sym = atom_getsymbol(av);
            if(sym == gensym("-noclip") && !numargs)
                x->x_clip = 0;
            else if(sym == gensym("-log") && !numargs)
                x->x_log = 1;
            else if(ac >= 2 && sym == gensym("-exp") && !numargs){
                ac--, av++;
                x->x_exp = atom_getfloat(av);
            }
            else if(ac >= 3 && sym == gensym("-in") && !numargs){
                ac--, av++;
                x->x_minin = atom_getfloat(av);
                ac--, av++;
                x->x_maxin = atom_getfloat(av);
            }
            else
                goto errstate;
            ac--, av++;
        }
        else{
            switch(numargs){
                case 0:
                    minout = atom_getfloat(av);
                    break;
                case 1:
                    maxout = atom_getfloat(av);
                    break;
                case 2:
                    x->x_exp = atom_getfloat(av);
                    break;
                default:
                    break;
            };
            numargs++;
            ac--, av++;
        }
    }
    x->x_inlet_minout = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_minout, minout);
    x->x_inlet_maxout = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_maxout, maxout);
    outlet_new((t_object *)x, &s_signal);
    return(x);
errstate:
    post("[rescale~]: improper args");
    return(NULL);
}

void rescale_tilde_setup(void){
    rescale_class = class_new(gensym("rescale~"),(t_newmethod)rescale_new,
        (t_method)rescale_free, sizeof(t_rescale), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(rescale_class, nullfn, gensym("signal"), 0);
    class_addmethod(rescale_class, (t_method)rescale_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(rescale_class, (t_method)rescale_in, gensym("in"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(rescale_class, (t_method)rescale_exp, gensym("exp"), A_FLOAT, 0);
    class_addmethod(rescale_class, (t_method)rescale_log, gensym("log"), A_FLOAT, 0);
    class_addmethod(rescale_class, (t_method)rescale_clip, gensym("clip"), A_FLOAT, 0);
}
