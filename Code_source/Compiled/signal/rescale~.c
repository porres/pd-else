// Porres 2017 - 2023

#include "m_pd.h"
#include <math.h>

typedef struct _rescale{
    t_object  x_obj;
    t_float   x_exp;
    int       x_log;
    t_int     x_clip;
    t_float   x_minin;
    t_float   x_maxin;
    t_float   x_minout;
    t_float   x_maxout;
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

static float convert(t_rescale *x, float f){
    float minin = x->x_minin;
    float minout = x->x_minout;
    float maxout = x->x_maxout;
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
    t_sample *in = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    while(n--)
        *out++ = convert(x, *in++);
    return(w+5);
}

static void rescale_dsp(t_rescale *x, t_signal **sp){
    signal_setmultiout(&sp[1], sp[0]->s_nchans);
    dsp_add(rescale_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec,
        ((t_int)((sp[0])->s_length * (sp[0])->s_nchans)));
}

static void *rescale_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_rescale *x = (t_rescale *)pd_new(rescale_class);
    x->x_minin = -1, x->x_maxin = 1, x->x_minout = 0, x->x_maxout = 1;
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
                    x->x_minout = atom_getfloat(av);
                    break;
                case 1:
                    x->x_maxout = atom_getfloat(av);
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
    floatinlet_new(&x->x_obj, &x->x_minout);
    floatinlet_new(&x->x_obj, &x->x_maxout);
    outlet_new((t_object *)x, &s_signal);
    return(x);
errstate:
    post("[rescale~]: improper args");
    return(NULL);
}

void rescale_tilde_setup(void){
    rescale_class = class_new(gensym("rescale~"),(t_newmethod)rescale_new,
        0, sizeof(t_rescale), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(rescale_class, nullfn, gensym("signal"), 0);
    class_addmethod(rescale_class, (t_method)rescale_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(rescale_class, (t_method)rescale_in, gensym("in"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(rescale_class, (t_method)rescale_exp, gensym("exp"), A_FLOAT, 0);
    class_addmethod(rescale_class, (t_method)rescale_log, gensym("log"), A_FLOAT, 0);
    class_addmethod(rescale_class, (t_method)rescale_clip, gensym("clip"), A_FLOAT, 0);
}
