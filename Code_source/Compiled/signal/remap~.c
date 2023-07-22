// porres

#include "m_pd.h"

static t_class *remap_class;

typedef struct _remap{
    t_object x_obj;
    int      x_n;
    t_atom  *x_vec;
}t_remap;

static void remap_list(t_remap *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(!ac)
        return;
    x->x_n = ac;
    x->x_vec = (t_atom *)getbytes(ac * sizeof(*x->x_vec));
    for(int i = 0; i < ac; i++)
        x->x_vec[i].a_w.w_float = atom_getfloat(av+i);
    canvas_update_dsp();
}
    
static void remap_dsp(t_remap *x, t_signal **sp){
    signal_setmultiout(&sp[1], x->x_n);
    int length = sp[0]->s_length;
    for(int i = 0; i < x->x_n; i++){
        int ch = x->x_vec[i].a_w.w_float;
        if(ch > sp[0]->s_nchans)
            ch = sp[0]->s_nchans;
        ch--;
        post("----------------");
        post("ch(%d)*length(%d) = %d", ch, length, ch*length);
        post("i(%d)*length(%d) = %d", i, length, i*length);
        if(ch >= 0)
            dsp_add_copy(sp[0]->s_vec + ch*length, sp[1]->s_vec + i*length, length);
        else
            dsp_add_zero(sp[1]->s_vec + i*length, length);
    }
    post("");
}

static void *remap_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_remap *x = (t_remap *)pd_new(remap_class);
    x->x_n = ac;
    x->x_vec = (t_atom *)getbytes(ac * sizeof(*x->x_vec));
    for(int i = 0; i < ac; i++)
        x->x_vec[i].a_w.w_float = atom_getfloat(av+i);
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void remap_tilde_setup(void){
    remap_class = class_new(gensym("remap~"), (t_newmethod)remap_new,
        0, sizeof(t_remap), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addlist(remap_class, remap_list);
    class_addmethod(remap_class, nullfn, gensym("signal"), 0);
    class_addmethod(remap_class, (t_method)remap_dsp, gensym("dsp"), 0);
}
