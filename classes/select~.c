#include "m_pd.h"
#include <math.h>

#define MAX_CHANS (256)
#define LINEAR (0)
#define EQ_POW (1)
#define HALF_PI M_PI  * 0.5

static t_class *select_class;

typedef struct _select
{
	t_object x_obj;
	float x_f;
	short input_chans;
	short active_chan;
	short last_chan;
	int samps_to_fade;
	int fadesamps;
	float fadetime;
	short fadetype;
	short *connected_list;
	float **bulk ; // array to point to all input audio channels
	float sr;
	float vs;
	int inlet_count;
} t_select;

void *select_new(t_symbol *s, int argc, t_atom *argv);

t_int *select_perform(t_int *w);
void select_dsp(t_select *x, t_signal **sp);
void select_float(t_select *x, t_float f);
void select_fadetime(t_select *x, t_floatarg f);
void select_int(t_select *x, t_int i);
void select_channel(t_select *x, t_floatarg i);
void select_free(t_select *x);

void select_fadetime(t_select *x, t_floatarg f)
{
	float sec = (float)f / 1000.0;
	if(sec < 0)
        sec = 0;
	x->fadetime = sec;
	x->fadesamps = x->sr * x->fadetime;
	x->samps_to_fade = 0;
}

t_int *select_perform(t_int *w)
{
	t_select *x = (t_select *) (w[1]);
	int i;
	int n;
	t_float *out;
	int fadesamps = x->fadesamps;
	short active_chan = x->active_chan;
	short last_chan = x->last_chan;
	int samps_to_fade = x->samps_to_fade;
	float m1, m2;
	float **bulk = x->bulk;
	short fadetype = x->fadetype;
	float phase;
	int inlet_count = x->inlet_count;
	for (i = 0; i < inlet_count; i++) {
		bulk[i] = (t_float *)(w[2 + i]);
        }
	out = (t_float *)(w[inlet_count + 2]);
	n = (int) w[inlet_count + 3];
	/********************************************/
    while(n--)
        {
        if (samps_to_fade >= 0)
            {
            if( fadetype == EQ_POW )
                {
                phase = HALF_PI * (1.0 - (samps_to_fade / (float) fadesamps));
                m1 = sin( phase );
                m2 = cos( phase );
                --samps_to_fade;
                *out++ = (*(bulk[active_chan])++ * m1) + (*(bulk[last_chan])++ * m2);
                }
            // Else Linear...
            }
        else {
            *out++ =  *(bulk[active_chan])++;
            }
        }
	x->samps_to_fade = samps_to_fade;
	return (w + (inlet_count + 4));
}

void select_dsp(t_select *x, t_signal **sp)
{
	long i;
	t_int **sigvec;
	int pointer_count;
	
	pointer_count = x->inlet_count + 3; // all inlets, 1 outlet, object pointer and vec-samps
	sigvec  = (t_int **) calloc(pointer_count, sizeof(t_int *));
	for(i = 0; i < pointer_count; i++){
		sigvec[i] = (t_int *) calloc(sizeof(t_int), 1);
        }
	sigvec[0] = (t_int *)x; // first pointer is to the object
	
	sigvec[pointer_count - 1] = (t_int *)sp[0]->s_n; // last pointer is to vector size (N)
    
	for(i = 1; i < pointer_count - 1; i++){ // now attach the inlet and all outlets
		sigvec[i] = (t_int *)sp[i-1]->s_vec;
        }
	if(x->sr != sp[0]->s_sr){
		x->sr = sp[0]->s_sr;
		x->fadesamps = x->fadetime * x->sr;
		x->samps_to_fade = 0;
	}
	dsp_addv(select_perform, pointer_count, (t_int *) sigvec);
	free(sigvec);
    for (i = 0; i < MAX_CHANS; i++) {
        x->connected_list[i] = 1;
    }
}

void select_channel(t_select *x, t_floatarg i) // Look at int at inlets
{
	int chan = i;
	if(chan < 0)
        chan = 0;
    if(chan > x->inlet_count - 1)
        chan = x->inlet_count - 1; // need not to be -1, cause 0 is now OFF!!! Porres
	if(chan != x->active_chan) {
		x->last_chan = x->active_chan;
		x->active_chan = chan;
		x->samps_to_fade = x->fadesamps;
		if( x->active_chan < 0)
			x->active_chan = 0;
		if( x->active_chan > MAX_CHANS - 1)
			x->active_chan = MAX_CHANS - 1;
	}	
}

void select_free(t_select *x)
{
    t_freebytes(x->bulk, 16 * sizeof(t_float *)); // 16 ???
}

void *select_new(t_symbol *s, int argc, t_atom *argv)
{
    int i;
    t_select *x;
    x = (t_select *)pd_new(select_class);
    x->fadetime = 0;
    x->inlet_count = 1;
    if(argc >= 1){
        x->inlet_count = (int)atom_getfloatarg(0,argc,argv);
        if(x->inlet_count < 1)
            x->inlet_count = 1;
        if(x->inlet_count > MAX_CHANS)
            x->inlet_count = MAX_CHANS;
        }
    if(argc >= 2){
        x->fadetime = atom_getfloatarg(1,argc,argv) / 1000.0;
    }
    
    for(i=0; i < x->inlet_count - 1; i++){
        inlet_new(&x->x_obj, &x->x_obj.ob_pd,gensym("signal"), gensym("signal"));
        }
    outlet_new(&x->x_obj, gensym("signal"));
    x->sr = sys_getsr();
    x->fadetype = EQ_POW;
    if(x->fadetime < 0.0)
        x->fadetime = 0;
    x->fadesamps = x->fadetime * x->sr;
    x->connected_list = (short *) t_getbytes(MAX_CHANS * sizeof(short));
    for(i=0; i < 16; i++){ // 16 ???
        x->connected_list[i] = 0;
        }
    x->active_chan = x->last_chan = 0;
    x->bulk = (t_float **) t_getbytes(16 * sizeof(t_float *)); // 16 ???
    x->samps_to_fade = 0;
    return (x);
}

void select_tilde_setup(void) {
    select_class = class_new(gensym("select~"), (t_newmethod)select_new,
            (t_method)select_free,sizeof(t_select), CLASS_DEFAULT, A_GIMME, 0);
    CLASS_MAINSIGNALIN(select_class, t_select, x_f);
    class_addmethod(select_class,(t_method)select_dsp,gensym("dsp"),0);
    class_addmethod(select_class,(t_method)select_fadetime,gensym("fadetime"),A_FLOAT,0);
    class_addmethod(select_class,(t_method)select_channel,gensym("channel"),A_FLOAT,0);
}
