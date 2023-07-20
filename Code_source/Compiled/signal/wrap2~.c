// Porres 2017

#include "m_pd.h"
#include "math.h"

static t_class *wrap2_class;

typedef struct _wrap2{
    t_object   x_obj;
    t_float    x_low;
    t_float    x_high;
    t_outlet  *x_outlet;
}t_wrap2;

static t_int *wrap2_perform(t_int *w){
    t_wrap2 *x = (t_wrap2 *)(w[1]);
    t_sample *in = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int n = (int)(w[4]);
    while(n--){
        t_float output;
        float input = *in++;
        float low = x->x_low;
        float high = x->x_high;
        if(low > high){
            low = x->x_high;
            high = x->x_low;
        }
        float range = high - low;
        if(low == high)
            output = low;
        else{
            if(input < low){
                output = input;
                while(output < low)
                    output += range;
            }
            else
                output = fmod(input - low, range) + low;
        }
        *out++ = output;
    }
    return(w+5);
}

static void wrap2_dsp(t_wrap2 *x, t_signal **sp){
    signal_setmultiout(&sp[1], sp[0]->s_nchans);
    dsp_add(wrap2_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec,
        ((t_int)((sp[0])->s_length * (sp[0])->s_nchans)));
}

static void *wrap2_new(t_symbol *s, int ac, t_atom *av){
    t_wrap2 *x = (t_wrap2 *)pd_new(wrap2_class);
    t_symbol *dummy = s;
    dummy = NULL;
/////////////////////////////////////////////////////////////////////////////////////
    float init_low, low, init_high, high;
    if(ac){
        if(ac == 1){
            if(av -> a_type == A_FLOAT){
                low = init_low = 0;
                high = init_high = atom_getfloat(av);
            }
            else
                goto errstate;
        }
        else if(ac == 2){
            int argnum = 0;
            while(ac){
                if(av->a_type != A_FLOAT)
                    goto errstate;
                else{
                    t_float curf = atom_getfloatarg(0, ac, av);
                    switch(argnum){
                        case 0:
                            low = init_low = curf;
                            break;
                        case 1:
                            high = init_high = curf;
                            break;
                        default:
                            break;
                    };
                };
                argnum++;
                ac--;
                av++;
            };
        }
        else goto errstate;
    }
    else{
        low = init_low = -1;
        high = init_high = 1;
    }
    if(low > high){
        low = init_high;
        high = init_low;
    }
/////////////////////////////////////////////////////////////////////////////////////
    x->x_low = low;
    x->x_high = high;
    floatinlet_new(&x->x_obj, &x->x_low);
    floatinlet_new(&x->x_obj, &x->x_high);
    outlet_new(&x->x_obj, &s_signal);
    return(x);
    errstate:
        pd_error(x, "[wrap2~]: improper args");
        return NULL;
}

void wrap2_tilde_setup(void){
    wrap2_class = class_new(gensym("wrap2~"), (t_newmethod)wrap2_new,
        0, sizeof(t_wrap2), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(wrap2_class, nullfn, gensym("signal"), 0);
    class_addmethod(wrap2_class, (t_method)wrap2_dsp, gensym("dsp"), A_CANT, 0);
}
