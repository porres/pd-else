// Porres 2018

#include <m_pd.h>
#include <else_alloca.h>
#include <math.h>
#include <stdlib.h>

static t_class *quantizer_class;

typedef struct _quantizer{
	t_object    x_obj;
	t_float     x_step;
    t_int       x_mode;
}t_quantizer;

static void quantizer_mode(t_quantizer *x, t_float f){
    x->x_mode = (int)f;
    if(x->x_mode < 0)
        x->x_mode = 0;
    if(x->x_mode > 4)
        x->x_mode = 4;
}

static float get_qtz(t_quantizer *x, t_float f){
	float div, qtz;	
		if(x->x_step > 0.){
			div = f/x->x_step;
			if(x->x_mode == 0) // round
				qtz = x->x_step * round(div);
			else if(x->x_mode == 1) // truncate
				qtz = x->x_step * trunc(div);
            else if(x->x_mode == 2) // floor
                qtz = x->x_step * floor(div);
            else  if(x->x_mode == 3)//ceil
                qtz = x->x_step * ceil(div);
            else{
                if(f > 0)
                    qtz = x->x_step * ceil(div);
                else
                    qtz = x->x_step * floor(div);
            }
		}
		else // quantizer is <= 0, do nothing
			qtz = f;
		return(qtz);
}

static void quantizer_list(t_quantizer *x, t_symbol *s, int argc, t_atom *argv){
    s = NULL;
    t_atom* at = ALLOCA(t_atom, argc);
	for(int i = 0; i < argc; i++) // get output list
		SETFLOAT(at+i, get_qtz(x, atom_getfloatarg(i, argc, argv)));
	outlet_list(x->x_obj.ob_outlet, &s_list, argc, at);
    FREEA(at, t_atom, argc);
}

static void *quantizer_new(t_symbol *s, int argc, t_atom *argv){
    s = NULL;
    t_quantizer *x = (t_quantizer *)pd_new(quantizer_class);
    x->x_step = 0;
    int numargs = 0;
    x->x_mode = 0;
    while(argc > 0){
        if(argv->a_type == A_FLOAT){
            switch(numargs){
                case 0:
                    x->x_step = atom_getfloatarg(0, argc, argv);
                    argc--, argv++;
                    break;
                case 1:
                    x->x_mode = (int)atom_getfloatarg(0, argc, argv);
                    argc--, argv++;
                    break;
                default:
                    argc--, argv++;
                    break;
            };
            numargs++;
        }
        else if(argv->a_type == A_SYMBOL && !numargs){
            if(atom_getsymbolarg(0, argc, argv) == gensym("-mode") && argc >= 2){
                if((argv+1)->a_type == A_FLOAT){
                    x->x_mode = (int)atom_getfloatarg(1, argc, argv);
                    argc-=2;
                    argv+=2;
                }
                else
                    goto errstate;
            }
            else
                goto errstate;
        }
        else
            goto errstate;
    };
    if(x->x_mode < 0)
        x->x_mode = 0;
    if(x->x_mode > 4)
        x->x_mode = 4;
    floatinlet_new(&x->x_obj, &x->x_step);
    outlet_new(&x->x_obj, 0);
    return(x);
errstate:
    pd_error(x, "quantizer: improper args");
    return(NULL);
}

void quantizer_setup(void){
	quantizer_class = class_new(gensym("quantizer"), (t_newmethod)quantizer_new,
        0, sizeof(t_quantizer), 0, A_GIMME, 0);
	class_addlist(quantizer_class, (t_method)quantizer_list);	
	class_addmethod(quantizer_class, (t_method)quantizer_mode,  gensym("mode"), A_FLOAT, 0);
}
