// porres 2023, rewrote from scratcg and ditched old version based on cyclone's

#include <m_pd.h>

#define mtx_MAX_INOUT  4096

typedef struct _mtx{
    t_object  x_obj;
    int       x_n;
    int       x_n_ins, x_n_outs, x_n_cells;
    int       x_ramp;                           // ramp size in samples
    int      *x_nleft;                          // n samples left in ramp for each cell
    float     x_ksr, x_ramp_ms;                 // sr in khz and ramp size in ms
    t_float **x_ins, **x_outs;                  // inputs and outputs
    t_float  *x_inputs;                         // inputs copy
    t_float  *x_tgain, *x_g, *x_inc;            // target gains, current gain and inc step
}t_mtx;

static t_class *mtx_class;

static void mtx_set_gain(t_mtx *x, int i, float g){
    if(x->x_tgain[i] == g)
        return;
    x->x_tgain[i] = g;
    x->x_nleft[i] = x->x_ramp;
    x->x_inc[i] = (x->x_tgain[i] - x->x_g[i]) / (float)x->x_nleft[i];
}

static void mtx_list(t_mtx *x, t_symbol *s, int ac, t_atom *av){
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
    mtx_set_gain(x, i, atom_getfloat(av++));
}

static void mtx_clear(t_mtx *x){
    for(int i = 0; i < x->x_n_cells; i++)
        mtx_set_gain(x, i, 0); // set gain to 0
}

static void mtx_ramp(t_mtx *x, t_floatarg f){
    x->x_ramp_ms = f < 0 ? 0 : f;
    x->x_ramp = (int)((x->x_ramp_ms * x->x_ksr) + 0.5);
}

static void mtx_print(t_mtx *x){
    post("-- [mtx.mc~] --:");
    for(int i = 0; i < x->x_n_ins; i++)
        for(int o = 0; o < x->x_n_outs; o++)
            post("%d %d %g", i, o, x->x_tgain[i*x->x_n_outs + o]);
}

static t_int *mtx_perform(t_int *w){
    t_mtx *x = (t_mtx *)(w[1]);
    t_float *ins = x->x_inputs;
    int i;
    for(int j = 0; j < x->x_n_ins; j++){ // copy input
        for(i = 0; i < x->x_n; i++)
            ins[j*x->x_n+i] = x->x_ins[j][i];
    }
    for(int j = 0; j < x->x_n_outs; j++){ // zero outputs
        for(i = 0; i < x->x_n; i++)
            x->x_outs[j][i] = 0;
    }
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
                x->x_outs[o][n] += ins[i*x->x_n + n] * gain;
            }
        }
    }
    return(w+2);
}

static void mtx_dsp(t_mtx *x, t_signal **sp){
    t_signal **sigp = sp;
    int i, n = sp[0]->s_n;
    for(i = 0; i < x->x_n_ins; i++)  // 'n' inlets
        *(x->x_ins+i) = (*sigp++)->s_vec;
    for(i = 0; i < x->x_n_outs; i++) // 'n' outlets
        *(x->x_outs+i) = (*sigp++)->s_vec;
    float ksr = sp[0]->s_sr * .001;
    if(ksr != x->x_ksr){ // update
        x->x_ksr = ksr;
        mtx_ramp(x, x->x_ramp_ms);
    }
    if(n != x->x_n){
        x->x_inputs = (t_float *)resizebytes(x->x_inputs,
            x->x_n*x->x_n_ins * sizeof(t_float), n*x->x_n_ins * sizeof(t_float));
        x->x_n = n;
    }
    dsp_add(mtx_perform, 1, x);
}

static void *mtx_free(t_mtx *x){
    freebytes(x->x_inputs, x->x_n*x->x_n_ins*sizeof(*x->x_inputs));
    freebytes(x->x_ins, x->x_n_ins * sizeof(*x->x_ins));
    freebytes(x->x_outs, x->x_n_outs * sizeof(*x->x_outs));
    freebytes(x->x_tgain, x->x_n_cells*sizeof(*x->x_tgain));
    freebytes(x->x_g, x->x_n_cells*sizeof(*x->x_g));
    freebytes(x->x_inc, x->x_n_cells*sizeof(*x->x_inc));
    freebytes(x->x_nleft, x->x_n_cells*sizeof(*x->x_nleft));
    return(void *)x;
}

static void *mtx_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_mtx *x = (t_mtx *)pd_new(mtx_class);
    t_float rampms = 10.;
    x->x_n_ins = x->x_n_outs = 1;
    x->x_ksr = sys_getsr() * .001;
    x->x_n = sys_getblksize();
    int i;
    if(ac){
        int ins = atom_getint(av);
        x->x_n_ins = ins < 1 ? 1 : ins > mtx_MAX_INOUT ? mtx_MAX_INOUT : ins;
        ac--, av++;
    }
    if(ac){
        int outs = atom_getint(av);
        x->x_n_outs = outs < 1 ? 1 : outs > mtx_MAX_INOUT ? mtx_MAX_INOUT : outs;
        ac--, av++;
    }
    if(ac)
        rampms = atom_getfloat(av);
    x->x_ins = getbytes(x->x_n_ins * sizeof(*x->x_ins));
    x->x_outs = getbytes(x->x_n_outs * sizeof(*x->x_outs));
    x->x_inputs = (t_float *)getbytes(x->x_n*x->x_n_ins*sizeof(*x->x_inputs));
    x->x_n_cells = x->x_n_ins * x->x_n_outs;
    x->x_tgain = getbytes(x->x_n_cells * sizeof(*x->x_tgain));
    x->x_g = getbytes(x->x_n_cells * sizeof(*x->x_g));
    x->x_nleft = getbytes(x->x_n_cells * sizeof(*x->x_nleft));
    x->x_inc = getbytes(x->x_n_cells * sizeof(*x->x_inc));
    for(i = 0; i < x->x_n_cells; i++)
        x->x_tgain[i] = x->x_g[i] = x->x_nleft[i] = x->x_inc[i] = 0.;
    mtx_ramp(x, rampms);
    for(i = 1; i < x->x_n_ins; i++)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    for(i = 0; i < x->x_n_outs; i++)
        outlet_new(&x->x_obj, gensym("signal"));
    return(x);
}

void mtx_tilde_setup(void){
    mtx_class = class_new(gensym("mtx~"), (t_newmethod)mtx_new,
        (t_method)mtx_free, sizeof(t_mtx), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(mtx_class, nullfn, gensym("signal"), 0);
    class_addmethod(mtx_class, (t_method)mtx_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(mtx_class, mtx_list);
    class_addmethod(mtx_class, (t_method)mtx_clear, gensym("clear"), 0);
    class_addmethod(mtx_class, (t_method)mtx_ramp, gensym("ramp"), A_FLOAT, 0);
    class_addmethod(mtx_class, (t_method)mtx_print, gensym("print"), 0);
}
