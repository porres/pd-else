// Porres 2017

#include "m_pd.h"
#include <string.h>
#include <ctype.h>
#include <stdlib.h>

static t_class *fold_class;

typedef struct _fold
{
    t_object   x_obj;
    t_float    x_fold;
    t_inlet   *x_low_let;
    t_inlet   *x_high_let;
    t_outlet  *x_outlet;
} t_fold;


static t_int *fold_perform(t_int *w)
{
    t_fold *x = (t_fold *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *in3 = (t_float *)(w[5]);
    t_float *out = (t_sample *)(w[6]);
    while (nblock--)
        {
        t_float output;
        float input = *in1++;
        float in_low = *in2++;
        float in_high = *in3++;
        float low = in_low;
        float high = in_high;
        if(low > high)
            {
            low = in_high;
            high = in_low;
            }
        float range = high - low;
        if(low == high)
            output = low;
// fold
            /*
        if(input < low)
            {
            float diff = low - input; //diff between input and minimum (positive)
            int mag = (int)(diff/range); //case where input is more than a range away from low
            if(mag % 2 == 0)
                { // even number of ranges away = counting up from min
                diff = diff - ((float)mag)*range;
                output = diff + low;
                }
            else
                { // odd number of ranges away = counting down from max
                diff = diff - ((float)mag)*range;
                output = high - diff;
                };
            }
        else
            { //input > high
            float diff = input - high; //diff between input and max (positive)
            int mag  = (int)(diff/range); //case where input is more than a range away from high
            if(mag % 2 == 0)
                { //even number of ranges away = counting down from max
                diff = diff - (float)mag*range;
                output = high - diff;
                }
            else
                { //odd number of ranges away = counting up from min
                diff = diff - (float)mag*range;
                output = diff + low;
                };
            };
// end of fold
             */
            
            
            if(input < low){
                float diff = low - input; //diff between input and minimum (positive)
                int mag = (int)(diff/range); //case where input is more than a range away from low
                if(mag % 2 == 0){// even number of ranges away = counting up from min
                    diff = diff - ((float)mag)*range;
                    output = diff + low;
                }
                else{// odd number of ranges away = counting down from max
                    diff = diff - ((float)mag)*range;
                    output = high - diff;
                };
            }
            else{ //input > high
                float diff = input - high; //diff between input and max (positive)
                int mag  = (int)(diff/range); //case where input is more than a range away from high
                if(mag % 2 == 0){//even number of ranges away = counting down from max
                    diff = diff - (float)mag*range;
                    output = high - diff;
                }
                else{//odd number of ranges away = counting up from min
                    diff = diff - (float)mag*range;
                    output = diff + low;
                };
            };
            
            
        *out++ = output;
        }
    return (w + 7);
}

static void fold_dsp(t_fold *x, t_signal **sp)
{
    dsp_add(fold_perform, 6, x, sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *fold_free(t_fold *x)
{
    inlet_free(x->x_low_let);
    inlet_free(x->x_high_let);
    outlet_free(x->x_outlet);
    return (void *)x;
}

static void *fold_new(t_symbol *s, int ac, t_atom *av)
{
    t_fold *x = (t_fold *)pd_new(fold_class);
/////////////////////////////////////////////////////////////////////////////////////
    float init_low, low, init_high, high;
    if(ac)
        {
        if(ac == 1)
            {
            if(av -> a_type == A_FLOAT)
                {
                low = init_low = 0;
                high = init_high = atom_getfloat(av);
                }
            else goto errstate;
            }
        else if(ac == 2)
            {
            int argnum = 0;
            while(ac)
                {
                if(av -> a_type != A_FLOAT)
                    {
                    goto errstate;
                    }
                else
                    {
                    t_float curf = atom_getfloatarg(0, ac, av);
                    switch(argnum)
                        {
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
    else
        {
        low = init_low = -1;
        high = init_high = 1;
        }
    if(low > high)
        {
        low = init_high;
        high = init_low;
        }
/////////////////////////////////////////////////////////////////////////////////////
    x->x_low_let = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_low_let, low);
    x->x_high_let = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_high_let, high);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
//
    return (x);
//
    errstate:
        pd_error(x, "fold~: improper args");
        return NULL;
}

void fold_tilde_setup(void)
{
    fold_class = class_new(gensym("fold~"), (t_newmethod)fold_new,
        (t_method)fold_free, sizeof(t_fold), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(fold_class, nullfn, gensym("signal"), 0);
    class_addmethod(fold_class, (t_method)fold_dsp, gensym("dsp"), A_CANT, 0);
}
