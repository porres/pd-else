// porres

#include "m_pd.h"

static t_class *remap_class;

#define INPUTLIMIT 32768 // 512 * 64

typedef struct _remap{
    t_object x_obj;
    int      x_n;
    t_atom  *x_vec;
    float   *x_in[INPUTLIMIT];
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
    
static t_int *remap_perform(t_int *w){
    t_remap *x = (t_remap *)(w[1]);
    t_int n = (t_int)(w[2]);
    t_int nchans = (t_int)(w[3]);
    t_sample *in = (t_sample *)(w[4]);
    t_sample *out = (t_sample *)(w[5]);
    int i;
    for(i = 0; i < n*nchans; i++){
        x->x_in[i] = (&*in++); // copy input
//        float f = *in++;
//        post("f = %f", f);
    }
    for(i = 0; i < x->x_n; i++){ // channels to copy
        int ch = x->x_vec[i].a_w.w_float - 1; // get channel number
        if(ch >= nchans)
            ch = nchans - 1;
        for(int j = 0; j < n; j++){ // j is sample number of each channel
            if(ch >= 0)
                *out++ = *x->x_in[ch*n + j];
            else
                *out++ = 0;
        }
    }
    return(w+6);
}

static void remap_dsp(t_remap *x, t_signal **sp){
    signal_setmultiout(&sp[1], x->x_n);
    post("x->x_n = %d", x->x_n);
    post("length = %d", sp[0]->s_length);
    post("nchans = %d", sp[0]->s_nchans);
    dsp_add(remap_perform, 5, x, sp[0]->s_length, sp[0]->s_nchans,
        sp[0]->s_vec, sp[1]->s_vec);
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
