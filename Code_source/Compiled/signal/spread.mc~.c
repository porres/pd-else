// porres 2023

#include "m_pd.h"
#include <math.h>
#include <stdlib.h>

static t_class *spreadmc_class;

#define HALF_PI (3.14159265358979323846 * 0.5)

typedef struct _spreadmc{
    t_object    x_obj;
    int         x_nchs;
    int         x_block;
    int         x_outchs;
    t_float    *x_input;
    t_float    *x_amp1;
    t_float    *x_amp2;
    t_int      *x_idx;
}t_spreadmc;

static void spreadmc_n(t_spreadmc *x, t_floatarg f){
    int n = f < 2 ? 2 : f > 512 ? 512 : (int)f;
    if(x->x_outchs != n){
        x->x_outchs = n;
        float chratio = (float)(x->x_outchs - 1)/(float)(x->x_nchs - 1);
        for(int i = 1; i < x->x_nchs - 1; i++){
            float frac = i * chratio;
            x->x_idx[i] = floor(frac);
            double pos = (double)(frac - x->x_idx[i]) * HALF_PI;
            x->x_amp1[i] = cos(pos);
            x->x_amp2[i] = sin(pos);
        }
        canvas_update_dsp();
    }
}

t_int *spreadmc_perform(t_int *w){
    t_spreadmc *x = (t_spreadmc*)w[1];
    t_float *input = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    t_float *in = x->x_input;
    int chs = x->x_nchs, n = x->x_block, outchs = x->x_outchs;
    for(int i = 0; i < n*chs; i++) // copy input
        in[i] = input[i];
    for(int i = 0; i < n; i++){
        for(int j = 1; j < outchs - 1; j++) // zero output channels
            out[j*n + i] = 0;
        out[i] = in[i]; // copy leftmost channel
        out[(outchs-1)*n + i] = in[(chs-1)*n + i]; // copy rightmost channel
        for(int j = 1; j < chs - 1; j++){ // spread the rest
            int idx = x->x_idx[j], ch1 = idx * n, ch2 = (idx+1) * n;
            out[ch1+i] += (x->x_amp1[j] * in[j*n + i]);
            out[ch2+i] += (x->x_amp2[j] * in[j*n + i]);;
        }
	}
    return(w+4);
}

void spreadmc_dsp(t_spreadmc *x, t_signal **sp){
    int n = sp[0]->s_n, chs = sp[0]->s_nchans;
    signal_setmultiout(&sp[1], x->x_outchs);
    if(x->x_block != n || x->x_nchs != chs){
        if(x->x_nchs != chs){
            x->x_idx = (t_int *)resizebytes(x->x_idx,
                x->x_nchs * sizeof(t_int), chs * sizeof(t_int));
            x->x_amp1 = (t_float *)resizebytes(x->x_amp1,
                x->x_nchs * sizeof(t_float), chs * sizeof(t_float));
            x->x_amp2 = (t_float *)resizebytes(x->x_amp2,
                x->x_nchs * sizeof(t_float), chs * sizeof(t_float));
            float chratio = (float)(x->x_outchs - 1)/(float)(chs - 1);
            for(int i = 1; i < chs - 1; i++){
                float frac = i * chratio;
                x->x_idx[i] = floor(frac);
                double pos = (double)(frac - x->x_idx[i]) * HALF_PI;
                x->x_amp1[i] = cos(pos);
                x->x_amp2[i] = sin(pos);
            }
        }
        x->x_input = (t_float *)resizebytes(x->x_input,
            x->x_block*x->x_nchs * sizeof(t_float), n*chs * sizeof(t_float));
        x->x_block = n, x->x_nchs = chs;
    }
    dsp_add(spreadmc_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
}

void spreadmc_free(t_spreadmc *x){
    freebytes(x->x_input, x->x_block*x->x_nchs * sizeof(*x->x_input));
    freebytes(x->x_amp1, x->x_nchs * sizeof(*x->x_amp1));
    freebytes(x->x_amp2, x->x_nchs * sizeof(*x->x_amp2));
    freebytes(x->x_idx, x->x_nchs * sizeof(*x->x_idx));
}

void *spreadmc_new(t_floatarg f){
    t_spreadmc *x = (t_spreadmc *)pd_new(spreadmc_class);
    x->x_outchs = f < 2 ? 2 : f > 512 ? 512 : (int)f;
    x->x_nchs = 1;
    x->x_block = sys_getblksize();
    x->x_input = (t_float *)getbytes(x->x_block*sizeof(*x->x_input));
    x->x_amp1 = (t_float *)getbytes(sizeof(*x->x_amp1));
    x->x_amp2 = (t_float *)getbytes(sizeof(*x->x_amp2));
    x->x_idx = (t_int *)getbytes(sizeof(*x->x_idx));
    outlet_new(&x->x_obj, gensym("signal"));
    return(x);
}

void setup_spread0x2emc_tilde(void){
    spreadmc_class = class_new(gensym("spread.mc~"), (t_newmethod)spreadmc_new,
        (t_method)spreadmc_free, sizeof(t_spreadmc), CLASS_MULTICHANNEL, A_DEFFLOAT, 0);
    class_addmethod(spreadmc_class, nullfn, gensym("signal"), 0);
    class_addmethod(spreadmc_class, (t_method)spreadmc_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(spreadmc_class, (t_method)spreadmc_n, gensym("n"), A_DEFFLOAT, 0);
}
