// porres

#include "m_pd.h"
#include <stdlib.h>

static t_class *get_class;

typedef struct _get{
    t_object x_obj;
    int      x_n;
    t_float *x_vec;
    t_float *x_input;
    int      x_block;
    int      x_chs;
}t_get;

static void get_list(t_get *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(!ac)
        return;
    if(x->x_n != ac){
        x->x_vec = (t_float *)resizebytes(x->x_vec,
            x->x_n * sizeof(t_float), ac * sizeof(t_float));
        x->x_n = ac;
        for(int i = 0; i < ac; i++)
            x->x_vec[i] = atom_getfloat(av+i);
        canvas_update_dsp();
    }
    else for(int i = 0; i < ac; i++)
        x->x_vec[i] = atom_getfloat(av+i);
}
    
static t_int *get_perform(t_int *w){
    int i;
    t_get *x = (t_get *)(w[1]);
    t_int n = (t_int)(w[2]);
    t_int nchans = (t_int)(w[3]);
    t_sample *input = (t_sample *)(w[4]);
    t_sample *out = (t_sample *)(w[5]);
    t_float *in = x->x_input;
    for(i = 0; i < n*nchans; i++)
        in[i] = input[i];
    for(i = 0; i < x->x_n; i++){ // channels to copy
        int ch = x->x_vec[i] - 1; // get channel number
        if(ch >= nchans)
            ch = nchans - 1;
        if(ch >= 0){
            for(int j = 0; j < n; j++)
                *out++ = in[ch*n + j];
        }
        else for(int j = 0; j < n; j++)
            *out++ = 0;
    }
    return(w+6);
}

static void get_dsp(t_get *x, t_signal **sp){
    signal_setmultiout(&sp[1], x->x_n);
    int n = sp[0]->s_n, chs = sp[0]->s_nchans;
    if(x->x_block != n || x->x_chs != chs){
        x->x_input = (t_float *)resizebytes(x->x_input,
            x->x_block*x->x_chs * sizeof(t_float), n*chs * sizeof(t_float));
        x->x_block = n, x->x_chs = chs;
    }
    dsp_add(get_perform, 5, x, n, chs, sp[0]->s_vec, sp[1]->s_vec);
}

void get_free(t_get *x){
    freebytes(x->x_input, x->x_block*x->x_chs * sizeof(*x->x_input));
    freebytes(x->x_vec, x->x_n * sizeof(*x->x_vec));
}

static void *get_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_get *x = (t_get *)pd_new(get_class);
    x->x_block = x->x_chs = 0;
    x->x_input = (t_float *)getbytes(0);
    if(!ac){
        x->x_vec = (t_float *)getbytes(sizeof(*x->x_vec));
        x->x_vec[0] = 0;
        x->x_n = 1;
    }
    else{
        x->x_n = ac;
        x->x_vec = (t_float *)getbytes(ac * sizeof(*x->x_vec));
        for(int i = 0; i < ac; i++)
            x->x_vec[i] = atom_getfloat(av+i);
    }
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void get_tilde_setup(void){
    get_class = class_new(gensym("get~"), (t_newmethod)get_new,
        (t_method)get_free, sizeof(t_get), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addlist(get_class, get_list);
    class_addmethod(get_class, nullfn, gensym("signal"), 0);
    class_addmethod(get_class, (t_method)get_dsp, gensym("dsp"), 0);
}
