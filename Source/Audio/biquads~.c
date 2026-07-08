// porres

#include <m_pd.h>
#include <stdlib.h>

#define COEFFS      5    // number of coeffs per filter stage
#define MAX_COEFFS  250  // defining max number of coeffs to take
#define MAX_STAGES  50   // number of MAX_STAGES = MAX_COEFFS/COEFFS

static t_class *biquads_class;

typedef struct _biquads{
    t_object    x_obj;
    t_outlet   *x_outlet;
    double     *x_xnm1;
    double     *x_xnm2;
    double     *x_ynm1;
    double     *x_ynm2;
    int         x_bypass;
    int         x_n;
    int         x_nchans;
    int         x_numfilt; // number of biquad filters
    int         x_allocfilt;
    double      x_coeff[MAX_COEFFS]; // array of coeffs
// the coeff array is an easy/cheap way of doing this
// without malloc/calloc-ing - maybe worth changing in the future
}t_biquads;

void *biquads_new(void);

void biquads_clear(t_biquads *x){
    for(int i = 0; i < x->x_allocfilt * x->x_nchans; i++)
        x->x_xnm1[i] = x->x_xnm2[i] = x->x_ynm1[i] = x->x_ynm2[i] = 0.;
}

void biquads_bypass(t_biquads *x, t_floatarg f){
    x->x_bypass = f != 0;
}

static void biquads_list(t_biquads *x, t_symbol *s, int ac, t_atom *av){
    (void)s;
    if(ac % COEFFS != 0)
        pd_error(x, "biquads~: list length must be a multiple of %d, "
            "trailing %d atom(s) ignored", COEFFS, ac % COEFFS);

    int numfilt = ac / COEFFS;
    if(numfilt > MAX_STAGES){
        pd_error(x, "biquads~: too many stages (%d), truncating to %d",
            numfilt, MAX_STAGES);
        numfilt = MAX_STAGES;
    }
    if(numfilt != x->x_allocfilt){
        x->x_xnm1 = (double *)resizebytes(x->x_xnm1,
            x->x_allocfilt * x->x_nchans * sizeof(double),
            numfilt * x->x_nchans * sizeof(double));
        x->x_xnm2 = (double *)resizebytes(x->x_xnm2,
            x->x_allocfilt * x->x_nchans * sizeof(double),
            numfilt * x->x_nchans * sizeof(double));
        x->x_ynm1 = (double *)resizebytes(x->x_ynm1,
            x->x_allocfilt * x->x_nchans * sizeof(double),
            numfilt * x->x_nchans * sizeof(double));
        x->x_ynm2 = (double *)resizebytes(x->x_ynm2,
            x->x_allocfilt * x->x_nchans * sizeof(double),
            numfilt * x->x_nchans * sizeof(double));
        x->x_allocfilt = numfilt;
        biquads_clear(x);
    }
    x->x_numfilt = numfilt;
    for(int curfilt = 0; curfilt < numfilt; curfilt++){
        int idx = COEFFS * curfilt;
        x->x_coeff[idx+0] = atom_getfloatarg(idx+0, ac, av);
        x->x_coeff[idx+1] = atom_getfloatarg(idx+1, ac, av);
        x->x_coeff[idx+2] = atom_getfloatarg(idx+2, ac, av);
        x->x_coeff[idx+3] = atom_getfloatarg(idx+3, ac, av);
        x->x_coeff[idx+4] = atom_getfloatarg(idx+4, ac, av);
    }
}

