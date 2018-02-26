#include "m_pd.h"

#define MAX_N_CH 512   // maximum number of channels

typedef struct _multigate{
    t_object   x_obj;
    t_float   *x_mainsig;  // the main inlet (leftmost) signal input
    int        x_n_ch;     // number of channels
    t_float    x_gate;     // 0 = off, nonzero = on
    t_float  **x_ivecs;    // input vectors
    t_float  **x_ovecs;    // output vectors
}t_multigate;

static t_class *multigate_class;

static t_int *multigate_perform(t_int *w){
    t_multigate *x = (t_multigate *)(w[1]);
    int nblock = (int)(w[2]);
    t_float *gate = x->x_mainsig;
    t_float **ivecs = x->x_ivecs;  // input vectors
    t_float **ovecs = x->x_ovecs;  // output vectors
    int i, j;
    int n_ch = x->x_n_ch;
    for(i = 0; i < nblock; i++){
        int state = gate[i] != 0;
        for(j = 0; j < n_ch; j++){ // signals counted unintuitively backwards/clockwise
            int ch_in = j;
            int ch_out = n_ch-j-1;
            ovecs[ch_out][i] = ivecs[ch_in][i] * state;
        };
    };
    return(w + 3);
}

static void multigate_dsp(t_multigate *x, t_signal **sp){
    int i, nblock = sp[0]->s_n;
    t_signal **sigp = sp;
	x->x_mainsig = (*sigp++)->s_vec;        // gate
    for(i = 0; i < x->x_n_ch; i++)              // inputs
        *(x->x_ivecs+i) = (*sigp++)->s_vec;
    for(i = 0; i < x->x_n_ch; i++)              // outputs
	*(x->x_ovecs+i) = (*sigp++)->s_vec;
    dsp_add(multigate_perform, 2, x, nblock);
}

static void *multigate_new(t_symbol *s, int argc, t_atom *argv){
    t_multigate *x = (t_multigate *)pd_new(multigate_class);
    t_float n_ch = 1; // default is 1 channel
    t_float gate = 0; // initially closed
    int i;
    int argnum = 0;
    while(argc > 0){
        if(argv -> a_type == A_FLOAT){
            t_float argval = atom_getfloatarg(0, argc, argv);
            switch(argnum){
                case 0:
		    n_ch = argval;
                    break;
		case 1:
		    gate = argval;
		    break;
                default:
                    break;
            };
            argc--;
            argv++;
            argnum++;
        };
    };
    if(n_ch < 1)
	n_ch = 1;
    else if(n_ch > (t_float)MAX_N_CH)
        n_ch = MAX_N_CH;
    x->x_n_ch = (int)n_ch;
    x->x_gate = gate != 0;
    x->x_ivecs = getbytes(n_ch * sizeof(*x->x_ivecs));
    for(i = 0; i < n_ch; i++)
        inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    x->x_ovecs = getbytes(n_ch * sizeof(*x->x_ovecs));
    for(i = 0; i < n_ch; i++)
	outlet_new(&x->x_obj, gensym("signal"));
    return(x);
}

void multigate_tilde_setup(void){
    t_class* c = class_new(gensym("multigate~"), (t_newmethod)multigate_new, (t_method)multigate_free,
            sizeof(t_multigate), CLASS_DEFAULT, A_FLOAT, A_DEFFLOAT, 0);
    if(c)
    {
        class_addmethod(c, (t_method)multigate_dsp, gensym("dsp"), A_CANT, 0);
        CLASS_MAINSIGNALIN(c, t_multigate, x_gate);
        multigate_class = c;
    }
}
