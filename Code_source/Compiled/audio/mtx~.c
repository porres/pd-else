// based on cyclone/matrix~

#include "m_pd.h"

#define MTX_MAX_INOUT   512

typedef struct _mtx{
    t_object   x_obj;
    int        x_n, x_maxblock;
    int        x_n_ins, x_n_outs, x_n_cells;
    float      x_ksr, x_ramp_ms;
    int        x_ramp;                  // ramp size in samples
    int       *x_nleft;                 // n samples left in ramp
    t_float  **x_ins, **x_outs, **x_osums;
    float     *x_gains, *x_coefs;       // target gains, current coefs
    float     *x_incrs, *x_bigincrs;
}t_mtx;

static t_class *mtx_class;

static void mtx_retarget(t_mtx *x, int i){ // LATER deal with changing nblock/ksr
    x->x_nleft[i] = x->x_ramp;
    if(x->x_nleft[i] == 0){
        x->x_coefs[i] = x->x_gains[i];
        x->x_incrs[i] = x->x_bigincrs[i] = 0;
    }
    else{
        x->x_incrs[i] = (x->x_gains[i] - x->x_coefs[i]) / (float)x->x_nleft[i];
        x->x_bigincrs[i] = x->x_n * x->x_incrs[i];
    }
}

static void mtx_list(t_mtx *x, t_symbol *s, int ac, t_atom *av){
    s = NULL;
    if(ac != 3){
        if(ac == 1)
            pd_error(x, "[mtx~]: no method for float");
        else
            pd_error(x, "[mtx~]: list size must be '3'");
        return;
    }
    int inlet_idx = atom_getint(av++);
    if(inlet_idx < 0 || inlet_idx >= x->x_n_ins){  // bound check
        pd_error(x, "[mtx~]: %d is not a valid inlet index!", inlet_idx);
        return;
    };
    int outlet_idx = atom_getint(av++);
    if(outlet_idx < 0 || outlet_idx >= x->x_n_outs){
        pd_error(x, "[mtx~]: %d is not a valid outlet index!", outlet_idx);
        return;
    };
    float gain = atom_getfloat(av++);
    int cell_idx = inlet_idx * x->x_n_outs + outlet_idx;
    x->x_gains[cell_idx] = gain;
    mtx_retarget(x, cell_idx);
}

static void mtx_clear(t_mtx *x){
    for(int i = 0; i < x->x_n_cells; i++){
        x->x_gains[i] = 0;
        mtx_retarget(x, i);
    }
}

static void mtx_ramp(t_mtx *x, t_floatarg f){
    x->x_ramp_ms = f < 0 ? 0 : f;
    x->x_ramp = (int)((x->x_ramp_ms * x->x_ksr) + 0.5);
}

static void mtx_print(t_mtx *x){
    post("[mtx~]:");
    float *gp = x->x_gains;
    for(int indx = 0; indx < x->x_n_ins; indx++)
        for(int ondx = 0; ondx < x->x_n_outs; ondx++, gp++)
            post("%d %d %g", indx, ondx, *gp);
}

static t_int *mtx_perform(t_int *w){
    t_mtx *x = (t_mtx *)(w[1]);
    t_float **ins = x->x_ins, **outs = x->x_outs;
    int *nleftp = x->x_nleft;
    float *gainp = x->x_gains, *coefp = x->x_coefs;
    float *incrp = x->x_incrs, *bigincrp = x->x_bigincrs;
    for(int i = 0; i < x->x_n_ins; i++){
        t_float *input = *ins++;
        t_float **ovecp = x->x_osums;
        for(int j = 0; j < x->x_n_outs; j++){
            t_float *in = input;
            t_float *out = *ovecp;
            int n = x->x_n, nleft = *nleftp;
            if(nleft >= x->x_n){
                float coef = *coefp;
                float incr = *incrp;
                if((*nleftp -= x->x_n) == 0)
                    *coefp = *gainp;
                else
                    *coefp += *bigincrp;
                while(n--)
                    *out++ += *in++ * coef, coef += incr;
            }
            else if(nleft > 0){
                float coef = *coefp;
                float incr = *incrp;
                n -= nleft;
                do{
                    *out++ += *in++ * coef, coef += incr;
                }while(--nleft);
                if(*gainp != 0){
                    coef = *coefp = *gainp;
                    while(n--)
                        *out++ += *in++ * coef;
                }
                else
                    *coefp = 0.;
                *nleftp = 0;
            }
            else if(*gainp != 0){
                float coef = *coefp;
                while(n--)
                    *out++ += *in++ * coef;
            }
            ovecp++, gainp++, coefp++;
            incrp++, bigincrp++, nleftp++;
        }
    }
    t_float **osums = x->x_osums;
    for(int i = 0; i < x->x_n_outs; i++){
        t_float *in = *osums++;
        t_float *out = *outs++;
        for(int n = 0; n < x->x_n; n++){
            *out++ = *in;
            *in++ = 0.;
        }
    }
    return(w+2);
}

