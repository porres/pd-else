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
    }
    for(int i = 0; i < ac; i++)
        x->x_vec[i] = atom_getfloat(av+i);
    canvas_update_dsp();
}

static void get_dsp(t_get *x, t_signal **sp){
    signal_setmultiout(&sp[1], x->x_n);
    int n = sp[0]->s_n, chs = sp[0]->s_nchans;
    dsp_add_copy(sp[0]->s_vec, x->x_input, n*chs);
    for(int i = 0; i < x->x_n; i++){
        int chan = x->x_vec[i] - 1;
        if(chan >= chs)
            chan = chs - 1;
        if(chan >= 0)
            dsp_add_copy(sp[0]->s_vec + chan*n, sp[1]->s_vec + i*n, n);
        else
            dsp_add_zero(sp[1]->s_vec + i*n, n);
    }
}

void get_free(t_get *x){
    freebytes(x->x_input, x->x_block*x->x_chs * sizeof(*x->x_input));
    freebytes(x->x_vec, x->x_n * sizeof(*x->x_vec));
}

static void *get_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_get *x = (t_get *)pd_new(get_class);
    x->x_block = x->x_chs = 0;
    x->x_input = (t_float *)getbytes(sizeof(*x->x_input));
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
