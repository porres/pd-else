// Porres

#include "m_pd.h"
#include "math.h"
#include <stdlib.h>

#define MAXLEN 1024

static t_class *phaseseq_class;

typedef struct _phaseseq{
    t_object    x_obj;
    float      *x_seq;
    float       x_lastphase;
    int         x_length;
    int         x_index;
    t_float     x_dummy;
    t_outlet   *x_out;
    t_outlet   *x_out_i;
}t_phaseseq;

static void phaseseq_set(t_phaseseq *x, t_symbol *s, int ac, t_atom * av){
    s = NULL;
    x->x_index = 0;
    x->x_length = ac;
    for(int i = 0; i < ac; i++)
        x->x_seq[i] = atom_getfloatarg(i, ac, av);
}

static t_int *phaseseq_perform(t_int *w){
    t_phaseseq *x = (t_phaseseq *)(w[1]);
    int nblock = (t_int)(w[2]);
    t_float *in = (t_float *)(w[3]);
    t_float *out1 = (t_float *)(w[4]);
    t_float *out2 = (t_float *)(w[5]);
    float lastphase = x->x_lastphase;
    while(nblock--){
        double phase = *in++;
        double delta = (phase - lastphase);
        int dir = (phase - lastphase) > 0;
        int transition = fabs(delta) > 0.5;
        if(x->x_length){
            int trig = 0;
            if(transition){
                if(x->x_seq[x->x_index] == 0)
                    trig = 1;
            }
            else if(phase == x->x_seq[x->x_index])
                trig = 1;
            else{
                if(dir && phase > x->x_seq[x->x_index] && lastphase < x->x_seq[x->x_index])
                    trig = 1;
                else if(!dir && phase < x->x_seq[x->x_index] && lastphase > x->x_seq[x->x_index])
                    trig = 1;
            }
            if(trig){
                *out1++ = 1.;
                x->x_index++;
                if(x->x_index == x->x_length)
                    x->x_index = 0;
            }
            else
                *out1++ = 0.;
            *out2++ = x->x_index;
        }
        else{
            *out1++ = 0.;
            *out2++ = 0.;
        }
        lastphase = phase;
    }
    x->x_lastphase = lastphase;
    return(w+6);
}

static void phaseseq_dsp(t_phaseseq *x, t_signal **sp){
    dsp_add(phaseseq_perform, 5, x, sp[0]->s_n, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void *phaseseq_free(t_phaseseq *x){
    outlet_free(x->x_out);
    outlet_free(x->x_out_i);
    return(void *)x;
}

static void *phaseseq_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_phaseseq *x = (t_phaseseq *)pd_new(phaseseq_class);
    x->x_seq = (float *) malloc(MAXLEN * sizeof(float));
    x->x_length = ac;
    for(int i = 0; i < ac; i++)
        x->x_seq[i] = atom_getfloatarg(i, ac, av);
    x->x_out = outlet_new(&x->x_obj, &s_signal);
    x->x_out_i = outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void phaseseq_tilde_setup(void){
    phaseseq_class = class_new(gensym("phaseseq~"), (t_newmethod)phaseseq_new,
        (t_method)phaseseq_free, sizeof(t_phaseseq), CLASS_DEFAULT, A_GIMME, 0);
    CLASS_MAINSIGNALIN(phaseseq_class, t_phaseseq, x_dummy);
    class_addmethod(phaseseq_class, (t_method)phaseseq_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(phaseseq_class, (t_method)phaseseq_set, gensym("set"), A_GIMME, 0);
}