static t_int *biquads_perform(t_int *w){
    t_biquads *x = (t_biquads *)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    int numfilt = x->x_numfilt, n = x->x_n;
    double a0, a1, a2, b1, b2;
    double xn, xnm1, xnm2, yn, ynm1, ynm2;
    for(int j = 0; j < x->x_nchans; j++){
        unsigned int offset = j * x->x_allocfilt;
        for(int i = 0; i < n; i++){
            if(x->x_bypass)
                out[j*n+i] = in[j*n+i];
            else{
                xn = in[j*n+i];
                for(int curfilt = 0; curfilt < numfilt; curfilt++){
                    int idx = COEFFS * curfilt;
                    b1 = x->x_coeff[idx+0];
                    b2 = x->x_coeff[idx+1];
                    a0 = x->x_coeff[idx+2];
                    a1 = x->x_coeff[idx+3];
                    a2 = x->x_coeff[idx+4];
                    xnm1 = x->x_xnm1[offset + curfilt];
                    xnm2 = x->x_xnm2[offset + curfilt];
                    ynm1 = x->x_ynm1[offset + curfilt];
                    ynm2 = x->x_ynm2[offset + curfilt];
                    yn = a0*xn + a1*xnm1 + a2*xnm2 + b1*ynm1 + b2*ynm2;
                    x->x_xnm2[offset + curfilt] = xnm1;
                    x->x_xnm1[offset + curfilt] = xn;
                    x->x_ynm2[offset + curfilt] = ynm1;
                    x->x_ynm1[offset + curfilt] = yn;
                    xn = yn;
                }
                out[j*n+i] = xn;
            }
        }
    }
    return(w+4);
}

static void biquads_dsp(t_biquads *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    if(chs != x->x_nchans){
        if(x->x_allocfilt > 0){
            x->x_xnm1 = (double *)resizebytes(x->x_xnm1,
                x->x_nchans * x->x_allocfilt * sizeof(double),
                chs * x->x_allocfilt * sizeof(double));
            x->x_xnm2 = (double *)resizebytes(x->x_xnm2,
                x->x_nchans * x->x_allocfilt * sizeof(double),
                chs * x->x_allocfilt * sizeof(double));
            x->x_ynm1 = (double *)resizebytes(x->x_ynm1,
                x->x_nchans * x->x_allocfilt * sizeof(double),
                chs * x->x_allocfilt * sizeof(double));
            x->x_ynm2 = (double *)resizebytes(x->x_ynm2,
                x->x_nchans * x->x_allocfilt * sizeof(double),
                chs * x->x_allocfilt * sizeof(double));
            x->x_nchans = chs;
            biquads_clear(x);
        }
        else
            x->x_nchans = chs;
    }
    signal_setmultiout(&sp[1], chs);
    dsp_add(biquads_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
}

static void *biquads_free(t_biquads *x){
    freebytes(x->x_xnm1, x->x_allocfilt * x->x_nchans * sizeof(double));
    freebytes(x->x_xnm2, x->x_allocfilt * x->x_nchans * sizeof(double));
    freebytes(x->x_ynm1, x->x_allocfilt * x->x_nchans * sizeof(double));
    freebytes(x->x_ynm2, x->x_allocfilt * x->x_nchans * sizeof(double));
    outlet_free(x->x_outlet);
    return (void *)x;
}
void *biquads_new(void){
    t_biquads *x = (t_biquads *)pd_new(biquads_class);
    x->x_outlet = outlet_new(&x->x_obj, &s_signal);
    x->x_bypass = 0;
    x->x_n = 0;
    x->x_numfilt = 0;
    x->x_allocfilt = 0;
    x->x_nchans = 1;
    x->x_xnm1 = x->x_xnm2 = x->x_ynm1 = x->x_ynm2 = NULL;
    for(int i = 0; i < MAX_COEFFS; i++)
        x->x_coeff[i] = 0.;
    return(x);
}

void biquads_tilde_setup(void){
    biquads_class = class_new(gensym("biquads~"), (t_newmethod)biquads_new,
        (t_method)biquads_free, sizeof(t_biquads), CLASS_MULTICHANNEL, 0);
    class_addmethod(biquads_class, nullfn, gensym("signal"), 0);
    class_addmethod(biquads_class, (t_method) biquads_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(biquads_class, (t_method) biquads_clear, gensym("clear"), 0);
    class_addmethod(biquads_class, (t_method) biquads_bypass, gensym("bypass"), A_DEFFLOAT, 0);
    class_addlist(biquads_class,(t_method)biquads_list);
}
