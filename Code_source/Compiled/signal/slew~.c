// porres 2021

#include "m_pd.h"

typedef struct _slew{
    t_object  x_obj;
    t_float  *x_lastin;
    t_float   x_sr_rec;
	t_inlet  *x_inlet;
    int       x_nchans;
}t_slew;

static t_class *slew_class;

static t_int *slew_perform(t_int *w){
    t_slew *x = (t_slew *)(w[1]);
    int n = (int)(w[2]);
    int ch2 = (int)(w[3]);
    t_float *in1 = (t_float *)(w[4]);
    t_float *in2 = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);
    t_float *lastin = x->x_lastin;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            float in = in1[j*n + i];
            float limit = ch2 == 1 ? in2[i] : in2[j*n + i];
            if(limit < 0)
                out[j*n + i] = lastin[j] = in;
            else{
                limit *= -x->x_sr_rec;
                float hi = -limit;
                float delta = in - lastin[j];
                if(delta < limit)
                    in = lastin[j] + limit;
                else if(delta > hi)
                    in = lastin[j] + hi;
                out[j*n + i] = lastin[j] = in;
            }
        }
    }
    x->x_lastin = lastin;
    return(w+7);
}

static void slew_dsp(t_slew *x, t_signal **sp){
    x->x_sr_rec = 1 / sp[0]->s_sr;
    int chs = sp[0]->s_nchans, ch2 = sp[1]->s_nchans, n = sp[0]->s_n;
    signal_setmultiout(&sp[2], chs);
    if(x->x_nchans != chs){
        x->x_lastin = (t_float *)resizebytes(x->x_lastin,
            x->x_nchans * sizeof(t_float), chs * sizeof(t_float));
        x->x_nchans = chs;
    }
    if(ch2 > 1 && ch2 != x->x_nchans){
        dsp_add_zero(sp[2]->s_vec, chs*n);
        pd_error(x, "[slew~]: channel sizes mismatch");
    }
    else
        dsp_add(slew_perform, 6, x, n, ch2, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

static void slew_set(t_slew *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    for(int i = 0; i < ac; i++)
        x->x_lastin[i] = atom_getfloat(av++);
}

static void *slew_free(t_slew *x){
    inlet_free(x->x_inlet);
    freebytes(x->x_lastin, x->x_nchans * sizeof(*x->x_lastin));
    return(void *)x;
}

static void *slew_new(t_symbol *s, t_floatarg f){
    s = NULL;
    t_slew *x = (t_slew *)pd_new(slew_class);
    x->x_lastin = (t_float *)getbytes(sizeof(*x->x_lastin));
	x->x_inlet = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet, f);
    outlet_new((t_object *)x, &s_signal);
    return(x);
}

void slew_tilde_setup(void){
    slew_class = class_new(gensym("slew~"), (t_newmethod)slew_new,
        (t_method)slew_free, sizeof(t_slew), CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
    class_addmethod(slew_class, nullfn, gensym("signal"), 0);
    class_addmethod(slew_class, (t_method)slew_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(slew_class, (t_method)slew_set, gensym("set"), A_GIMME, 0);
}
