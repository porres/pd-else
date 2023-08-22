// porres 2021

#include "m_pd.h"

typedef struct _slew2{
    t_object  x_obj;
    t_float  *x_last;
    t_float   x_in;
    t_float   x_sr_rec;
    t_inlet  *x_downlet;
    t_inlet  *x_uplet;
    int       x_nchans;
    float    x_f;
}t_slew2;

static t_class *slew2_class;

static t_int *slew2_perform(t_int *w){
    t_slew2 *x = (t_slew2 *)(w[1]);
    int n = (int)(w[2]);
    int ch2 = (int)(w[3]);
    int ch3 = (int)(w[4]);
    t_float *in1 = (t_float *)(w[5]);
    t_float *in2 = (t_float *)(w[6]);
    t_float *in3 = (t_float *)(w[7]);
    t_float *out = (t_float *)(w[8]);
    t_float *last = x->x_last;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            float f = in1[j*n + i];
            float up = (ch2 == 1 ? in2[i] : in2[j*n + i]) * x->x_sr_rec;
            float down = (ch3 == 1 ? in3[i] : in3[j*n + i]) * -x->x_sr_rec;
            float delta = f - last[j];
            if(delta > 0){ // up
                if(up >= 0 && delta > up)
                    f = last[j] + up;
            }
            else{ // down
                if(down <= 0 && delta < down)
                    f = last[j] + down;
            }
            out[j*n + i] = last[j] = f;
        }
    }
    x->x_last = last;
    return(w+9);
}

static void slew2_dsp(t_slew2 *x, t_signal **sp){
    x->x_sr_rec = 1 / sp[0]->s_sr;
    int chs = sp[0]->s_nchans, ch2 = sp[1]->s_nchans, ch3 = sp[2]->s_nchans, n = sp[0]->s_n;
    signal_setmultiout(&sp[3], chs);
    if(x->x_nchans != chs){
        x->x_last = (t_float *)resizebytes(x->x_last,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchans = chs;
    }
    if((ch2 > 1 && ch2 != x->x_nchans) || (ch3 > 1 && ch3 != x->x_nchans)){
        dsp_add_zero(sp[3]->s_vec, chs*n);
        pd_error(x, "[slew2~]: channel sizes mismatch");
    }
    else
        dsp_add(slew2_perform, 8, x, n, ch2, ch3,
            sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec, sp[3]->s_vec);
}

static void slew2_set(t_slew2 *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    for(int i = 0; i < ac; i++)
        x->x_last[i] = atom_getfloat(av++);
}

static void *slew2_free(t_slew2 *x){
    inlet_free(x->x_downlet);
    inlet_free(x->x_uplet);
    freebytes(x->x_last, x->x_nchans * sizeof(*x->x_last));
    return(void *)x;
}

static void *slew2_new(t_symbol *s, t_floatarg f1, t_floatarg f2){
    s = NULL;
    t_slew2 *x = (t_slew2 *)pd_new(slew2_class);
    x->x_last = (t_float *)getbytes(sizeof(*x->x_last));
    t_float up = f1;
    t_float down = f2;
    x->x_uplet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_uplet, up);
    x->x_downlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_downlet, down);
    outlet_new((t_object *)x, &s_signal);
    x->x_last[0] = 0;
    return(x);
}

void slew2_tilde_setup(void){
    slew2_class = class_new(gensym("slew2~"), (t_newmethod)slew2_new,
        (t_method)slew2_free, sizeof(t_slew2), CLASS_MULTICHANNEL, A_DEFFLOAT, A_DEFFLOAT, 0);
    CLASS_MAINSIGNALIN(slew2_class, t_slew2, x_f);
    class_addmethod(slew2_class, (t_method)slew2_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(slew2_class, (t_method)slew2_set, gensym("set"), A_GIMME, 0);
}
