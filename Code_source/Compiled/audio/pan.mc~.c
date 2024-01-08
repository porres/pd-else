// porres

#include "m_pd.h"
#include "buffer.h"

#define MAXOUTPUT 4096

typedef struct _panmc{
    t_object    x_obj;
    t_inlet    *x_inlet_spread;
    t_inlet    *x_inlet_gain;
    int         x_n;            // block size
    int         x_n_outlets;    // outlets
    t_float     x_offset;
}t_panmc;

static t_class *panmc_class;

static t_int *panmc_perform(t_int *w){
    t_panmc *x = (t_panmc *)(w[1]);
    t_float *input = (t_float *)(w[2]);
    t_float *gain = (t_float *)(w[3]);
    t_float *azimuth = (t_float *)(w[4]);
    t_float *spreadin = (t_float *)(w[5]);
    t_float *out = (t_float *)(w[6]);
    for(int i = 0; i < x->x_n; i++){
        t_float in = input[i];
        t_float g = gain[i];
        t_float pos = azimuth[i];
        t_float spread = spreadin[i];
        pos -= x->x_offset;
        while(pos < 0)
            pos += 1;
        while(pos >= 1)
            pos -= 1;
        if(spread < 0.1)
            spread = 0.1;
        pos = pos * x->x_n_outlets + spread;
        spread *= 2;
        float range = x->x_n_outlets / spread;
        for(int j = 0; j < x->x_n_outlets; j++){
            float chanpos = (pos - j) / spread;
            chanpos = chanpos - range * floor(chanpos/range);
            float chanamp = chanpos >= 1 ? 0 : read_sintab(chanpos*0.5);
            out[j*x->x_n+i] = (in * chanamp) * g;
        }
    };
    return(w+7);
}

void panmc_dsp(t_panmc *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    signal_setmultiout(&sp[4], x->x_n_outlets);
    if(sp[0]->s_nchans > 1 || sp[1]->s_nchans > 1
    || sp[2]->s_nchans > 1 || sp[2]->s_nchans > 1){
        dsp_add_zero(sp[4]->s_vec, x->x_n_outlets*x->x_n);
        pd_error(x, "[pan.mc~] input channels cannot be greater than 1");
        return;
    }
    dsp_add(panmc_perform, 6, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec, sp[4]->s_vec);
}

static void panmc_offset(t_panmc *x, t_floatarg f){
    x->x_offset = (f < 0 ? 0 : f) / 360;
}

void *panmc_free(t_panmc *x){
    inlet_free(x->x_inlet_spread);
    inlet_free(x->x_inlet_gain);
    return(void *)x;
}

static void *panmc_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_panmc *x = (t_panmc *)pd_new(panmc_class);
    init_sine_table();
    t_float n_outlets = 2;
    float spread = 1, gain = 1;
//    x->x_offset = 90. / 360.;
    x->x_offset = 0;
    if(atom_getsymbol(av) == gensym("-offset")){
        ac--, av++;
        x->x_offset = atom_getfloat(av) / 360.;
        ac--, av++;
    }
    if(ac){
        n_outlets = atom_getint(av);
        ac--, av++;
    }
    if(ac){
        spread = atom_getfloat(av);
        ac--, av++;
    }
    if(n_outlets < 2)
        n_outlets = 2;
    else if(n_outlets > (t_float)MAXOUTPUT)
        n_outlets = MAXOUTPUT;
    x->x_n_outlets = (int)n_outlets;
    x->x_inlet_gain = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_gain, gain);
    inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal); // azimuth
    x->x_inlet_spread = inlet_new(&x->x_obj, &x->x_obj.ob_pd,  &s_signal, &s_signal);
        pd_float((t_pd *)x->x_inlet_spread, spread);
    outlet_new((t_object *)x, &s_signal);
    return(x);
}

void setup_pan0x2emc_tilde(void){
    panmc_class = class_new(gensym("pan.mc~"), (t_newmethod)panmc_new,
        (t_method)panmc_free, sizeof(t_panmc), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(panmc_class, nullfn, gensym("signal"), 0);
    class_addmethod(panmc_class, (t_method)panmc_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(panmc_class, (t_method)panmc_offset, gensym("offset"), A_FLOAT, 0);
}
