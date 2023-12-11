// Porres

#include "m_pd.h"

static t_class *fold_class;

typedef struct _fold {
	t_object    x_obj;
    t_inlet    *x_inlet_min;
    t_inlet    *x_inlet_max;
    int         x_nchans;
    t_int       x_n;
    t_int       x_ch2;
    t_int       x_ch3;
}t_fold;

static t_int *fold_perform(t_int *w){
    t_fold *x = (t_fold *)(w[1]);
    t_sample *in1 = (t_sample *)(w[2]);
    t_sample *in2 = (t_sample *)(w[3]);
    t_sample *in3 = (t_sample *)(w[4]);
    t_sample *out = (t_sample *)(w[5]);
	int n = x->x_n;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            float in = in1[j*n + i];
            float min = x->x_ch2 == 1 ? in2[i] : in2[j*n + i];
            float max = x->x_ch3 == 1 ? in3[i] : in3[j*n + i];
            float output;
            if(min > max){ // swap values
                float temp;
                temp = max;
                max = min;
                min = temp;
            };
            if(min == max)
                output = min;
            else if(in <= max && in >= min)
                output = in; // if in range, = in
            else{ // folding
                float range = max - min;
                if(in < min){
                    float diff = min - in; // positive diff between in and min
                    int mag = (int)(diff/range); // in is > range from min
                    if(mag % 2 == 0){ // even # of ranges away = counting up from min
                        diff = diff - ((float)mag * range);
                        output = diff + min;
                    }
                    else{ // odd # of ranges away = counting down from max
                        diff = diff - ((float)mag * range);
                        output = max - diff;
                    };
                }
                else{ // in > max
                    float diff = in - max; // positive diff between in and max
                    int mag  = (int)(diff/range); // in is > range from max
                    if(mag % 2 == 0){ // even # of ranges away = counting down from max
                        diff = diff - ((float)mag * range);
                        output = max - diff;
                    }
                    else{ // odd # of ranges away = counting up from min
                        diff = diff - ((float)mag * range);
                        output = diff + min;
                    };
                };
            }
            out[j*n + i] = output;
        }
    }
	return(w+6);
}


static void fold_dsp(t_fold *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    x->x_nchans = sp[0]->s_nchans;
    x->x_ch2 = sp[1]->s_nchans, x->x_ch3 = sp[2]->s_nchans;
    signal_setmultiout(&sp[3], x->x_nchans);
    if((x->x_ch2 > 1 && x->x_ch2 != x->x_nchans)
    || (x->x_ch3 > 1 && x->x_ch3 != x->x_nchans)){
        dsp_add_zero(sp[3]->s_vec, x->x_nchans*x->x_n);
        pd_error(x, "[fold~]: channel sizes mismatch");
        return;
    }
    dsp_add(fold_perform, 5, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void *fold_free(t_fold *x){
    inlet_free(x->x_inlet_min);
    inlet_free(x->x_inlet_max);
    return(void *)x;
}

static void *fold_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_fold *x = (t_fold *)pd_new(fold_class);
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
    pd_error(x, "[fold~]: improper args");
    return(NULL);
}

void fold_tilde_setup(void){
	fold_class = class_new(gensym("fold~"), (t_newmethod)fold_new,
        (t_method)fold_free, sizeof(t_fold), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(fold_class, nullfn, gensym("signal"), 0);
    class_addmethod(fold_class, (t_method)fold_dsp, gensym("dsp"), A_CANT, 0);
}
