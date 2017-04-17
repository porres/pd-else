
#include "m_pd.h"
#include <math.h>

#define MAX_INTPUT 500 //maximum number of channel inlets

typedef struct _select
{
    t_object   x_obj;
    int 	  x_sigputs; //inlets excluding select idx (1 indexed)
    t_float   x_state; //0 = closed, nonzero = index of inlet to pass (1 indexed)
    t_float   x_last_state;
    t_float  **x_ivecs; // copying from matrix
    t_float  *x_ovec; // single pointer since it's an array rather than an array of arrays
    t_float  x_time; // fade time
} t_select;

static t_class *select_class;

static void select_float(t_select *x, t_float f)
{
    int state = (int)f;
    int sigputs = x->x_sigputs;
    if (state < 0) state = 0;
    if (state > sigputs) state = sigputs;
    if (state != x->x_state){
        x->x_last_state = x->x_state;
        x->x_state = state;
        }
}

static void select_time(t_select *x, t_floatarg f)
{
/*    int i;
    x->x_time = (f < 0 ? 0. : f);
    for (i = 0; i < x->x_ncells; i++)
       x->x_ramps[i] = x->x_time; */
}

static t_int *select_perform(t_int *w)
{
    t_select *x = (t_select *)(w[1]);
    int nblock = (int)(w[2]);
    t_float **ivecs = x->x_ivecs;
    t_float *ovec = x->x_ovec;
    t_float state = x->x_state;
    int sigputs = x->x_sigputs;
	int i,j;
	for(i=0; i< nblock; i++){
        int curst = (int)state;
        t_float output = 0;
        if(curst != 0){
            for(j=0; j<sigputs;j++){
                if(curst == (j+1)) output = ivecs[j][i];
                };
            };
        ovec[i] = output;
        };
    return (w + 3);
}



static void select_dsp(t_select *x, t_signal **sp)
{
	int i, nblock = sp[0]->s_n;
    t_signal **sigp = sp;
    for (i = 0; i < x->x_sigputs; i++){ //now for the sigputs
		*(x->x_ivecs+i) = (*sigp++)->s_vec;
	};
	x->x_ovec = (*sigp++)->s_vec; //now for the outlet
	dsp_add(select_perform, 2, x, nblock);
}

static void *select_new(t_symbol *s, int argc, t_atom *argv)
{
    t_select *x = (t_select *)pd_new(select_class);
    t_float sigputs = 1;
	t_float state = 0; // start off closed initially
    int i;
    int argnum = 0;
    while(argc > 0){
        if(argv -> a_type == A_FLOAT){
            t_float argval = atom_getfloatarg(0, argc, argv);
            switch(argnum){
                case 0:
					sigputs = argval;
                    break;
				case 1:
					state = argval;
					break;
                default:
                    break;
            };
            argc--;
            argv++;
            argnum++;
        };
    };
	if(sigputs < 1) sigputs = 1;
    else if(sigputs > (t_float)MAX_INTPUT) sigputs = MAX_INTPUT;
    if(state < 0) state = 0;
	else if(state > sigputs) state = sigputs;
	x->x_sigputs = (int)sigputs;
	x->x_state = (int)state;
    t_float   x_last_state = 0;
    t_float  x_time = 0;
    x->x_ivecs = getbytes(sigputs * sizeof(*x->x_ivecs));
	for (i = 1; i < sigputs; i++){
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    };
    outlet_new((t_object *)x, &s_signal);
    return (x);
}

void * select_free(t_select *x){
    
	 freebytes(x->x_ivecs, x->x_sigputs * sizeof(*x->x_ivecs));
         return (void *) x;
}

void select_tilde_setup(void)
{
    select_class = class_new(gensym("select~"), (t_newmethod)select_new, (t_method)select_free,
           sizeof(t_select), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(select_class, nullfn, gensym("signal"), 0);
    class_addfloat(select_class, select_float);
    class_addmethod(select_class, (t_method)select_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(select_class, (t_method)select_time, gensym("time"), A_FLOAT, 0);
}
