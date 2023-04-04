// Porres 2017

#include "m_pd.h"
#include <math.h>

typedef struct _rescale{
    t_object  x_obj;
    t_inlet  *x_inlet_1;
    t_inlet  *x_inlet_2;
    t_inlet  *x_inlet_3;
    t_inlet  *x_inlet_4;
    t_float   x_exp;
    t_int     x_mode;
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

static void rescale_clip(t_rescale *x, t_floatarg f){
    x->x_clip = f != 0;
}

static float convert(t_rescale *x, float f){
    float minin = x->x_minin;
    float minout = x->x_minout;
    float maxout = x->x_maxout;
    float rangein = x->x_maxin - minin;
    float rangeout = maxout - minout;
    float e = x->x_exp;
    if(f == minin)
        return(minout);
    if(f == x->x_maxin)
        return(maxout);
    if(x->x_clip){
        if(f < minin)
            return(minout);
        else if(f > x->x_maxin)
            return(maxout);
    }
    if(e == 0) // linear
        return(minout + rangeout * (f-minin)/rangein);
    float p = (f-minin)/rangein; // position
    if(e == 1){ // 'log'
        if((minout <= 0 && maxout >= 0) || (minout >= 0 && maxout <= 0)){
            pd_error(x, "[rescale~]: output range cannot contain '0' in log mode");
            return(0);
        }
        return(exp(p * log(maxout / minout)) * minout);
    }
    if(e > 0) // exponential
        return(pow(p, e) * rangeout + minout);
    else // negative exponential
        return((1 - pow(1-p, -e)) * rangeout + minout);
}

static t_int *rescale_perform2(t_int *w){
    t_rescale *x = (t_rescale *)(w[1]);
    t_int n = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *in3 = (t_float *)(w[5]);
    t_float *in4 = (t_float *)(w[6]);
    t_float *in5 = (t_float *)(w[7]);
    t_float *out = (t_float *)(w[8]);
    while(n--){
        float f = *in1++;
        x->x_minin = *in2++; // Input LOW
        x->x_maxin = *in3++; // Input HIGH
        x->x_minout = *in4++; // Output LOW
        x->x_maxout = *in5++; // Output HIGH
        *out++ = convert(x, f);
    }
    return(w+9);
}

static t_int *rescale_perform1(t_int *w){
    t_rescale *x = (t_rescale *)(w[1]);
    int n = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *in3 = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);
    while(n--){
        float f = *in1++;
        x->x_minout = *in2++; // Output LOW
        x->x_maxout = *in3++; // Output HIGH
        *out++ = convert(x, f);
    }
    return(w+7);
}

static void rescale_dsp(t_rescale *x, t_signal **sp){
    if(!x->x_mode){
        dsp_add(rescale_perform1, 6, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec,
                sp[2]->s_vec, sp[3]->s_vec);
    }
    else{
        dsp_add(rescale_perform2, 8, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec,
                sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec);
    }
}

static void *rescale_free(t_rescale *x){
    inlet_free(x->x_inlet_1);
    inlet_free(x->x_inlet_2);
    if(x->x_mode){
        inlet_free(x->x_inlet_3);
        inlet_free(x->x_inlet_4);
    }
    return(void *)x;
}

static void *rescale_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_rescale *x = (t_rescale *)pd_new(rescale_class);
    t_float min_in, max_in, min_out, max_out;
    x->x_minin = -1;
    x->x_maxin = 1;
    x->x_minout = 0;
    x->x_maxout = 1;
    x->x_exp = 0;
    x->x_mode = 0;
    x->x_clip = 0;
    t_int numargs = 0;
    if(ac > 0){
        if(av->a_type == A_SYMBOL){
            if(atom_getsymbolarg(0, ac, av) == gensym("-clip") && !numargs){
                x->x_clip = 1;
                ac--, av++;
            }
            else
                goto errstate;
        }
        if(ac <= 3){
            while(ac){
                if(av->a_type == A_FLOAT){
                    t_float argval = atom_getfloatarg(0, ac, av);
                    switch(numargs){
                        case 0:
                            min_out = argval;
                            break;
                        case 1:
                            max_out = argval;
                            break;
                        case 2:
                            x->x_exp = argval;
                            break;
                        default:
                            break;
                    };
                }
                else
                    goto errstate;
                numargs++;
                ac--, av++;
            }
        }
        else if(ac <= 5){ // numargs = 4 || 5
            while(ac){
                if(av->a_type == A_FLOAT){
                    t_float argval = atom_getfloatarg(0, ac, av);
                    switch(numargs){
                        case 0:
                            min_in = argval;
                            break;
                        case 1:
                            max_in = argval;
                            break;
                        case 2:
                            min_out = argval;
                            break;
                        case 3:
                            max_out = argval;
                            break;
                        case 4:
                            x->x_exp = argval;
                            break;
                        default:
                            break;
                    };
                }
                else
                    goto errstate;
                numargs++;
                ac--, av++;
            }
        }
        else
            goto errstate;
    }
    if(numargs <= 3){
        x->x_inlet_1 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
            pd_float((t_pd *)x->x_inlet_1, x->x_minout = min_out);
        x->x_inlet_2 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
            pd_float((t_pd *)x->x_inlet_2, x->x_maxout = max_out);
    }
    else{
        x->x_mode = 1;
        x->x_inlet_1 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
            pd_float((t_pd *)x->x_inlet_1, x->x_minin = min_in);
        x->x_inlet_2 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
            pd_float((t_pd *)x->x_inlet_2, x->x_maxin = max_in);
        x->x_inlet_3 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
            pd_float((t_pd *)x->x_inlet_3, x->x_minout = min_out);
        x->x_inlet_4 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
            pd_float((t_pd *)x->x_inlet_4, x->x_maxout = max_out);
    }
    outlet_new((t_object *)x, &s_signal);
    return(x);
errstate:
    post("[rescale~]: improper args");
    return(NULL);
}

void rescale_tilde_setup(void){
    rescale_class = class_new(gensym("rescale~"),(t_newmethod)rescale_new,
        (t_method)rescale_free, sizeof(t_rescale), 0, A_GIMME, 0);
    class_addmethod(rescale_class, nullfn, gensym("signal"), 0);
    class_addmethod(rescale_class, (t_method)rescale_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(rescale_class, (t_method)rescale_exp, gensym("exp"), A_FLOAT, 0);
    class_addmethod(rescale_class, (t_method)rescale_clip, gensym("clip"), A_FLOAT, 0);
}
