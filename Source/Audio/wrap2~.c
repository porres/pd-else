// Porres

#include <m_pd.h>
#include "math.h"

static t_class *wrap2_class;

typedef struct _wrap2 {
	t_object    x_obj;
    t_inlet    *x_inlet_min;
    t_inlet    *x_inlet_max;
    int         x_nchans;
    t_int       x_n;
    t_int       x_ch2;
    t_int       x_ch3;
}t_wrap2;

static t_int *wrap2_perform(t_int *w){
    t_wrap2 *x = (t_wrap2 *)(w[1]);
    t_sample *in1 = (t_sample *)(w[2]);
    t_sample *in2 = (t_sample *)(w[3]);
    t_sample *in3 = (t_sample *)(w[4]);
    t_sample *out = (t_sample *)(w[5]);
	int n = x->x_n;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            float input = in1[j*n + i];
            float low = x->x_ch2 == 1 ? in2[i] : in2[j*n + i];
            float high = x->x_ch3 == 1 ? in3[i] : in3[j*n + i];
            float output;
            if(low > high){ // swap values
                 float temp = high;
                 high = low;
                 low = temp;
            };
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
            out[j*n + i] = output;
        }
    }
	return(w+6);
}


static void wrap2_dsp(t_wrap2 *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    x->x_nchans = sp[0]->s_nchans;
    x->x_ch2 = sp[1]->s_nchans, x->x_ch3 = sp[2]->s_nchans;
    signal_setmultiout(&sp[3], x->x_nchans);
    if((x->x_ch2 > 1 && x->x_ch2 != x->x_nchans)
    || (x->x_ch3 > 1 && x->x_ch3 != x->x_nchans)){
        dsp_add_zero(sp[3]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[wrap2~]: channel sizes mismatch");
        return;
    }
    dsp_add(wrap2_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *wrap2_free(t_wrap2 *x){
    inlet_free(x->x_inlet_min);
    inlet_free(x->x_inlet_max);
    return(void *)x;
}

static void *wrap2_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_wrap2 *x = (t_wrap2 *)pd_new(wrap2_class);
///////////////////////////
    float min = -1.;
    float max = 1.;
    if(ac == 1){
        if(av -> a_type == A_FLOAT){
            min = 0;
            max = atom_getfloat(av);
        }
        else
            goto errstate;
    }
    else if(ac == 2){
        int numargs = 0;
        while(ac > 0 ){
            if(av -> a_type == A_FLOAT){ // if nullpointer, should be float or int
                switch(numargs){
                    case 0: min = atom_getfloat(av);
                        numargs++;
                        ac--;
                        av++;
                        break;
                    case 1: max = atom_getfloat(av);
                        numargs++;
                        ac--;
                        av++;
                        break;
                    default:
                        ac--;
                        av++;
                        break;
                };
            }
            else // not a float
                goto errstate;
        };
    }
    else if(ac > 2)
        goto errstate;
///////////////////////////
    x->x_inlet_min = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_min, min);
    x->x_inlet_max = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_max, max);
    outlet_new(&x->x_obj, gensym("signal"));
    return(x);
errstate:
    pd_error(x, "[wrap2~]: improper args");
    return(NULL);
}

void wrap2_tilde_setup(void){
	wrap2_class = class_new(gensym("wrap2~"), (t_newmethod)wrap2_new,
        (t_method)wrap2_free, sizeof(t_wrap2), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(wrap2_class, nullfn, gensym("signal"), 0);
    class_addmethod(wrap2_class, (t_method)wrap2_dsp, gensym("dsp"), A_CANT, 0);
}
