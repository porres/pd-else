// porres 2023

#include "m_pd.h"

#define MTXMC_MAX_INOUT  4096

typedef struct _mtxmc{
    t_object  x_obj;
    int       x_n;
    int       x_n_ins, x_n_outs, x_n_cells;
    int       x_ramp;                           // ramp size in samples
    float     x_ksr, x_ramp_ms;                 // sr in khz and ramp size in ms
    int      *x_nleft;                          // n samples left in ramp for each cell
    t_float  *x_tgain, *x_g, *x_inc;            // target gains, current gain and inc step
    t_float  *x_input;
}t_mtxmc;

static t_class *mtxmc_class;

static void mtxmc_set_gain(t_mtxmc *x, int i, float g){
    if(x->x_tgain[i] == g)
        return;
    x->x_tgain[i] = g;
    x->x_nleft[i] = x->x_ramp;
    x->x_inc[i] = (x->x_tgain[i] - x->x_g[i]) / (float)x->x_nleft[i];
}

static void mtxmc_list(t_mtxmc *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(ac != 3){
        if(ac == 1)
            pd_error(x, "[mtx.mc~]: no method for float");
        else
            pd_error(x, "[mtx.mc~]: list size must be '3'");
        return;
    }
    int inlet_idx = atom_getint(av++);
    if(inlet_idx < 0 || inlet_idx >= x->x_n_ins){  // bound check
        pd_error(x, "[mtx.mc~]: %d is not a valid inlet index!", inlet_idx);
        return;
    };
    int outlet_idx = atom_getint(av++);
    if(outlet_idx < 0 || outlet_idx >= x->x_n_outs){
        pd_error(x, "[mtx.mc~]: %d is not a valid outlet index!", outlet_idx);
        return;
    };
    int i = inlet_idx * x->x_n_outs + outlet_idx;
    mtxmc_set_gain(x, i, atom_getfloat(av++));
}

static void mtxmc_clear(t_mtxmc *x){
    for(int i = 0; i < x->x_n_cells; i++)
        mtxmc_set_gain(x, i, 0); // set gain to 0
}

static void mtxmc_outs(t_mtxmc *x, t_floatarg f){
    int n_outs = f < 1 ? 1 : f > MTXMC_MAX_INOUT ? MTXMC_MAX_INOUT : (int)f;
    if(n_outs != x->x_n_outs){
        int n_cells = x->x_n_ins * n_outs;
        x->x_tgain = (t_float *)resizebytes(x->x_tgain,
            x->x_n_cells*sizeof(t_float), n_cells*sizeof(t_float));
        x->x_g = (t_float *)resizebytes(x->x_g,
            x->x_n_cells*sizeof(t_float), n_cells*sizeof(t_float));
        x->x_inc = (t_float *)resizebytes(x->x_inc,
            x->x_n_cells*sizeof(t_float), n_cells*sizeof(t_float));
        x->x_nleft = (int *)resizebytes(x->x_nleft,
            x->x_n_cells*sizeof(t_float), n_cells*sizeof(int));
        x->x_n_outs = n_outs, x->x_n_cells = n_cells;
        canvas_update_dsp();
    }
}

static void mtxmc_ramp(t_mtxmc *x, t_floatarg f){
    x->x_ramp_ms = f < 0 ? 0 : f;
    x->x_ramp = (int)((x->x_ramp_ms * x->x_ksr) + 0.5);
}

static void mtxmc_print(t_mtxmc *x){
    post("-- [mtx.mc~] --:");
    for(int i = 0; i < x->x_n_ins; i++)
        for(int o = 0; o < x->x_n_outs; o++)
            post("%d %d %g", i, o, x->x_tgain[i*x->x_n_outs + o]);
}

static t_int *mtxmc_perform(t_int *w){
    t_mtxmc *x = (t_mtxmc *)(w[1]);
    t_float *insig = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    t_float *in = x->x_input;
    int i;
    for(i = 0; i < x->x_n * x->x_n_ins; i++) // copy input
        in[i] = insig[i];
    for(i = 0; i < x->x_n * x->x_n_outs; i++) // zero output
        out[i] = 0;
    for(i = 0; i < x->x_n_ins; i++){
        for(int o = 0; o < x->x_n_outs; o++){
            int idx = i * x->x_n_outs + o; // cell index
            for(int n = 0; n < x->x_n; n++){ // n sample in block
                float gain;
                if(x->x_nleft[idx] > 0){
                    gain = x->x_g[idx];
                    x->x_g[idx] += x->x_inc[idx];
                    x->x_nleft[idx]--;
                }
                else
                    gain = x->x_g[idx] = x->x_tgain[idx];
                out[o*x->x_n + n] += in[i*x->x_n + n] * gain;
            }
        }
    }
    return(w+4);
}

