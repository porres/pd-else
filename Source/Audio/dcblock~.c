// Porres

#include <m_pd.h>
#include <math.h>

#define PI 3.14159265358979323846
#define DC_BLOCK_FREQ 5.0

typedef struct _dcblock{
    t_object    x_obj;
    t_int       x_n;
    int         x_nchans;
    double      x_R;
    double     *x_xnm1;
    double     *x_ynm1;
}t_dcblock;

static t_class *dcblock_class;

static t_int *dcblock_perform(t_int *w){
    t_dcblock *x = (t_dcblock *)(w[1]);
    t_sample *in = (t_sample *)(w[2]);
    t_sample *out = (t_sample *)(w[3]);
    for(int j = 0, n = x->x_n; j < x->x_nchans; j++){
        double xnm1 = x->x_xnm1[j];
        double ynm1 = x->x_ynm1[j];
        for(int i = 0; i < n; i++){
            double xn = in[j*n+i];
            double yn = xn - xnm1 + x->x_R * ynm1;
            out[j*n+i] = yn;
            xnm1 = xn;
            ynm1 = yn;
        }
        x->x_xnm1[j] = xnm1;
        x->x_ynm1[j] = ynm1;
    }
    return(w+4);
}

static void dcblock_dsp(t_dcblock *x, t_signal **sp){
    x->x_n = sp[0]->s_n;
    double sr = sp[0]->s_sr;
    x->x_R = sr > 0 ? exp(-2.0 * PI * DC_BLOCK_FREQ / sr) : 0;
    int chs = sp[0]->s_nchans;
    if(x->x_nchans != chs){
        x->x_xnm1 = (double *)resizebytes(x->x_xnm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_ynm1 = (double *)resizebytes(x->x_ynm1,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        for(int j = x->x_nchans; j < chs; j++){
            x->x_xnm1[j] = 0;
            x->x_ynm1[j] = 0;
        }
        x->x_nchans = chs;
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(dcblock_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
}

static void dcblock_clear(t_dcblock *x){
    for(int i = 0; i < x->x_nchans; i++)
        x->x_xnm1[i] = x->x_ynm1[i] = 0;
}

static void *dcblock_free(t_dcblock *x){
    freebytes(x->x_xnm1, x->x_nchans * sizeof(*x->x_xnm1));
    freebytes(x->x_ynm1, x->x_nchans * sizeof(*x->x_ynm1));
    return(void *)x;
}

static void *dcblock_new(void){
    t_dcblock *x = (t_dcblock *)pd_new(dcblock_class);
    x->x_nchans = 1;
    x->x_xnm1 = (double *)getbytes(sizeof(*x->x_xnm1));
    x->x_ynm1 = (double *)getbytes(sizeof(*x->x_ynm1));
    x->x_xnm1[0] = 0;
    x->x_ynm1[0] = 0;
    x->x_R = 0;
    outlet_new((t_object *)x, &s_signal);
    return(x);
}

void dcblock_tilde_setup(void){
    dcblock_class = class_new(gensym("dcblock~"), (t_newmethod)dcblock_new,
        (t_method)dcblock_free, sizeof(t_dcblock), CLASS_MULTICHANNEL, 0);
    class_addmethod(dcblock_class, (t_method)dcblock_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(dcblock_class, nullfn, gensym("signal"), 0);
    class_addmethod(dcblock_class, (t_method)dcblock_clear, gensym("clear"), 0);
}
