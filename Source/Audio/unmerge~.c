// porres

#include "m_pd.h"

static t_class *unmerge_class;

typedef struct _unmerge{
    t_object x_obj;
    t_int    x_n;
    t_int    x_ch;
}t_unmerge;

static void unmerge_float(t_unmerge *x, t_floatarg f){
    x->x_ch = f < 1 ? 1 : (int)f;
    canvas_update_dsp();
}

static void unmerge_dsp(t_unmerge *x, t_signal **sp){
    int n = sp[0]->s_n, chs = sp[0]->s_nchans, ch = x->x_ch;
    for(int i = 0; i <= x->x_n; i++){
        if(chs < 1){
            signal_setmultiout(&sp[i+1], 1);
            dsp_add_zero(sp[i+1]->s_vec, n);
        }
        else{
            if(i == x->x_n){
                signal_setmultiout(&sp[i+1], chs);
                dsp_add_copy(sp[0]->s_vec + i*ch*n, sp[i+1]->s_vec, chs*n);
            }
            else{
                signal_setmultiout(&sp[i+1], ch);
                dsp_add_copy(sp[0]->s_vec + i*ch*n, sp[i+1]->s_vec, ch*n);
            }
        }
        chs-=ch;
    }
}

static void *unmerge_new(t_floatarg f1, t_floatarg f2){
    t_unmerge *x = (t_unmerge *)pd_new(unmerge_class);
    x->x_n = f1 < 2 ? 2 : (int)f1;
    x->x_ch = f2 < 1 ? 1 : (int)f2;
    for(int i = 0; i <= x->x_n; i++)
        outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void unmerge_tilde_setup(void){
    unmerge_class = class_new(gensym("unmerge~"), (t_newmethod)unmerge_new,
        0, sizeof(t_unmerge), CLASS_MULTICHANNEL, A_DEFFLOAT, A_DEFFLOAT, 0);
    class_addmethod(unmerge_class, nullfn, gensym("signal"), 0);
    class_addmethod(unmerge_class, (t_method)unmerge_dsp, gensym("dsp"), 0);
    class_addfloat(unmerge_class, unmerge_float);
}
