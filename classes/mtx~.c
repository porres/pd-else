#include <m_pd.h>

static t_class *mtx_class;

typedef struct _mtx{
    t_object    x_obj;
    t_float     x_f;
    size_t      x_ch;
    t_sample*   x_vtemp;
    size_t      x_vsize;
    t_inlet *x_inlet;
}t_mtx;

t_int *mtx_perform(t_int *w){
    size_t i, j, k;
    size_t ch       = (size_t)(w[1]);     // channels in and out
    size_t n        = (size_t)(w[2]);     // block size
    t_sample  *vec  = (t_sample *)(w[3]); // sample vectors?
    t_float   *gate = (t_float *)(w[4]);  // gate
    t_sample  *in, *out, *temp;
    temp = vec;                           // temp = sample vectors
    for(k = 0; k < n; k++){
        t_float gv = *gate++;
        for(i = 5; i < ch + 5; i++){ // i = 5; i < ch + 5; i++
            in = (t_sample *)(w[i]);
            for(j = 0; j < n; j++)
                *temp++ = *in++ + gv;              // <= multiply here!!!!
        }
        temp = vec;
        for(; i < 2*ch + 5; ++i){ // ; i < 2*ch + 5; ++i
            out = (t_sample *)(w[i]);
            for(j = 0; j < n; ++j)
                *out++ = *temp++;
        }
    }
    return(w + 2*ch + 5); // w + 2*ch + 5
}

void mtx_dsp(t_mtx *x, t_signal **sp){
    size_t i;
    t_int* vec;
    if(x->x_vsize && x->x_vtemp){
        freebytes(x->x_vtemp, x->x_vsize);
        x->x_vtemp = NULL;
        x->x_vsize = 0;
    }
    x->x_vsize = sizeof(t_sample) * x->x_ch * (size_t)sp[0]->s_n;
    x->x_vtemp = (t_sample *)getbytes(x->x_vsize);
    if(x->x_vtemp){
        vec = (t_int *)getbytes((x->x_ch * 2 + 4) * sizeof(t_int)); // x->x_ch * 2 + 4
        if(vec){
            vec[0] = (t_int)x->x_ch;        // channels in and out
            vec[1] = (t_int)sp[0]->s_n;     // block size
            vec[2] = (t_int)x->x_vtemp;     // sample vectors?
            vec[3] = (t_int)sp[0]->s_vec;     // gate
            for(i = 0; i < x->x_ch * 2; ++i)
                vec[i+4] = (t_int)sp[i+1]->s_vec; // vec[i+4]
            dsp_addv(mtx_perform, (x->x_ch * 2 + 4), vec); // x->x_ch * 2 + 4
            freebytes(vec, (x->x_ch * 2 + 3) * sizeof(t_int)); // change?? to "+ 3"?s
        }
        else
            pd_error(x, "[mtx~]: can't allocate temporary vectors.");
    }
    else
        pd_error(x, "[mtx~]: can't allocate temporary vectors.");
}

static void *mtx_new(t_float f){
    size_t i;
    t_mtx *x = (t_mtx *)pd_new(mtx_class);
    x->x_ch = (f < 1) ? 1 : (size_t)f;
    x->x_inlet = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    for(i = 0; i < x->x_ch; ++i){
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
        outlet_new(&x->x_obj, gensym("signal"));
    }
    x->x_vsize = 0;
    x->x_vtemp = NULL;
    return(x);
}

void mtx_free(t_mtx *x){
    inlet_free(x->x_inlet);
    if(x->x_vsize && x->x_vtemp){
        freebytes(x->x_vtemp, x->x_vsize);
        x->x_vtemp = NULL;
        x->x_vsize = 0;
    }
}

void mtx_tilde_setup(void){
    mtx_class = class_new(gensym("mtx~"), (t_newmethod)mtx_new, NULL,
        sizeof(t_mtx), CLASS_NOINLET, A_DEFFLOAT, 0);
    class_addmethod(mtx_class, (t_method)mtx_dsp, gensym("dsp"), A_CANT, 0);
//    CLASS_MAINSIGNALIN(mtx_class, t_mtx, x_f);
}
