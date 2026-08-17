// based on cyclone's buffir~

#include <string.h>
#include "m_pd.h"
#include "buffer.h"

#define CONVFILT_MAXSIZE 4096

typedef struct _convfilt{
    t_object    x_obj;
    t_buffer   *x_buf;
    int         x_n;
    t_float   **x_lohead;
    t_float   **x_hihead;
    t_float    *x_histbuf;
    int         x_nchans;
}t_convfilt;

static t_class *convfilt_class;

static void convfilt_clear(t_convfilt *x){
    memset(x->x_histbuf, 0,
        x->x_nchans * 2 * CONVFILT_MAXSIZE * sizeof(*x->x_histbuf));
    for(int j = 0; j < x->x_nchans; j++){
        x->x_lohead[j] = x->x_histbuf +
            j * 2 * CONVFILT_MAXSIZE;
        x->x_hihead[j] = x->x_lohead[j] + CONVFILT_MAXSIZE;
    }
}

static void convfilt_set(t_convfilt *x, t_symbol *s){
    buffer_setarray(x->x_buf, s);
}

static t_int *convfilt_perform(t_int *w){
    t_convfilt *x = (t_convfilt *)(w[1]);
    t_float *xin = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    t_buffer *c = x->x_buf;
    int n = x->x_n;
    int bufnpts = c->c_npts;
    if(bufnpts > CONVFILT_MAXSIZE)
        bufnpts = CONVFILT_MAXSIZE;
    for(int j = 0; j < x->x_nchans; j++){
        t_float *lohead = x->x_lohead[j];
        t_float *hihead = x->x_hihead[j];
        for(int i = 0; i < n; i++){
            t_float *in = xin + j*n + i;
            t_float *o = out + j*n + i;
            if(c->c_playable){
                t_word *vec = c->c_vectors[0];
                int off = 0;
                int npts = bufnpts;
                if(npts > 0){
                    t_float *hp = hihead;
                    t_float sum = 0.;
                    *lohead++ = *hihead++ = *in;
                    while(npts--)
                        sum += vec[off++].w_float * *hp--;
                    *o = sum;
                }
                else{
                    *lohead++ = *hihead++ = *in;
                    *o = 0.;
                }
            }
            else{
                *lohead++ = *hihead++ = *in;
                *o = 0.;
            }
            if(lohead >= x->x_histbuf +
                (j * 2 + 1) * CONVFILT_MAXSIZE){
                lohead = x->x_histbuf +
                    j * 2 * CONVFILT_MAXSIZE;
                hihead = lohead + CONVFILT_MAXSIZE;
            }
        }
        x->x_lohead[j] = lohead;
        x->x_hihead[j] = hihead;
    }
    return(w+4);
}

static void convfilt_dsp(t_convfilt *x, t_signal **sp){
    buffer_checkdsp(x->x_buf);
    x->x_n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    if(x->x_nchans != chs){
        x->x_histbuf = (t_float *)resizebytes(x->x_histbuf,
            x->x_nchans * 2 * CONVFILT_MAXSIZE * sizeof(*x->x_histbuf),
            chs * 2 * CONVFILT_MAXSIZE * sizeof(*x->x_histbuf));
        x->x_lohead = (t_float **)resizebytes(x->x_lohead,
            x->x_nchans * sizeof(*x->x_lohead),
            chs * sizeof(*x->x_lohead));
        x->x_hihead = (t_float **)resizebytes(x->x_hihead,
            x->x_nchans * sizeof(*x->x_hihead),
            chs * sizeof(*x->x_hihead));
        x->x_nchans = chs;
        convfilt_clear(x);
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(convfilt_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
}

static void convfilt_free(t_convfilt *x){
    buffer_free(x->x_buf);
    freebytes(x->x_lohead, x->x_nchans * sizeof(*x->x_lohead));
    freebytes(x->x_hihead, x->x_nchans * sizeof(*x->x_hihead));
    freebytes(x->x_histbuf,
        x->x_nchans * 2 * CONVFILT_MAXSIZE * sizeof(*x->x_histbuf));
}
static void *convfilt_new(t_symbol *s){
    t_convfilt *x = (t_convfilt *)pd_new(convfilt_class);
    x->x_nchans = 1;
    x->x_histbuf = (t_float *)getbytes(
        2 * CONVFILT_MAXSIZE * sizeof(*x->x_histbuf));
    x->x_lohead = (t_float **)getbytes(sizeof(*x->x_lohead));
    x->x_hihead = (t_float **)getbytes(sizeof(*x->x_hihead));
    x->x_buf = buffer_init((t_class *)x, s, 1, 0);
    outlet_new(&x->x_obj, gensym("signal"));
    convfilt_clear(x);
    return(x);
}

void setup_conv0x2efilt_tilde(void){
    convfilt_class = class_new(gensym("conv.filt~"), (t_newmethod)convfilt_new,
        (t_method)convfilt_free, sizeof(t_convfilt), CLASS_MULTICHANNEL, A_DEFSYM, 0);
    class_addmethod(convfilt_class, (t_method)convfilt_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(convfilt_class, nullfn, gensym("signal"), 0);
    class_addmethod(convfilt_class, (t_method)convfilt_clear, gensym("clear"), 0);
    class_addmethod(convfilt_class, (t_method)convfilt_set, gensym("set"), A_SYMBOL, 0);
}
