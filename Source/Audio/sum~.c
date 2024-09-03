// porres

#include "m_pd.h"
#include <stdlib.h>

static t_class *sum_class;

typedef struct _sum{
    t_object x_obj;
    t_float *x_input;
    int      x_block;
    int      x_chs;
    int       x_sum;
}t_sum;
    
static void sum_sum(t_sum *x, t_floatarg f){
    x->x_sum = (f != 0.);
    canvas_update_dsp();
}

static t_int *sum_perform(t_int *w){
    t_sum *x = (t_sum *)(w[1]);
    t_sample *input = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    t_float *in = x->x_input;
    int n = x->x_block, nchans = x->x_chs, i;
    for(i = 0; i < n*nchans; i++){
        in[i] = input[i];
    }
    for(i = 0; i < n; i++)
        out[i] = 0;
    for(int j = 0; j < nchans; j++){ // channels to sum
        for(i = 0; i < n; i++)
            out[i] += in[j*n + i];
    }
    return(w+4);
}

static void sum_dsp(t_sum *x, t_signal **sp){
    int n = sp[0]->s_n, chs = sp[0]->s_nchans;
    if(x->x_block != n || x->x_chs != chs){
        x->x_input = (t_float *)resizebytes(x->x_input,
            x->x_block*x->x_chs * sizeof(t_float), n*chs * sizeof(t_float));
        x->x_block = n, x->x_chs = chs;
    }
    if(x->x_sum){
        signal_setmultiout(&sp[1], 1);
        dsp_add(sum_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
    }
    else{
        signal_setmultiout(&sp[1], x->x_chs);
        dsp_add_copy(sp[0]->s_vec, sp[1]->s_vec, x->x_chs*x->x_block);
    }
}

void sum_free(t_sum *x){
    freebytes(x->x_input, x->x_block*x->x_chs * sizeof(*x->x_input));
}

static void *sum_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_sum *x = (t_sum *)pd_new(sum_class);
    x->x_block = x->x_chs = 0;
    x->x_sum = 1;
    if(ac)
        x->x_sum = atom_getfloatarg(0, ac, av) != 0;
    x->x_input = (t_float *)getbytes(0);
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void sum_tilde_setup(void){
    sum_class = class_new(gensym("sum~"), (t_newmethod)sum_new,
        (t_method)sum_free, sizeof(t_sum), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(sum_class, nullfn, gensym("signal"), 0);
    class_addmethod(sum_class, (t_method)sum_dsp, gensym("dsp"), 0);
    class_addmethod(sum_class, (t_method)sum_sum, gensym("sum"), A_FLOAT, 0);
}
