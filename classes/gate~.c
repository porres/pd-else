
#include "m_pd.h"

#define MAX_CH 512

typedef struct _gate{
    t_object   x_obj;
	t_float   *x_mainsig;
    t_int 	   x_n_outs;
	t_float    x_state;
    t_float   *x_ivec;
    t_float  **x_ovecs;
} t_gate;

static t_class *gate_class;

static t_int *gate_perform(t_int *w){
    t_gate *x = (t_gate *)(w[1]);
    int nblock = (int)(w[2]);
	t_float *state = x->x_mainsig;
    t_float *ivec = x->x_ivec;
    t_float **ovecs = x->x_ovecs;
	int i, j;
	int n_outs = x->x_n_outs;
	for(i = 0; i < nblock; i++){
		int curst = (int)state[i];
        if (curst > n_outs)
            curst = n_outs;
        for(j = 0; j < n_outs; j++){ /* signals counted clockwise so outlets 
                                      are labeled BACKWARDS from intuition */
            int realidx = n_outs-j-1;
            if(curst == realidx+1) // if index == count
                ovecs[realidx][i] = ivec[i]; // gate_out == indexed output
            else
                ovecs[realidx][i] = 0.0;
        };
	};
    return(w+3);
}

static void gate_dsp(t_gate *x, t_signal **sp){
	int i, nblock = sp[0]->s_n;
    t_signal **sigp = sp;
	x->x_mainsig = (*sigp++)->s_vec; // gate idx
    x->x_ivec = (*sigp++)->s_vec; // now for the signal inlet
    for (i = 0; i < x->x_n_outs; i++) //now for the n_outs
		*(x->x_ovecs+i) = (*sigp++)->s_vec;
	dsp_add(gate_perform, 2, x, nblock);
}

static void *gate_new(t_floatarg f1, t_floatarg f2){
    t_gate *x = (t_gate *)pd_new(gate_class);
    t_float n_outs = f1;
	t_float state = f2;
	if(n_outs < 1)
		n_outs = 1;
    else if(n_outs > (t_float)MAX_CH)
        n_outs = MAX_CH;
    if(state < 0)
		state = 0;
	else if(state > n_outs)
		state = n_outs;
	x->x_n_outs = (int)n_outs;
	x->x_state = state; 
	x->x_ovecs = getbytes(n_outs * sizeof(*x->x_ovecs));
    inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
	for(int i = 0; i < n_outs; i++)
	 	outlet_new(&x->x_obj, gensym("signal"));
    return(x);
}

void gate_tilde_setup(void){
    gate_class = class_new(gensym("gate~"), (t_newmethod)gate_new, 0,
            sizeof(t_gate), CLASS_DEFAULT, A_DEFFLOAT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(gate_class, t_gate, x_state);
    class_addmethod(gate_class, (t_method)gate_dsp, gensym("dsp"), A_CANT, 0);
}
