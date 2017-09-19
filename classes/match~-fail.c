// porres 2017

#include "m_pd.h"
#include <stdlib.h>

typedef struct _match{
	t_object x_obj;
    t_float x_f;
    t_float x_lastin;
    t_int x_2nd_let;
    t_inlet         *x_match_let;
	t_float *matches; // numbers to match against
    t_int   x_length;
    t_float **ins; // input vectors
    t_float **outs; // output vectors
} t_match;


static t_class *match_class;

void *match_new(t_symbol *msg, short argc, t_atom *argv);
void match_free(t_match *x);
void match_dsp(t_match *x, t_signal **sp);

t_int *match_perform(t_int *w){
    int i, j;
    t_match *x = (t_match *) w[1];
    t_float **ins = x->ins;
    t_float **outs = x->outs;
    t_float *invec;
	t_float *inlet;
	t_float *match_outlet;
	t_float *matches = x->matches;
    t_int length = x->x_length;
    t_float last = x->x_lastin;
    t_int matched;
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
        matched = 0;
        if (inlet[i] != last){ // if changed
			for(j = 0; j < length - 1; j++){
                if(inlet[i] == matches[j]){ // if matched
                    match_outlet = (double *) outs[j];
                    match_outlet[i] = 1.0; // always send a unity click
                    matched = 1;
                }
			}
            if(!matched){
                match_outlet = (double *) outs[length - 1];
                match_outlet[i] = 1.0;
           }
        }
        last = inlet[i];
	}
    x->x_lastin = inlet[n-1];
    return (w + length + 4);
}

t_int *match_perform2(t_int *w){
    int i, j;
    t_match *x = (t_match *) w[1]; // 1st is object
    t_float **ins = x->ins;
    t_float **outs = x->outs;
    t_float *invec;
    t_float *matchvec;
    t_float *inlet;
    t_float *matchlet;
    t_float *match_outlet;
    t_int length = x->x_length;
    t_float last = x->x_lastin;
    t_int matched;
    int n = (int) w[7];
    invec = (t_float *) w[2]; // copy input vector
    for(j = 0; j < n; j++)
        ins[0][j] = invec[j];
    inlet = ins[0];
    matchvec = (t_float *) w[3]; // copy match vector
    for(j = 0; j < n; j++)
        ins[1][j] = matchvec[j];
    matchlet = ins[1];
    for(i = 0; i < 2; i++)  // assign output vector pointers
        outs[i] = (t_float *) w[4 + i];
    for(j = 0; j < 2; j++){ // clean each outlet
        match_outlet = (double *) outs[j];
        for(i = 0; i < n; i++)
            match_outlet[i] = 0.0;
    }
    for(i = 0; i < n; i++){ // match & route
        matched = 0;
        if (inlet[i] != last){ // if changed
            if(inlet[i] == matchlet[i]){ // if matched
                match_outlet = (double *) outs[0];
                match_outlet[i] = 1.0; // always send a unity click
                matched = 1;
            }
            else{
                match_outlet = (double *) outs[1];
                match_outlet[i] = 1.0;
            }
        }
        last = inlet[i];
    }
    x->x_lastin = inlet[n-1];
    return (w + length + 4);
}

void match_dsp(t_match *x, t_signal **sp)
{
	long i;
    t_int **sigvec;
    int pointer_count = x->x_length + 3;
    if(!sp[0]->s_sr)
        return;
    sigvec  = (t_int **) calloc(pointer_count, sizeof(t_int *));
	for(i = 0; i < pointer_count; i++)
		sigvec[i] = (t_int *) calloc(sizeof(t_int),1);
	sigvec[0] = (t_int *)x; // first pointer is to the object
	sigvec[pointer_count - 1] = (t_int *)sp[0]->s_n; // last is vector size (n)
	for(i = 1; i < pointer_count - 1; i++) // now I/O
		sigvec[i] = (t_int *)sp[i-1]->s_vec;
    if(x->x_2nd_let)
        dsp_addv(match_perform2, pointer_count, (t_int *)sigvec);
    else
        dsp_addv(match_perform, pointer_count, (t_int *)sigvec);
    free(sigvec);
}

void match_free(t_match *x){
    inlet_free(x->x_match_let);
    free(x->matches);
    free(x->outs);
    free(x->ins);
}


void *match_new(t_symbol *msg, short argc, t_atom *argv){
    t_match *x = (t_match *)pd_new(match_class);
    x->x_lastin = 0;
    if(!argc){
        x->x_length = 3; // out + 2nd in
        x->x_match_let = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
            pd_float((t_pd *)x->x_match_let, 0);
        outlet_new(&x->x_obj, gensym("signal"));
        outlet_new(&x->x_obj, gensym("signal"));
        x->x_2nd_let = 1;
        x->ins = (t_float **) malloc(2 * sizeof(t_float *));
        x->outs = (t_float **) malloc(2 * sizeof(t_float *));
        x->ins[0] = (t_float *) malloc(8192 * sizeof(t_float));
    }
    else if (argc == 1){
        x->x_length = 3;
        t_float argument = (double)atom_getfloatarg(0, 1, argv);
        x->x_match_let = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
            pd_float((t_pd *)x->x_match_let, argument);
        outlet_new(&x->x_obj, gensym("signal"));
        outlet_new(&x->x_obj, gensym("signal"));
        x->x_2nd_let = 1;
        x->ins = (t_float **) malloc(2 * sizeof(t_float *));
        x->outs = (t_float **) malloc(2 * sizeof(t_float *));
        x->ins[0] = (t_float *) malloc(8192 * sizeof(t_float));
    }
    else{
        int i;
        x->x_length = (t_int)argc + 1; // length is # outs
        for(i=0; i < x->x_length ; i++)
            outlet_new(&x->x_obj, gensym("signal"));
        x->matches = (double *) malloc((x->x_length - 1) * sizeof(double));
        for(i = 0; i < argc; i++)
            x->matches[i] = (double)atom_getfloatarg(i,argc,argv);
        x->ins = (t_float **) malloc(1 * sizeof(t_float *));
        x->outs = (t_float **) malloc(x->x_length * sizeof(t_float *));
        x->ins[0] = (t_float *) malloc(8192 * sizeof(t_float));
    }
    return x;
}

void match_tilde_setup(void){
    match_class = class_new(gensym("match~"), (t_newmethod)match_new,
                            (t_method)match_free, sizeof(t_match),0,A_GIMME,0);
    CLASS_MAINSIGNALIN(match_class, t_match, x_f);
    class_addmethod(match_class, (t_method)match_dsp, gensym("dsp"), A_CANT, 0);
}
