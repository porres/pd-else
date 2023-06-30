// Porres 2023

#include "m_pd.h"

static t_class *mix_class;

typedef struct _mix{
    t_object  x_obj;
    int       x_mix;
}t_mix;

static void mix_mix(t_mix *x, t_floatarg f){
    x->x_mix = (f != 0.);
    canvas_update_dsp();
}

static t_int * mix_perform(t_int *w){
    int n = (int)(w[1]);
    t_sample *in = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int nchs = (int)(w[4]);
    int i, j;
    for(i = 0; i < n; i++){
        for(j = 0; j < nchs-1; j++)
            out[i] += in[j*n + i];
    }
    return(w+5);
}

static t_int * nomix_perform(t_int *w){
    int n = (int)(w[1]);
    t_sample *in = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    int nchs = (int)(w[4]);
    int i, j;
    for(i = 0; i < n; i++){
        for(j = 0; j < nchs-1; j++)
            out[i] += in[j*n + i];
    }
    return(w+5);
}

static void mix_dsp(t_mix *x, t_signal **sp){
    if(x->x_mix){
        signal_setmultiout(&sp[1], 1);
        dsp_add(mix_perform, 4, (t_int)sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, (t_int)sp[0]->s_nchans);
    }
    else{
        signal_setmultiout(&sp[1], sp[0]->s_nchans);
        dsp_add(nomix_perform, 4, (t_int)sp[0]->s_n,
            sp[0]->s_vec, sp[1]->s_vec, (t_int)sp[0]->s_nchans);
    }
}

void *mix_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_mix *x = (t_mix *)pd_new(mix_class);
    x->x_mix = 1;
    if(ac)
        x->x_mix = atom_getfloatarg(0, ac, av) != 0;
    outlet_new(&x->x_obj, &s_signal);
    return(void *)x;
}

void mix_tilde_setup(void){
    mix_class = class_new(gensym("mix~"), (t_newmethod)mix_new, 0,
        sizeof(t_mix), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(mix_class, nullfn, gensym("signal"), 0);
    class_addmethod(mix_class, (t_method)mix_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(mix_class, (t_method)mix_mix, gensym("mix"), A_FLOAT, 0);
}
