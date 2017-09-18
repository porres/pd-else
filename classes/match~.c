// porres 2017

#include "m_pd.h"
#include <stdlib.h>

typedef struct _match{
	t_object x_obj;
    t_float x_f;
    t_float x_lastin;
	t_float *matches; // store numbers to match against
	t_float *trigger_vec; // copy of input vector
	t_int length; // number of matches to check
    t_float **ins; // array of input signal vectors
    t_float **outs; // array of output signal vectors
} t_match;


static t_class *match_class;

void *match_new(t_symbol *msg, short argc, t_atom *argv);
void match_free(t_match *x);
void match_dsp(t_match *x, t_signal **sp);


void *match_new(t_symbol *msg, short argc, t_atom *argv){
	int i;
	t_match *x = (t_match *)pd_new(match_class);
	x->length = (t_int)argc;
	for(i=0; i< x->length ; i++)
        outlet_new(&x->x_obj, gensym("signal"));
	x->matches = (double *) malloc(x->length * sizeof(double));
	for(i = 0; i < argc; i++)
		x->matches[i] = (double)atom_getfloatarg(i,argc,argv);
    x->ins = (t_float **) malloc(1 * sizeof(t_float *));
    x->outs = (t_float **) malloc(x->length * sizeof(t_float *));
    for(i = 0; i < 1; i++) // ???????????
        x->ins[i] = (t_float *) malloc(8192 * sizeof(t_float));
	return x;
}

void match_free(t_match *x){
	free(x->matches);
    free(x->outs);
    free(x->ins[0]);
    free(x->ins);
}

t_int *match_perform(t_int *w){
    int i, j;
    t_match *x = (t_match *) w[1];
    t_float **ins = x->ins;
    t_float **outs = x->outs;
    t_float *invec;
	t_float *inlet;
	t_float *match_outlet;
	t_float *matches = x->matches;
    t_int length = x->length;
    t_float last = x->x_lastin;
    int n = (int) w[length + 3];
    for(i = 0; i < 1; i++){ // copy input vectors
        invec = (t_float *) w[2 + i];
        for(j = 0; j < n; j++)
            ins[i][j] = invec[j];
    }
    inlet = ins[0];
    for(i = 0; i < length; i++)  // assign output vector pointers
        outs[i] = (t_float *) w[3 + i];
    for(j = 0; j < length; j++){ // clean each outlet
		match_outlet = (double *) outs[j];
		for(i = 0; i < n; i++)
			match_outlet[i] = 0.0;
	}
	for(i = 0; i < n; i++){ // match & route
		if(inlet[i]){
			for(j = 0; j < length; j++){
				if( inlet[i] == matches[j]){
					match_outlet = (double *) outs[j];
					match_outlet[i] = 1.0; // always send a unity click
				}
			}
		}
	}
    x->x_lastin = inlet[n-1];
    return (w + length + 4);
}

/*
    for(i = 0; i < n; i++){
        for(j = 0; j < length; j++){ // match & route
            if(inlet[i] == matches[j] && inlet[i] != last){
                match_outlet = (double *) outs[j];
                match_outlet[i] = 1.0;
            }
            last = inlet[i];
        }
    } */


void match_dsp(t_match *x, t_signal **sp)
{
	long i;
    t_int **sigvec;
    int pointer_count = x->length + 3; // output chans + object + inchan + vectorsize
    if(!sp[0]->s_sr)
        return;
    sigvec  = (t_int **) calloc(pointer_count, sizeof(t_int *));
	for(i = 0; i < pointer_count; i++)
		sigvec[i] = (t_int *) calloc(sizeof(t_int),1);
	sigvec[0] = (t_int *)x; // first pointer is to the object
	sigvec[pointer_count - 1] = (t_int *)sp[0]->s_n; // last pointer is to vector size (N)
	for(i = 1; i < pointer_count - 1; i++) // now attach the inlet and all outlets
		sigvec[i] = (t_int *)sp[i-1]->s_vec;
    dsp_addv(match_perform, pointer_count, (t_int *)sigvec);
    free(sigvec);
}

void match_tilde_setup(void){
    match_class = class_new(gensym("match~"), (t_newmethod)match_new,
                            (t_method)match_free, sizeof(t_match),0,A_GIMME,0);
    CLASS_MAINSIGNALIN(match_class, t_match, x_f);
    class_addmethod(match_class, (t_method)match_dsp, gensym("dsp"), A_CANT, 0);
}
