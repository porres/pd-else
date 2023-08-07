// porres

#include "m_pd.h"
#include <math.h>

static t_class *xselectmc_class;

#define MAXIN 512
#define HALF_PI (3.14159265358979323846 * 0.5)

typedef struct _xselectmc{
    t_object  x_obj;
    float     x_sr_khz;
    float     x_fade; // fade in ms
    int       x_active_ch[MAXIN];
    int       x_count[MAXIN];
    int       x_ch;
    int       x_last_ch;
    int       x_n;
}t_xselectmc;

static void xselectmc_float(t_xselectmc *x, t_floatarg f){
    x->x_ch = f < 0 ? 0 : (int)f;
    if(x->x_ch != x->x_last_ch){
        if(x->x_ch)
            x->x_active_ch[x->x_ch*x->x_n - x->x_n] = 1;
        if(x->x_last_ch)
            x->x_active_ch[x->x_last_ch*x->x_n - x->x_n] = 0;
        x->x_last_ch = x->x_ch;
    }
}

static void xselectmc_time(t_xselectmc *x, t_floatarg f){
    x->x_fade = f;
}

static t_int *xselectmc_perform(t_int *w){
    t_xselectmc *x = (t_xselectmc *)(w[1]);
    t_int nblock = (t_int)(w[2]);
    t_int chs = (t_int)(w[3]);
    t_sample *in = (t_sample *)(w[4]);
    t_sample *out = (t_sample *)(w[5]);
    float fade = x->x_fade * x->x_sr_khz;
    if(fade <= 0)
        fade = 1;
    for(int i = 0; i < nblock; i++){
        for(int n = 0; n < x->x_n; n++){
            float sum = 0;
            for(int j = 0; j < chs; j++){
                if(x->x_active_ch[j] && x->x_count[j] < fade)
                    x->x_count[j]++;
                else if(!x->x_active_ch[j] && x->x_count[j] > 0)
                    x->x_count[j]--;
                if(x->x_count[j]){
                    float fadeval;
                    if(x->x_count[j] < fade)
                        fadeval = sin(((float)x->x_count[j] / fade) * HALF_PI);
                    else
                        fadeval = 1;
                    sum += in[j*nblock + n*nblock + i] * fadeval;
                }
            }
            out[n*nblock+i] = sum;
        }
    }
    return(w+6);
}

static void xselectmc_dsp(t_xselectmc *x, t_signal **sp){
    signal_setmultiout(&sp[1], x->x_n);
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    dsp_add(xselectmc_perform, 5, x, (t_int)sp[0]->s_n, (t_int)sp[0]->s_nchans, sp[0]->s_vec, sp[1]->s_vec);
}

static void *xselectmc_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_xselectmc *x = (t_xselectmc *)pd_new(xselectmc_class);
    x->x_ch = x->x_last_ch = 0;
    x->x_sr_khz = sys_getsr() * 0.001;
    int ch = 0;
    float ms = 0;
    x->x_n = 1;
    if(ac >= 2 && av->a_type == A_SYMBOL){
        if(atom_getsymbol(av) == gensym("-n")){
            ac--, av++;
            x->x_n = atom_getint(av);
            if(x->x_n < 1)
                x->x_n = 1;
            ac--, av++;
        }
    }
    if(ac){
        ms = atom_getfloat(av);
        ac--, av++;
    }
    if(ac)
        ch = atom_getfloat(av);
    xselectmc_time(x, ms);
    xselectmc_float(x, ch);
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void setup_xselect0x2emc_tilde(void){
    xselectmc_class = class_new(gensym("xselect.mc~"), (t_newmethod)xselectmc_new,
        0, sizeof(t_xselectmc), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addfloat(xselectmc_class, xselectmc_float);
    class_addmethod(xselectmc_class, nullfn, gensym("signal"), 0);
    class_addmethod(xselectmc_class, (t_method)xselectmc_dsp, gensym("dsp"), 0);
    class_addmethod(xselectmc_class, (t_method)xselectmc_time, gensym("time"), A_FLOAT, 0);
}
