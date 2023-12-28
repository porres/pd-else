// based on cyclone/matrix~

#include "m_pd.h"

#define MTX_MAX_INOUT   512
#define MTX_DEFFADE     10.0
#define MTX_MINFADE     0.001    // LATER rethink!!!????!???!?!!!?

typedef struct _mtx{
    t_object   x_obj;
    int        x_n;
    int        x_n_ins;
    int        x_n_outs;
    int        x_maxblock;
    int        x_ncells;
    float      x_fade;
    int       *x_cells;
    int       *x_remains;
    t_float  **x_ins;
    t_float  **x_outs;
    t_float  **x_osums;
    float      x_ksr;
    float     *x_coefs;         // current coefs
    float     *x_gains;         // target gains
    float     *x_incrs;
    float     *x_bigincrs;
}t_mtx;

static t_class *mtx_class;

static void mtx_retarget(t_mtx *x, int i){ // LATER deal with changing nblock/ksr
    float target = (x->x_cells[i] ? x->x_gains[i] : 0.);
    if(x->x_fade < MTX_MINFADE){
        x->x_coefs[i] = target;
        x->x_remains[i] = 0;
    }
    else{
        x->x_remains[i] = x->x_fade * x->x_ksr + 0.5;  // LATER rethink
        x->x_incrs[i] = (target - x->x_coefs[i]) / (float)x->x_remains[i];
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
    int on = gain != 0;
    x->x_cells[cell_idx] = on;
    if(on)
        x->x_gains[cell_idx] = gain;
    mtx_retarget(x, cell_idx);
}

static void mtx_clear(t_mtx *x){
    for(int i = 0; i < x->x_ncells; i++){
        x->x_cells[i] = 0;
        mtx_retarget(x, i);
    }
}

static void mtx_fade(t_mtx *x, t_floatarg f){
    x->x_fade = f < 0 ? 0 : f;
}

static t_int *mtx_perform(t_int *w){
    t_mtx *x = (t_mtx *)(w[1]);
    int nblock = x->x_n;
    t_float **ins = x->x_ins;
    t_float **outs = x->x_outs;
    t_float **osums = x->x_osums;
    int *cellp = x->x_cells;
    float *gainp = x->x_gains;
    float *coefp = x->x_coefs;
    float *incrp = x->x_incrs;
    float *bigincrp = x->x_bigincrs;
    int *nleftp = x->x_remains;
    int indx;
    for(indx = 0; indx < x->x_n_ins; indx++){
        t_float *input = *ins++;
        t_float **ovecp = osums;
        int ondx = x->x_n_outs;
        while(ondx--){
            t_float *in = input;
            t_float *out = *ovecp;
            float nleft = *nleftp;
            int sndx = nblock;
            if(nleft >= nblock){
                float coef = *coefp;
                float incr = *incrp;
                if((*nleftp -= nblock) == 0)
                    *coefp = (*cellp ? *gainp : 0.);
                else
                    *coefp += *bigincrp;
                while(sndx--)
                    *out++ += *in++ * coef, coef += incr;
            }
            else if(nleft > 0){
                float coef = *coefp;
                float incr = *incrp;
                sndx -= nleft;
                do{
                    *out++ += *in++ * coef, coef += incr;
                }while(--nleft);
                if(*cellp){
                    coef = *coefp = *gainp;
                    while(sndx--)
                        *out++ += *in++ * coef;
                }
                else
                    *coefp = 0.;
                *nleftp = 0;
            }
            else if(*cellp){
                float coef = *coefp;
                while(sndx--)
                    *out++ += *in++ * coef;
            }
            cellp++, ovecp++, gainp++, coefp++;
            incrp++, bigincrp++, nleftp++;
        }
    }
    osums = x->x_osums;
    indx = x->x_n_outs;
    while(indx--){
        t_float *in = *osums++;
        t_float *out = *outs++;
        int sndx = nblock;
        while(sndx--){
            *out++ = *in;
            *in++ = 0.;
        }
    }
    return(w+2);
}

static void mtx_dsp(t_mtx *x, t_signal **sp){
    int i, nblock = sp[0]->s_n;
    x->x_ksr = sp[0]->s_sr * .001;
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
    dsp_add(mtx_perform, 1, x);
}

static void mtx_print(t_mtx *x){
    post("[mtx~]:");
    int *cellp = x->x_cells;
    float *gp = x->x_gains;
    for(int indx = 0; indx < x->x_n_ins; indx++)
        for(int ondx = 0; ondx < x->x_n_outs; ondx++, cellp++, gp++)
            post("%d %d %g", indx, ondx, *cellp ? *gp : 0.);
}

static void *mtx_free(t_mtx *x){
    freebytes(x->x_ins, x->x_n_ins * sizeof(*x->x_ins));
    freebytes(x->x_outs, x->x_n_outs * sizeof(*x->x_outs));
    for(int i = 0; i < x->x_n_outs; i++)
        freebytes(x->x_osums[i], x->x_maxblock * sizeof(*x->x_osums[i]));
    freebytes(x->x_osums, x->x_n_outs * sizeof(*x->x_osums));
    freebytes(x->x_cells, x->x_ncells * sizeof(*x->x_cells));
    freebytes(x->x_gains, x->x_ncells * sizeof(*x->x_gains));
    freebytes(x->x_coefs, x->x_ncells * sizeof(*x->x_coefs));
    freebytes(x->x_incrs, x->x_ncells * sizeof(*x->x_incrs));
    freebytes(x->x_bigincrs, x->x_ncells * sizeof(*x->x_bigincrs));
    freebytes(x->x_remains, x->x_ncells * sizeof(*x->x_remains));
    return(void *)x;
}

static void *mtx_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_mtx *x = (t_mtx *)pd_new(mtx_class);
    t_float fadeval = MTX_DEFFADE;
    x->x_n_ins = x->x_n_outs = 1;
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
        fadeval = atom_getfloat(av);
    x->x_ksr = sys_getsr() * .001;
    x->x_ncells = x->x_n_ins * x->x_n_outs;
    x->x_ins = getbytes(x->x_n_ins * sizeof(*x->x_ins));
    x->x_outs = getbytes(x->x_n_outs * sizeof(*x->x_outs));
    x->x_n = x->x_maxblock = sys_getblksize();
    x->x_osums = getbytes(x->x_n_outs * sizeof(*x->x_osums));
    for(i = 0; i < x->x_n_outs; i++)
        x->x_osums[i] = getbytes(x->x_maxblock * sizeof(*x->x_osums[i]));
    x->x_cells = getbytes(x->x_ncells * sizeof(*x->x_cells));
    x->x_gains = getbytes(x->x_ncells * sizeof(*x->x_gains));
    x->x_coefs = getbytes(x->x_ncells * sizeof(*x->x_coefs));
    x->x_incrs = getbytes(x->x_ncells * sizeof(*x->x_incrs));
    x->x_bigincrs = getbytes(x->x_ncells * sizeof(*x->x_bigincrs));
    x->x_remains = getbytes(x->x_ncells * sizeof(*x->x_remains));
    for(i = 0; i < x->x_ncells; i++)
        x->x_coefs[i] = x->x_gains[i] = x->x_remains[i] = 0.;
    mtx_clear(x);
    mtx_fade(x, fadeval);
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
    class_addmethod(mtx_class, (t_method)mtx_fade, gensym("ramp"), A_FLOAT, 0);
    class_addmethod(mtx_class, (t_method)mtx_print, gensym("print"), 0);
}