static void mtx_dsp(t_mtx *x, t_signal **sp){
    int i, nblock = sp[0]->s_n;
    float ksr = sp[0]->s_sr * .001;
    t_float **vecp = x->x_ins;
    t_signal **sigp = sp;
    for(i = 0; i < x->x_n_ins; i++)
        *vecp++ = (*sigp++)->s_vec;
    vecp = x->x_outs;
    for(i = 0; i < x->x_n_outs; i++)
        *vecp++ = (*sigp++)->s_vec;
    if(nblock != x->x_n){
        if(nblock > x->x_maxblock){
            size_t oldsize = x->x_maxblock * sizeof(*x->x_osums[i]),
            newsize = nblock * sizeof(*x->x_osums[i]);
            for(i = 0; i < x->x_n_outs; i++)
                x->x_osums[i] = resizebytes(x->x_osums[i], oldsize, newsize);
            x->x_maxblock = nblock;
        };
        x->x_n = nblock;
    }
    if(ksr != x->x_ksr){ // update
        x->x_ksr = ksr;
        mtx_ramp(x, x->x_ramp_ms);
    }
    dsp_add(mtx_perform, 1, x);
}

static void *mtx_free(t_mtx *x){
    freebytes(x->x_ins, x->x_n_ins * sizeof(*x->x_ins));
    freebytes(x->x_outs, x->x_n_outs * sizeof(*x->x_outs));
    for(int i = 0; i < x->x_n_outs; i++)
        freebytes(x->x_osums[i], x->x_maxblock * sizeof(*x->x_osums[i]));
    freebytes(x->x_osums, x->x_n_outs * sizeof(*x->x_osums));
    freebytes(x->x_gains, x->x_n_cells * sizeof(*x->x_gains));
    freebytes(x->x_coefs, x->x_n_cells * sizeof(*x->x_coefs));
    freebytes(x->x_incrs, x->x_n_cells * sizeof(*x->x_incrs));
    freebytes(x->x_bigincrs, x->x_n_cells * sizeof(*x->x_bigincrs));
    freebytes(x->x_nleft, x->x_n_cells * sizeof(*x->x_nleft));
    return(void *)x;
}

static void *mtx_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_mtx *x = (t_mtx *)pd_new(mtx_class);
    t_float rampms = 10.;
    x->x_n_ins = x->x_n_outs = 1;
    x->x_ksr = sys_getsr() * .001;
    x->x_n = x->x_maxblock = sys_getblksize();
    int i;
    if(ac){
        int ins = atom_getint(av);
        x->x_n_ins = ins < 1 ? 1 : ins > MTX_MAX_INOUT ? MTX_MAX_INOUT : ins;
        ac--, av++;
    }
    if(ac){
        int outs = atom_getint(av);
        x->x_n_outs = outs < 1 ? 1 : outs > MTX_MAX_INOUT ? MTX_MAX_INOUT : outs;
        ac--, av++;
    }
    if(ac)
        rampms = atom_getfloat(av);
    x->x_ins = getbytes(x->x_n_ins * sizeof(*x->x_ins));
    x->x_outs = getbytes(x->x_n_outs * sizeof(*x->x_outs));
    x->x_osums = getbytes(x->x_n_outs * sizeof(*x->x_osums));
    for(i = 0; i < x->x_n_outs; i++)
        x->x_osums[i] = getbytes(x->x_maxblock * sizeof(*x->x_osums[i]));
    x->x_n_cells = x->x_n_ins * x->x_n_outs;
    x->x_gains = getbytes(x->x_n_cells * sizeof(*x->x_gains));
    x->x_coefs = getbytes(x->x_n_cells * sizeof(*x->x_coefs));
    x->x_incrs = getbytes(x->x_n_cells * sizeof(*x->x_incrs));
    x->x_bigincrs = getbytes(x->x_n_cells * sizeof(*x->x_bigincrs));
    x->x_nleft = getbytes(x->x_n_cells * sizeof(*x->x_nleft));
    for(i = 0; i < x->x_n_cells; i++)
        x->x_coefs[i] = x->x_gains[i] = x->x_nleft[i] = 0.;
    mtx_clear(x);
    mtx_ramp(x, rampms);
    for(i = 1; i < x->x_n_ins; i++)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    for(i = 0; i < x->x_n_outs; i++)
         outlet_new(&x->x_obj, gensym("signal"));
    return(x);
}

void mtx_tilde_setup(void){
    mtx_class = class_new(gensym("mtx~"), (t_newmethod)mtx_new,
        (t_method)mtx_free, sizeof(t_mtx), 0, A_GIMME, 0);
    class_addmethod(mtx_class, nullfn, gensym("signal"), 0);
    class_addmethod(mtx_class, (t_method)mtx_dsp, gensym("dsp"), A_CANT, 0);
    class_addlist(mtx_class, mtx_list);
    class_addmethod(mtx_class, (t_method)mtx_clear, gensym("clear"), 0);
    class_addmethod(mtx_class, (t_method)mtx_ramp, gensym("ramp"), A_FLOAT, 0);
    class_addmethod(mtx_class, (t_method)mtx_print, gensym("print"), 0);
}
