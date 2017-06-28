
#include "m_pd.h"

static t_class *fold_tilde_class;

typedef struct _fold_tilde {//fold_tilde (control rate) 
	t_object x_obj;
	t_float minval;
	t_float maxval;
	t_float x_input; //dummy var
	t_inlet *x_minlet; 
	t_inlet *x_maxlet; 
	t_outlet *x_outlet;
} t_fold_tilde;


static t_int *fold_tilde_perform(t_int *w)
{
	t_fold_tilde *x = (t_fold_tilde *)(w[1]);
	t_float *in1 = (t_float *)(w[2]);
	t_float *in2 = (t_float *)(w[3]);
	t_float *in3 = (t_float *)(w[4]);
	t_float *out = (t_float *)(w[5]);
	int n = (int)(w[6]);
	while (n--){
		float input = *in1++;
		float minv = *in2++;
		float maxv = *in3++;
		if(minv > maxv)
            {
			float temp;
			temp = maxv;
			maxv = minv;
			minv = temp;
			};
        float output;
        float range = maxv - minv;
        if(input < maxv && input >= minv) // if input in range, return input
            output = input;
        else if(minv == maxv)
            output = minv;
        else // folding
            {
            if(input < minv)
                {
                float diff = minv - input; //diff between input and minimum (positive)
                int mag = (int)(diff/range); //case where input is more than a range away from minval
                if(mag % 2 == 0)
                    { // even number of ranges away = counting up from min
                    diff = diff - ((float)mag)*range;
                    output = diff + minv;
                    }
                else
                    { // odd number of ranges away = counting down from max
                    diff = diff - ((float)mag) * range;
                    output = maxv - diff;
                    };
                }
            else
                { //input > maxv
                float diff = input - maxv; //diff between input and max (positive)
                int mag  = (int)(diff/range); //case where input is more than a range away from maxv
                if(mag % 2 == 0)
                    { //even number of ranges away = counting down from max
                    diff = diff - (float)mag*range;
                    output = maxv - diff;
                    }
                else
                    { //odd number of ranges away = counting up from min
                    diff = diff - (float)mag*range;
                    output = diff + minv;
                    };
                };
            }
		*out++ = output;
		};
	return (w+7);
}

static void fold_tilde_dsp(t_fold_tilde *x, t_signal **sp)
{
		dsp_add(fold_tilde_perform, 6, x, sp[0]->s_vec, sp[1]->s_vec,
                sp[2]->s_vec, sp[3]->s_vec, sp[0]->s_n);
}

static void *fold_tilde_new(t_symbol *s, int argc, t_atom *argv){
    t_fold_tilde *x = (t_fold_tilde *)pd_new(fold_tilde_class);
    int numargs = 0;//number of args read
    int pastargs = 0; //if any attrs have been declared yet
    x->minval = -1.;
    x-> maxval = 1.;
    
    while(argc > 0 ){
        if(argv -> a_type == A_FLOAT){ //if nullpointer, should be float or int
            if(!pastargs){//if we aren't past the args yet
                switch(numargs){
                        
                    case 0: 	x->minval = atom_getfloatarg(0, argc, argv);
                        numargs++;
                        argc--;
                        argv++;
                        break;
                        
                    case 1: 	x->maxval = atom_getfloatarg(0, argc, argv);
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
            else{
                argc--;
                argv++;
            };
        }
        else if (argv->a_type == A_SYMBOL){

            goto errstate;
        };
    };
    x->x_minlet = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    x->x_maxlet =  inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    pd_float( (t_pd *) x->x_minlet, x->minval);
    pd_float( (t_pd *) x->x_maxlet, x->maxval);
    x->x_outlet =  outlet_new(&x->x_obj, gensym("signal"));
    return (x);
errstate:
    pd_error(x, "fold~: improper args");
    return NULL;
}

void fold_tilde_setup(void){
	fold_tilde_class = class_new(gensym("fold~"), (t_newmethod)fold_tilde_new, 0,
			sizeof(t_fold_tilde), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(fold_tilde_class, (t_method)fold_tilde_dsp, gensym("dsp"), A_CANT, 0);
    CLASS_MAINSIGNALIN(fold_tilde_class, t_fold_tilde, x_input);
}
