// porres 2023

#include "m_pd.h"
#include "buffer.h"

static t_class *rotatemc_class;

typedef struct _rotatemc{
    t_object    x_obj;
    t_float    *x_input;
    t_inlet    *x_inlet_pos;
    int         x_n, x_chs;
}t_rotatemc;

static t_int *rotatemc_perform(t_int *w){
    t_rotatemc *x = (t_rotatemc*)w[1];
    t_float *input = (t_float *)(w[2]);
    t_float *position = (t_float *)(w[3]);
    t_float *out = (t_float *)(w[4]);
    int chs = x->x_chs, n = x->x_n;
    t_float *in = x->x_input;
    for(int i = 0; i < n*chs; i++) // copy input
        in[i] = input[i];
	for(int i = 0; i < n; i++){
        for(int j = 0; j < chs; j++)
            out[j*n + i] = 0;
        double idx = position[i] * (double)chs; // pos inlet
        if(idx <= -chs || idx >= chs)
            idx = 0;
        while(idx < 0)
            idx += chs;
        int offset = (int)floor(idx) % chs;
        double pos = (idx - offset) * 0.25;
        double amp1 = read_sintab(pos + 0.25), amp2 = read_sintab(pos);
        for(int j = 0; j < chs; j++){
            int ch1 = ((j+offset) % chs) * n, ch2 = ((j+offset+1) % chs) * n;
            out[ch1+i] += (amp1 * in[j*n + i]);
            out[ch2+i] += (amp2 * in[j*n + i]);
        }
	}
    return(w+5);
}

static void rotatemc_dsp(t_rotatemc *x, t_signal **sp){
    int n = sp[0]->s_n, chs = sp[0]->s_nchans;
    signal_setmultiout(&sp[2], chs);
    if(x->x_n != n || x->x_chs != chs){
        x->x_input = (t_float *)resizebytes(x->x_input,
            x->x_n*x->x_chs * sizeof(t_float), n*chs * sizeof(t_float));
        x->x_n = n, x->x_chs = chs;
    }
    dsp_add(rotatemc_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[2]->s_vec);
}

void rotatemc_free(t_rotatemc *x){
    freebytes(x->x_input, x->x_n*x->x_chs * sizeof(*x->x_input));
}

static void *rotatemc_new(t_floatarg f){
    t_rotatemc *x = (t_rotatemc *)pd_new(rotatemc_class);
    init_sine_table();
    x->x_n = x->x_chs = 0;
    x->x_input = (t_float *)getbytes(0);
    x->x_inlet_pos = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_inlet_pos, f);
    outlet_new(&x->x_obj, gensym("signal"));
    return(x);
}

void setup_rotate0x2emc_tilde(void){
    rotatemc_class = class_new(gensym("rotate.mc~"), (t_newmethod)rotatemc_new,
        0, sizeof(t_rotatemc), CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
    class_addmethod(rotatemc_class, nullfn, gensym("signal"), 0);
    class_addmethod(rotatemc_class, (t_method)rotatemc_dsp, gensym("dsp"), A_CANT, 0);
}
