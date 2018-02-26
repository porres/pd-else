#include <m_pd.h>
#include <float.h>
#include <math.h>

typedef struct _multigate{
    t_object   x_obj;
    t_int      x_n_ch;     // number of channels
    t_float    x_gate;     // 0 = off, nonzero = on
    t_sample  *x_temp;     // temporary audio vectors
    t_int      x_blksize;
}t_multigate;

static t_class *multigate_class;

static t_int *multigate_perform(t_int *w)
{
    t_int i, j;
    t_int nchannels     = (t_int)(w[1]);
    t_int blocksize     = (t_int)(w[2]);
    t_sample *gate      = (t_sample *)(w[3]);
    t_sample *temporary = (t_sample *)(w[4]);
    t_sample *tempio, *tempgate, *temp;
    
    for(j = 0; j < blocksize; ++j)
    {
        // Safe and valid check for booloean float
        gate[j] = (fabsf(gate[j]) > FLT_EPSILON);
    }
    // Record the result of the inputs multiplied by the gate into the temporary vector
    temp = temporary;
    for(i = 0; i < nchannels; ++i)
    {
        tempgate = gate;
        tempio = (t_sample *)(w[i+5]);
        for(j = 0; j < blocksize; ++j)
        {
            *temp++ = *tempio++ * *tempgate++;
        }
    }
    // Record temporary vector into the outputs
    temp = temporary;
    for(i = 0; i < nchannels; ++i)
    {
        tempio = (t_sample *)(w[i+5+nchannels]);
        for(j = 0; j < blocksize; ++j)
        {
            *tempio++ = *temp++;
        }
    }
    
    return (w + nchannels * 2 + 5);
}

static void multigate_free(t_multigate *x)
{
    if(x->x_temp)
    {
        freebytes(x->x_temp, x->x_n_ch * x->x_blksize * sizeof(*x->x_temp));
        x->x_temp = NULL;
        x->x_blksize = 0;
    }
}

static void multigate_dsp(t_multigate *x, t_signal **sp)
{
    size_t i, vecsize;
    t_int* vec;
    // Free the memory if the temporary vector has already been allocated
    multigate_free(x);
    // Allocate a C-like matrix (number of rows * number of columns)
    x->x_blksize = (t_int)sp[0]->s_n;
    x->x_temp    = (t_sample *)getbytes(x->x_n_ch * x->x_blksize * sizeof(*x->x_temp));
    if(x->x_temp)
    {
        // Pass the argument to the method and the arguments to DSP chain
        vecsize = x->x_n_ch * 2 + 4;
        vec = (t_int *)getbytes(vecsize * sizeof(*vec));
        if(vec)
        {
            vec[0] = (t_int)x->x_n_ch;
            vec[1] = (t_int)sp[0]->s_n;
            vec[2] = (t_int)sp[0]->s_vec;
            vec[3] = (t_int)x->x_temp;
            for(i = 0; i < x->x_n_ch * 2; ++i)
            {
                vec[i+4] = (t_int)sp[i+1]->s_vec;
            }
            dsp_addv(multigate_perform, vecsize, vec);
            freebytes(vec, vecsize * sizeof(*vec));
        }
        else
        {
            pd_error(x, "can't allocate temporary vectors.");
        }
    }
    else
    {
        x->x_blksize = 0;
        pd_error(x, "can't allocate temporary vectors.");
    }
}

static void *multigate_new(t_symbol *s, t_float nchannels, t_floatarg state)
{
    int i;
    t_multigate *x = (t_multigate *)pd_new(multigate_class);
    if(x)
    {
        x->x_n_ch   = (int)nchannels >= 1 ? (int)nchannels : 1;
        x->x_gate   = (int)state != 0;
        x->x_temp   = NULL;
        x->x_blksize = 0;
        for(i = 0; i < x->x_n_ch; i++)
        {
            inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
            outlet_new((t_object *)x, &s_signal);
        }
    }
    return(x);
}

void multigate_tilde_setup(void){
    t_class* c = class_new(gensym("multigate~"), (t_newmethod)multigate_new, (t_method)multigate_free,
            sizeof(t_multigate), CLASS_DEFAULT, A_FLOAT, A_DEFFLOAT, 0);
    if(c)
    {
        class_addmethod(c, (t_method)multigate_dsp, gensym("dsp"), A_CANT, 0);
        CLASS_MAINSIGNALIN(c, t_multigate, x_gate);
        multigate_class = c;
    }
}
