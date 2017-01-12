// Porres 2016

#include "m_pd.h"
#include <math.h>
#include <string.h>

#define RESCALE_MININ  0.
#define RESCALE_MAXIN  127.
#define RESCALE_MINOUT  0.
#define RESCALE_MAXOUT  1.
#define RESCALE_EXPO  1.

typedef struct _rescale
{
    t_object x_obj;
    t_inlet  *x_inlet_1;
    t_inlet  *x_inlet_2;
    t_inlet  *x_inlet_3;
    t_inlet  *x_inlet_4;
    t_inlet  *x_inlet_5;
    t_int    x_classic;
} t_rescale;

static t_class *rescale_class;

static t_int *rescale_perform(t_int *w)
{
    t_rescale *x = (t_rescale *)(w[1]);
    int nblock = (int)(w[2]);
    t_float *in1 = (t_float *)(w[3]);
    t_float *in2 = (t_float *)(w[4]);
    t_float *in3 = (t_float *)(w[5]);
    t_float *in4 = (t_float *)(w[6]);
    t_float *in5 = (t_float *)(w[7]);
    t_float *in6 = (t_float *)(w[8]);
    t_float *out = (t_float *)(w[9]);
    t_int classic_flag = x->x_classic;
    while (nblock--)
    {
    float in = *in1++;
    float il = *in2++; // Input LOW
    float ih = *in3++; // Input HIGH
    float ol = *in4++; // Output LOW
    float oh = *in5++; // Output HIGH
    float p = *in6++; // power (exponential) factor
    float output;
    if (classic_flag != 0)
        {p = p <= 1 ? 1 : p;
        if (p == 1) output = ((in - il) / (ih - il) == 0) ? ol
            : (((in - il) / (ih - il)) > 0) ? (ol + (oh - ol) * pow((in - il) / (ih - il), p))
            : (ol + (oh - ol) * -(pow(((-in + il) / (ih - il)), p)));
        else
            {output = ol + (oh - ol) * ((oh - ol) * exp((il - ih) * log(p)) * exp(in * log(p)));
            if (oh - ol <= 0) output = output * -1;
            
            }
        }
        else {
        p = p <= 0 ? 0 : p;
        output = ((in - il) / (ih - il) == 0) ? ol :
        (((in - il) / (ih - il)) > 0) ?
        (ol + (oh - ol) * pow((in - il) / (ih - il), p)) :
        (ol + (oh - ol) * -(pow(((-in + il) / (ih - il)), p)));
        }
    *out++ = output;
    }
    return (w + 10);
}

static void rescale_dsp(t_rescale *x, t_signal **sp)
{
    dsp_add(rescale_perform, 9, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec,
            sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec, sp[5]->s_vec, sp[6]->s_vec);
}

static void *rescale_free(t_rescale *x)
{
		inlet_free(x->x_inlet_1);
        inlet_free(x->x_inlet_2);
        inlet_free(x->x_inlet_3);
        inlet_free(x->x_inlet_4);
        inlet_free(x->x_inlet_5);
        return (void *)x;
}

static void *rescale_new(t_symbol *s, int argc, t_atom *argv)
{
    t_rescale *x = (t_rescale *)pd_new(rescale_class);
    t_float min_in, max_in, min_out, max_out, exponential;
    min_in = RESCALE_MININ;
    max_in = RESCALE_MAXIN;
    min_out = RESCALE_MINOUT;
    max_out = RESCALE_MAXOUT;
    exponential = RESCALE_EXPO;
    t_int classic_exp;
    classic_exp = 1;
    
	int argnum = 0;
	while(argc > 0){
		if(argv -> a_type == A_FLOAT)
        {
			t_float argval = atom_getfloatarg(0,argc,argv);
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
                        exponential = argval;
                        break;
					default:
						break;
				};
				argc--;
				argv++;
				argnum++;
        }
// attribute
        else if(argv -> a_type == A_SYMBOL){
            t_symbol *curarg = atom_getsymbolarg(0, argc, argv);
            if(strcmp(curarg->s_name, "@classic")==0){
                if(argc >= 2){
                    t_float argval = atom_getfloatarg(1, argc, argv);
                    classic_exp = (int)argval;
                    argc-=2;
                    argv+=2;
                }
                else{
                    goto errstate;
                };
            }
            else{
                goto errstate;
            };
            
        }
        else{
            goto errstate;
        };
    };
    
	x->x_inlet_1 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
	pd_float((t_pd *)x->x_inlet_1, min_in);
    x->x_inlet_2 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_2, max_in);
    x->x_inlet_3 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_3, min_out);
    x->x_inlet_4 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_4, max_out);
    x->x_inlet_5 = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_5, exponential);

    outlet_new((t_object *)x, &s_signal);
    
    x->x_classic = classic_exp;
    
    return (x);
	errstate:
		pd_error(x, "rescale~: improper args");
		return NULL;
}

void rescale_tilde_setup(void)
{
    rescale_class = class_new(gensym("rescale~"),
				(t_newmethod)rescale_new,
                (t_method)rescale_free,
				sizeof(t_rescale), 0, A_GIMME, 0);
    class_addmethod(rescale_class, nullfn, gensym("signal"), 0);
    class_addmethod(rescale_class, (t_method)rescale_dsp, gensym("dsp"), A_CANT, 0);
}
