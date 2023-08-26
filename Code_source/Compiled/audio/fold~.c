
#include "m_pd.h"

static t_class *fold_tilde_class;

typedef struct _fold_tilde {
	t_object    x_obj;
	t_float     x_min;
	t_float     x_max;
}t_fold_tilde;

static t_int *fold_tilde_perform(t_int *w){
    t_fold_tilde *x = (t_fold_tilde *)(w[1]);
    t_sample *input = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
	int n = (int)(w[4]);
	while(n--){
		float in = *input++;
		float min = x->x_min;
		float max = x->x_max;
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
		*out++ = output;
    };
	return (w+5);
}

static void fold_tilde_dsp(t_fold_tilde *x, t_signal **sp){
    signal_setmultiout(&sp[1], sp[0]->s_nchans);
    dsp_add(fold_tilde_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec,
        ((t_int)((sp[0])->s_length * (sp[0])->s_nchans)));
}

static void *fold_tilde_new(t_symbol *s, int argc, t_atom *argv){
    s = NULL;
    t_fold_tilde *x = (t_fold_tilde *)pd_new(fold_tilde_class);
///////////////////////////
    x->x_min = -1.;
    x-> x_max = 1.;
    if(argc == 1){
        if(argv -> a_type == A_FLOAT){
            x->x_min = 0;
            x->x_max = atom_getfloat(argv);
        }
        else
            goto errstate;
    }
    else if(argc == 2){
        int numargs = 0;
        while(argc > 0 ){
            if(argv -> a_type == A_FLOAT){ // if nullpointer, should be float or int
                switch(numargs){
                    case 0: x->x_min = atom_getfloatarg(0, argc, argv);
                        numargs++;
                        argc--;
                        argv++;
                        break;
                    case 1: x->x_max = atom_getfloatarg(0, argc, argv);
                        numargs++;
                        argc--;
                        argv++;
                        break;
                    default:
                        argc--;
                        argv++;
                        break;
                };
            }
            else // not a float
                goto errstate;
        };
    }
    else if(argc > 2)
        goto errstate;
///////////////////////////
    floatinlet_new(&x->x_obj, &x->x_min);
    floatinlet_new(&x->x_obj, &x->x_max);
    outlet_new(&x->x_obj, gensym("signal"));
    return (x);
errstate:
    pd_error(x, "[fold~]: improper args");
    return NULL;
}

void fold_tilde_setup(void){
	fold_tilde_class = class_new(gensym("fold~"), (t_newmethod)fold_tilde_new, 0,
			sizeof(t_fold_tilde), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(fold_tilde_class, nullfn, gensym("signal"), 0);
    class_addmethod(fold_tilde_class, (t_method)fold_tilde_dsp, gensym("dsp"), A_CANT, 0);
}
