// porres

#include <m_pd.h>
#include <stdlib.h>
#include <math.h>

#define MRMS_MAXBUF         192000000   // max buffer size - undocumented
#define MRMS_DEF_BUFSIZE    1024        // default size
#define LOGTEN              2.302585092994

typedef struct _mrms{
    t_object        x_obj;
    unsigned int    x_size;
    double         *x_buf;          // buffer pointer
    unsigned int   *x_count;        // for 1st round of accumulation
    double         *x_accum;        // accumulation
    unsigned int   *x_bufrd;        // buffer readhead
    unsigned int    x_n_samps;      // number of samples to average
    unsigned int    x_n;
    unsigned int    x_nchans;
    int             x_db;
}t_mrms;

static t_class *mrms_class;

static void mrms_clear(t_mrms * x){ // clear buffer and reset things to 0
    unsigned int i;
    for(i = 0; i < x->x_nchans; i++){
        x->x_count[i] = x->x_bufrd[i] = 0;
        x->x_accum[i] = 0.0;
    }
    for(unsigned int i = 0; i < x->x_size; i++)
        x->x_buf[i] = 0.;
};

// set size and deal with allocation if needed
static void mrms_size(t_mrms *x, t_float f){
    unsigned int cursz = x->x_size;                     // current size
    unsigned int newsz = f < 1 ? 1 : (unsigned int)f;   // new requested size
    if(newsz > MRMS_MAXBUF)
        newsz = MRMS_MAXBUF;
    x->x_buf = (double *)realloc(x->x_buf, sizeof(double) * newsz * x->x_nchans);
    x->x_size = newsz;
    mrms_clear(x);
}

static void mrms_lin(t_mrms *x, t_float f){
    x->x_db = f == 0;
}

static void mrms_n(t_mrms *x, t_float f){
    unsigned int n = f < 1 ? 1 : f > x->x_size ? x->x_size : (unsigned int)f;
    if(n != x->x_n_samps){
        x->x_n_samps = n;
        mrms_clear(x);
    }
}

static t_int *mrms_perform(t_int *w){
    t_mrms *x = (t_mrms *)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    unsigned int nsamps = x->x_n_samps;
    unsigned int n = x->x_n;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            double input = (double)in[i+j*n], result;
            double squared = input * input;
            if(nsamps == 1)  // passes through
                result = input;
            else{
                if(x->x_count[j] < nsamps)
                    x->x_count[j]++;
                else // subtract oldest sample when full
                    x->x_accum[j] -= x->x_buf[j * x->x_size + x->x_bufrd[j]];
                x->x_accum[j] += squared; // add squared value
                x->x_buf[j * x->x_size + x->x_bufrd[j]++] = squared;
                if(x->x_bufrd[j] >= nsamps){  // wrap readhead and correct drift each cycle
                    x->x_bufrd[j] = 0;
                    double resum = 0.0;
                    for(unsigned int idx = 0; idx < nsamps; idx++)
                        resum += x->x_buf[j * x->x_size + idx];
                    x->x_accum[j] = resum;
                }
                result = x->x_accum[j] / (double)x->x_count[j];
                if(result < 0)
                    result = 0;
                else
                    result = sqrt(result);  // get rms
                if(x->x_db){ // convert to db
                    result = 20. * log(result)/LOGTEN;
                    if(result < -999)
                        result = -999;
                }
            }
            out[i+j*n] = (t_float)result;
        }
    }
    return(w+4);
}

static void mrms_dsp(t_mrms *x, t_signal **sp){
    int n = sp[0]->s_n;
    int chs = sp[0]->s_nchans;
    if(n != x->x_n || chs != x->x_nchans){
        x->x_count = (unsigned int *)resizebytes(x->x_count,
            x->x_nchans * sizeof(unsigned int), chs * sizeof(unsigned int));
        x->x_bufrd = (unsigned int *)resizebytes(x->x_bufrd,
            x->x_nchans * sizeof(unsigned int), chs * sizeof(unsigned int));
        x->x_accum = (double *)resizebytes(x->x_accum,
            x->x_nchans * sizeof(double), chs * sizeof(double));
        x->x_n = n;
        x->x_nchans = chs;
        mrms_size(x, (float)x->x_size);
        mrms_clear(x);
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(mrms_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
}

static void mrms_free(t_mrms *x){
    freebytes(x->x_count, x->x_nchans * sizeof(*x->x_count));
    freebytes(x->x_bufrd, x->x_nchans * sizeof(*x->x_bufrd));
    freebytes(x->x_accum, x->x_nchans * sizeof(*x->x_accum));
    free(x->x_buf);
}

static void *mrms_new(t_symbol *s, int ac, t_atom * av){
    (void)s;
    t_mrms *x = (t_mrms *)pd_new(mrms_class);
// default buf / size / n
    x->x_size = MRMS_DEF_BUFSIZE;
    x->x_db = 1;
    x->x_nchans = 1;
    x->x_n = 64;
    x->x_n_samps = 1;
    x->x_count = (unsigned int*)getbytes(sizeof(*x->x_count));
    x->x_bufrd = (unsigned int*)getbytes(sizeof(*x->x_bufrd));
    x->x_accum = (double*)getbytes(sizeof(*x->x_accum));
/////////////////////////////////////////////////////////////////////////////////
    int argn = 0;
    int flag = 0;
    while(ac > 0){
        if(av->a_type == A_SYMBOL){
            t_symbol * cursym = atom_getsymbol(av);
            if(cursym == gensym("-size") && !argn){
                if(ac >= 2 && (av+1)->a_type == A_FLOAT){
                    x->x_size = atom_getintarg(1, ac, av);
                    ac-=2, av+=2;
                    flag = 1;
                }
                else
                    goto errstate;
            }
            else if(cursym == gensym("-lin") && !argn){
                x->x_db = 0;
                ac--, av++;
            }
            else
                goto errstate;
        }
        else if(av->a_type == A_FLOAT){
            argn = 1;
            unsigned int nsamps = (unsigned int)atom_getint(av);
            if(nsamps < 1)
                nsamps = 1;
            x->x_n_samps = nsamps;
            if(!flag)
                x->x_size = nsamps;
            ac--, av++;
        }
        else
            goto errstate;
    };
/////////////////////////////////////////////////////////////////////////////////
    x->x_buf = NULL;
    mrms_size(x, (float)x->x_size);
    outlet_new((t_object *)x, &s_signal);
    return(x);
errstate:
    pd_error(x, "[mov.rms~]: improper args");
    return(NULL);
}

void setup_mov0x2erms_tilde(void){
    mrms_class = class_new(gensym("mov.rms~"), (t_newmethod)mrms_new,
            (t_method)mrms_free, sizeof(t_mrms), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(mrms_class, (t_method) mrms_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(mrms_class, nullfn, gensym("signal"), 0);
    class_addmethod(mrms_class, (t_method)mrms_clear, gensym("clear"), 0);
    class_addmethod(mrms_class, (t_method)mrms_size, gensym("size"), A_DEFFLOAT, 0);
    class_addmethod(mrms_class, (t_method)mrms_lin, gensym("lin"), A_DEFFLOAT, 0);
    class_addmethod(mrms_class, (t_method)mrms_n, gensym("n"), A_DEFFLOAT, 0);
}
