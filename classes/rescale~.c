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
}t_rescale;

static t_class *rescale_class;

void rescale_exp(t_rescale *x, t_floatarg f){
    x->x_exp = f < 0. ? 0. : f;
}

static t_int *rescale_perform2(t_int *w){
    t_rescale *x = (t_rescale *)(w[1]);
    int n = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *in3 = (t_float *)(w[5]);
    t_float *in4 = (t_float *)(w[6]);
    t_float *in5 = (t_float *)(w[7]);
    t_float *out = (t_float *)(w[8]);
    float exp = x->x_exp;
    while(n--){
        float f = *in1++;
        float il = *in2++; // Intput LOW
        float ih = *in3++; // Intput HIGH
        float ol = *in4++; // Output LOW
        float oh = *in5++; // Output HIGH
        if(exp < 0)
            exp = 0;     
        float r_in = ih - il; // range input
        float r_out = oh - ol; // range output
        *out++ = (f-il) == 0 ? ol :
            (f-il)/r_in > 0 ?
            ol + r_out * pow((f-il)/r_in, exp) :
            ol + r_out * -pow((il-f)/r_in, exp);
    }
    return (w + 9);
}

static t_int *rescale_perform1(t_int *w){
    t_rescale *x = (t_rescale *)(w[1]);
    int n = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *in3 = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);
    float exp = x->x_exp;
    while(n--){
        float f = *in1++;
        float ol = *in2++; // Output LOW
        float oh = *in3++; // Output HIGH
        if(exp < 0)
            exp = 0;
        *out++ = f == -1 ? ol :
            (f + 1) / 2 > 0 ?
            ol + (oh - ol) * pow((f + 1) / 2, exp) :
            ol + (oh - ol) * -(pow(((-f - 1) / 2), exp));
    }
    return (w + 7);
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
    return (void *)x;
}

static void *rescale_new(t_symbol *s, int ac, t_atom *av){
    t_rescale *x = (t_rescale *)pd_new(rescale_class);
    t_float min_in, max_in, min_out, max_out;
    min_in = -1;
    max_in = 1;
    min_out = 0;
    max_out = 1;
    x->x_exp = 1;
    x->x_mode = 0;
    t_int numargs = 0;
    if(ac > 0){
        numargs = ac;
        t_int argnum = 0;
        if(ac <= 3){
            while(ac){
                t_float argval = atom_getfloatarg(0, ac, av);
                switch(argnum){
                    case 0:
                        min_out = argval;
                        break;
                    case 1:
                        max_out = argval;
                        break;
                    case 2:
                        x->x_exp = argval < 0 ? 0 : argval;
                        break;
                    default:
                        break;
                };
                argnum++;
                ac--;
                av++;
            }
        }
        else{ // ac =4 || 5
            while(ac){
                t_float argval = atom_getfloatarg(0, ac, av);
                switch(argnum){
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
                        x->x_exp = argval < 0 ? 0 : argval;
                        break;
                    default:
                        break;
                };
                argnum++;
                ac--;
                av++;
            }
        }
    }
    if(numargs <= 3){
        x->x_inlet_1 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
            pd_float((t_pd *)x->x_inlet_1, min_out);
        x->x_inlet_2 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
            pd_float((t_pd *)x->x_inlet_2, max_out);
    }
    else{
        x->x_mode = 1;
        x->x_inlet_1 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
            pd_float((t_pd *)x->x_inlet_1, min_in);
        x->x_inlet_2 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
            pd_float((t_pd *)x->x_inlet_2, max_in);
        x->x_inlet_3 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
            pd_float((t_pd *)x->x_inlet_3, min_out);
        x->x_inlet_4 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
            pd_float((t_pd *)x->x_inlet_4, max_out);
    }
    outlet_new((t_object *)x, &s_signal);
    return (x);
}

void rescale_tilde_setup(void){
    rescale_class = class_new(gensym("rescale~"),(t_newmethod)rescale_new,
        (t_method)rescale_free, sizeof(t_rescale), 0, A_GIMME, 0);
    class_addmethod(rescale_class, nullfn, gensym("signal"), 0);
    class_addmethod(rescale_class, (t_method)rescale_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(rescale_class, (t_method)rescale_exp, gensym("exp"), A_FLOAT, 0);
}
