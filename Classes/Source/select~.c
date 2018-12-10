
#include "m_pd.h"

#define MAX_INS 512 

typedef struct _select{
    t_object   x_obj;
	t_float   *x_mainsig;   // index input
    t_int 	   x_n_inputs;  // # of inputs
	t_float    x_state;     // dummy
    t_float  **x_ivecs;     // input vectors
    t_float   *x_ovec;      // output vector
}t_select;

static t_class *select_class;

static t_int *select_perform(t_int *w){
    t_select *x = (t_select *)(w[1]);
    int nblock = (int)(w[2]);
	t_float *state = x->x_mainsig;
    t_float **ivecs = x->x_ivecs;
    t_float *ovec = x->x_ovec;
	int i, j;
	int sigputs = x->x_n_inputs;
	for(i = 0; i < nblock; i++){
        int curst = (int)state[i];
        if(curst > sigputs)
            curst = sigputs;
		t_float output = 0;
		if(curst != 0){
			for(j = 0; j < sigputs; j++){
				if(curst == (j + 1))
                    output = ivecs[j][i];
			};
		};
		ovec[i] = output;
	};
    return(w+3);
}


static void select_dsp(t_select *x, t_signal **sp){
	int i, nblock = sp[0]->s_n;
    t_signal **sigp = sp;
	x->x_mainsig = (*sigp++)->s_vec; // index
    for(i = 0; i < x->x_n_inputs; i++) // inputs
		*(x->x_ivecs+i) = (*sigp++)->s_vec;
	x->x_ovec = (*sigp++)->s_vec; // outlet
	dsp_add(select_perform, 2, x, nblock);
}

static void *select_new(t_floatarg f1, t_floatarg f2){
    t_select *x = (t_select *)pd_new(select_class);
    t_float sigputs = f1;
	t_float state = f2;
    if(sigputs < 1)
		sigputs = 1;
    else if(sigputs > (t_float)MAX_INS)
        sigputs = MAX_INS;
    if(state < 0)
		state = 0;
	else if(state > sigputs)
		state = sigputs;
	x->x_n_inputs = (int)sigputs;
	x->x_state = state; 
	x->x_ivecs = getbytes(sigputs * sizeof(*x->x_ivecs));
	for(int i = 0; i < sigputs; i++)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    outlet_new((t_object *)x, &s_signal);
    return (x);
}

void * select_free(t_select *x){
	freebytes(x->x_ivecs, x->x_n_inputs * sizeof(*x->x_ivecs));
    return (void *) x;
}

void select_tilde_setup(void){
    select_class = class_new(gensym("select~"), (t_newmethod)select_new, (t_method)select_free,
                             sizeof(t_select), CLASS_DEFAULT, A_DEFFLOAT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(select_class, t_select, x_state);
    class_addmethod(select_class, (t_method)select_dsp, gensym("dsp"), A_CANT, 0);
}
