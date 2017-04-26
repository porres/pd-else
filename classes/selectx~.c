
#include "m_pd.h"
//#include <math.h>

#define MAXINTPUT 500 //maximum number of channel inlets

typedef struct _selectx
{
    t_object   x_obj;
    t_float *x_ch_select; // main signal (channel selector)
    int 	  x_n_inlets; // inlets excluding main signal
    t_float   x_channel; // 0 = closed, nonzero = index of inlet to pass (1 indexed)
    t_float  **x_ivecs; // copying from matrix
    t_float  *x_ovec; // single pointer since we're dealing with an array rather than an array of arrays
} t_selectx;

static t_class *selectx_class;

static t_int *selectx_perform(t_int *w)
{
    t_selectx *x = (t_selectx *)(w[1]);
    int nblock = (int)(w[2]);
    t_float *channel = x->x_ch_select;
    t_float **ivecs = x->x_ivecs;
    t_float *ovec = x->x_ovec;
    t_float output;
    int i,j;
    int n_inlets = x->x_n_inlets;
    for(i=0; i < nblock; i++){
        int ch = (int)channel[i];
        float fade = channel[i] - (int)channel[i];
        if (channel[i] < 0)
            output = 0;
        else if (ch == 0)
            output = ivecs[0][i] * fade;
        else if (ch == n_inlets)
            output = ivecs[n_inlets - 1][i] * (1 - fade);
        else if (channel[i] >= n_inlets + 1)
            output = 0;
        else {
            for(j = 0; j < n_inlets; j++){
                if(ch == (j+1)){
                    output = ivecs[j][i] * (1 - fade) + ivecs[j+1][i] * fade;
                };
            };
        };
        ovec[i] = output;
    };
    return (w + 3);
}


static void selectx_dsp(t_selectx *x, t_signal **sp)
{
    int i, nblock = sp[0]->s_n;
    t_signal **sigp = sp;
    x->x_ch_select = (*sigp++)->s_vec; //the first sig in is the selectx idx
    for (i = 0; i < x->x_n_inlets; i++){ //now for the n_inlets
        *(x->x_ivecs+i) = (*sigp++)->s_vec;
    };
    x->x_ovec = (*sigp++)->s_vec; //now for the outlet
    dsp_add(selectx_perform, 2, x, nblock);
}

static void *selectx_new(t_symbol *s, int argc, t_atom *argv)
{
    t_selectx *x = (t_selectx *)pd_new(selectx_class);
    t_float n_inlets = 1; //inlets not counting selectx input
    t_float channel = 0; //start off closed initially
    int i;
    int argnum = 0;
    while(argc > 0){
        if(argv -> a_type == A_FLOAT){
            t_float argval = atom_getfloatarg(0, argc, argv);
            switch(argnum){
                case 0:
                    n_inlets = argval;
                    break;
                case 1:
                    channel = argval;
                    break;
                default:
                    break;
            };
            argc--;
            argv++;
            argnum++;
        };
    };
    
    //bounds checking
    if(n_inlets < 1){
        n_inlets = 1;
    }
    else if(n_inlets > (t_float)MAXINTPUT){
        n_inlets = MAXINTPUT;
    };
    if(channel < 0){
        channel = 0;
    }
    else if(channel > n_inlets){
        channel = n_inlets;
    };
    
    x->x_n_inlets = (int)n_inlets;
    x->x_channel = channel;
    x->x_ivecs = getbytes(n_inlets * sizeof(*x->x_ivecs));
    
    for (i = 0; i < n_inlets; i++){
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    };
    outlet_new((t_object *)x, &s_signal);
    
    return (x);
}

void * selectx_free(t_selectx *x){
    freebytes(x->x_ivecs, x->x_n_inlets * sizeof(*x->x_ivecs));
    return (void *) x;
}

void selectx_tilde_setup(void)
{
    selectx_class = class_new(gensym("selectx~"), (t_newmethod)selectx_new,
                    (t_method)selectx_free, sizeof(t_selectx), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(selectx_class, (t_method)selectx_dsp, gensym("dsp"), A_CANT, 0);
    CLASS_MAINSIGNALIN(selectx_class, t_selectx, x_channel);
}
