// porres

#include <m_pd.h>
#include <stdlib.h>

#define MAVG_MAXBUF         192000000   // max buffer size - undocumented
#define MAVG_DEF_BUFSIZE    100         // default size

typedef struct _mavg{
    t_object        x_obj;
    unsigned int    x_size;
    double         *x_buf;                      // buffer pointer
    unsigned int   *x_count;                    // for 1st round of accumulation
    double         *x_accum;                    // accumulation
    unsigned int   *x_bufrd;                    // buffer readhead
    unsigned int    x_n_samps;                  // number of samples to average
    unsigned int    x_n;
    unsigned int    x_nchans;
}t_mavg;

static t_class *mavg_class;

static void mavg_clear(t_mavg * x){ // clear buffer and reset things to 0
    unsigned int i;
    for(i = 0; i < x->x_nchans; i++){
        x->x_count[i] = x->x_bufrd[i] = 0;
        x->x_accum[i] = 0.0;
    }
    for(unsigned int i = 0; i < x->x_size; i++)
        x->x_buf[i] = 0.;
};

// set size and deal with allocation if needed
static void mavg_size(t_mavg *x, t_float f){
    unsigned int cursz = x->x_size;                     // current size
    unsigned int newsz = f < 1 ? 1 : (unsigned int)f;   // new requested size
    if(newsz > MAVG_MAXBUF)
        newsz = MAVG_MAXBUF;
    x->x_buf = (double *)realloc(x->x_buf, sizeof(double) * newsz * x->x_nchans);
    x->x_size = newsz;
    mavg_clear(x);
}

static void mavg_n(t_mavg *x, t_float f){
    unsigned int n = f < 1 ? 1 : f > x->x_size ? x->x_size : (unsigned int)f;
    if(n != x->x_n_samps){
        x->x_n_samps = n;
        mavg_clear(x);
    }
}

static t_int *mavg_perform(t_int *w){
    t_mavg *x = (t_mavg *)(w[1]);
    t_float *in = (t_float *)(w[2]);
    t_float *out = (t_float *)(w[3]);
    unsigned int nsamps = x->x_n_samps;
    unsigned int n = x->x_n;
    for(int j = 0; j < x->x_nchans; j++){
        for(int i = 0; i < n; i++){
            double input = (double)in[i+j*n], result;
            if(nsamps == 1)  // passes through
                result = input;
            else{
                if(x->x_count[j] < nsamps)
                    x->x_count[j]++;
                else // subtract oldest sample when full
                    x->x_accum[j] -= x->x_buf[j * x->x_size + x->x_bufrd[j]];
                x->x_accum[j] += input; // add new input
                x->x_buf[j * x->x_size + x->x_bufrd[j]++] = input;
                if(x->x_bufrd[j] >= nsamps){  // wrap readhead and correct drift each cycle
                    x->x_bufrd[j] = 0;
                    double resum = 0.0;
                    for(unsigned int idx = 0; idx < nsamps; idx++)
                        resum += x->x_buf[j * x->x_size + idx];
                    x->x_accum[j] = resum;
                }
                result = x->x_accum[j] / (double)x->x_count[j];
            }
            out[i+j*n] = (t_float)result;
        }
    }
    return(w+4);
}

static void mavg_dsp(t_mavg *x, t_signal **sp){
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
        mavg_size(x, (float)x->x_size);
        mavg_clear(x);
    }
    signal_setmultiout(&sp[1], x->x_nchans);
    dsp_add(mavg_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
}

static void mavg_free(t_mavg *x){
    freebytes(x->x_count, x->x_nchans * sizeof(*x->x_count));
    freebytes(x->x_bufrd, x->x_nchans * sizeof(*x->x_bufrd));
    freebytes(x->x_accum, x->x_nchans * sizeof(*x->x_accum));
    free(x->x_buf);
}

static void *mavg_new(t_symbol *s, int ac, t_atom * av){
    (void)s;
    t_mavg *x = (t_mavg *)pd_new(mavg_class);
// default buf / size / n
    x->x_size = MAVG_DEF_BUFSIZE;
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
    mavg_size(x, (float)x->x_size);
    outlet_new((t_object *)x, &s_signal);
    return(x);
errstate:
    pd_error(x, "[mov.avg~]: improper args");
    return(NULL);
}

void setup_mov0x2eavg_tilde(void){
    mavg_class = class_new(gensym("mov.avg~"), (t_newmethod)mavg_new,
            (t_method)mavg_free, sizeof(t_mavg), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(mavg_class, (t_method) mavg_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(mavg_class, nullfn, gensym("signal"), 0);
    class_addmethod(mavg_class, (t_method)mavg_clear, gensym("clear"), 0);
    class_addmethod(mavg_class, (t_method)mavg_size, gensym("size"), A_DEFFLOAT, 0);
    class_addmethod(mavg_class, (t_method)mavg_n, gensym("n"), A_DEFFLOAT, 0);
}