static void mtxmc_dsp(t_mtxmc *x, t_signal **sp){
    int n_ins = sp[0]->s_nchans, n = sp[0]->s_n;
    if(n_ins > MTXMC_MAX_INOUT){
        pd_error(x, "[mtx.mc~]: c'mon %d is enough channels, huh?", MTXMC_MAX_INOUT);
        return;
    }
    if(n != x->x_n || n_ins != x->x_n_ins){
        x->x_input = (t_float *)resizebytes(x->x_input,
            x->x_n*x->x_n_ins * sizeof(t_float), n*n_ins * sizeof(t_float));
        int n_cells = n_ins * x->x_n_outs;
        x->x_tgain = (t_float *)resizebytes(x->x_tgain,
            x->x_n_cells*sizeof(t_float), n_cells * sizeof(t_float));
        x->x_g = (t_float *)resizebytes(x->x_g,
            x->x_n_cells*sizeof(t_float), n_cells * sizeof(t_float));
        x->x_inc = (t_float *)resizebytes(x->x_inc,
            x->x_n_cells*sizeof(t_float), n_cells * sizeof(t_float));
        x->x_nleft = (int *)resizebytes(x->x_nleft,
            x->x_n_cells*sizeof(t_float), n_cells * sizeof(int));
        x->x_n = n, x->x_n_ins = n_ins, x->x_n_cells = n_cells;
    }
    float ksr = sp[0]->s_sr * .001;
    if(ksr != x->x_ksr){ // update
        x->x_ksr = ksr;
        mtxmc_ramp(x, x->x_ramp_ms);
    }
    signal_setmultiout(&sp[1], x->x_n_outs);
    dsp_add(mtxmc_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
}

static void *mtxmc_free(t_mtxmc *x){
    freebytes(x->x_input, x->x_n*x->x_n_ins*sizeof(*x->x_input));
    freebytes(x->x_tgain, x->x_n_cells*sizeof(*x->x_tgain));
    freebytes(x->x_g, x->x_n_cells*sizeof(*x->x_g));
    freebytes(x->x_inc, x->x_n_cells*sizeof(*x->x_inc));
    freebytes(x->x_nleft, x->x_n_cells*sizeof(*x->x_nleft));
    return(void *)x;
}

static void *mtxmc_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_mtxmc *x = (t_mtxmc *)pd_new(mtxmc_class);
    t_float rampms = 10.;
    x->x_n_ins = x->x_n_outs = 1;
    x->x_ksr = sys_getsr() * .001;
    x->x_n = sys_getblksize();
    int i;
    if(ac){
        int ins = atom_getint(av);
        x->x_n_ins = ins < 1 ? 1 : ins > MTXMC_MAX_INOUT ? MTXMC_MAX_INOUT : ins;
        ac--, av++;
    }
    if(ac){
        int outs = atom_getint(av);
        x->x_n_outs = outs < 1 ? 1 : outs > MTXMC_MAX_INOUT ? MTXMC_MAX_INOUT : outs;
        ac--, av++;
    }
    if(ac)
        rampms = atom_getfloat(av);
    x->x_input = (t_float *)getbytes(x->x_n*x->x_n_ins*sizeof(*x->x_input));
    x->x_n_cells = x->x_n_ins * x->x_n_outs;
    x->x_tgain = getbytes(x->x_n_cells * sizeof(*x->x_tgain));
    x->x_g = getbytes(x->x_n_cells * sizeof(*x->x_g));
    x->x_nleft = getbytes(x->x_n_cells * sizeof(*x->x_nleft));
    x->x_inc = getbytes(x->x_n_cells * sizeof(*x->x_inc));
    for(i = 0; i < x->x_n_cells; i++)
        x->x_tgain[i] = x->x_g[i] = x->x_nleft[i] = x->x_inc[i] = 0.;
    mtxmc_ramp(x, rampms);
    outlet_new(&x->x_obj, gensym("signal"));
    return(x);
}

void setup_mtx0x2emc_tilde(void){
    mtxmc_class = class_new(gensym("mtx.mc~"), (t_newmethod)mtxmc_new,
        (t_method)mtxmc_free, sizeof(t_mtxmc), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(mtxmc_class, nullfn, gensym("signal"), 0);
    class_addmethod(mtxmc_class, (t_method)mtxmc_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(mtxmc_class, mtxmc_list);
    class_addmethod(mtxmc_class, (t_method)mtxmc_clear, gensym("clear"), 0);
    class_addmethod(mtxmc_class, (t_method)mtxmc_ramp, gensym("ramp"), A_FLOAT, 0);
    class_addmethod(mtxmc_class, (t_method)mtxmc_outs, gensym("outs"), A_FLOAT, 0);
    class_addmethod(mtxmc_class, (t_method)mtxmc_print, gensym("print"), 0);
}
