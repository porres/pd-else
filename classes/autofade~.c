// porres 2017

#include "m_pd.h"
#include <math.h>

#define UNITBIT32 1572864.  /* 3*2^19; bit 32 has place value 1 */

#define HALF_PI M_PI * 0.5

/* machine-dependent definitions.  These ifdefs really
 should have been by CPU type and not by operating system! */
#ifdef IRIX
/* big-endian.  Most significant byte is at low address in memory */
#define HIOFFSET 0    /* word offset to find MSB */
#define LOWOFFSET 1    /* word offset to find LSB */
#define int32 long  /* a data type that has 32 bits */
#endif /* IRIX */

#ifdef MSW
/* little-endian; most significant byte is at highest address */
#define HIOFFSET 1
#define LOWOFFSET 0
#define int32 long
#endif /* MSW */

#if defined(__FreeBSD__) || defined(__APPLE__)
#include <machine/endian.h>
#endif

#ifdef __linux__
#include <endian.h>
#endif

#if defined(__unix__) || defined(__APPLE__)
#if !defined(BYTE_ORDER) || !defined(LITTLE_ENDIAN)
#error No byte order defined
#endif

#if BYTE_ORDER == LITTLE_ENDIAN
#define HIOFFSET 1
#define LOWOFFSET 0
#else
#define HIOFFSET 0    /* word offset to find MSB */
#define LOWOFFSET 1    /* word offset to find LSB */
#endif /* __BYTE_ORDER */
#include <sys/types.h>
#define int32 int32_t
#endif /* __unix__ or __APPLE__*/

#define MAXCH 64

t_float *autofade_table_lin=(t_float *)0L;
t_float *autofade_table_linsin=(t_float *)0L;
t_float *autofade_table_sqrt=(t_float *)0L;
t_float *autofade_table_quartic=(t_float *)0L;
t_float *autofade_table_sin=(t_float *)0L;
t_float *autofade_table_hannsin=(t_float *)0L;
t_float *autofade_table_hann=(t_float *)0L;


typedef struct _autofade{
    t_object x_obj;
    t_float  x_f;
    t_float  x_ms;
    t_int    x_n;
    t_int    x_ch;
    double   x_coef;
    t_float  x_last;
    t_float  x_target;
    t_float  x_sr_khz;
    double   x_incr;
    t_int    x_nleft;
    t_float  *x_ins[MAXCH];
    t_float  *x_outs[MAXCH];
    t_float  *x_table;
} t_autofade;

union tabfudge_d{
    double tf_d;
    int32 tf_i[2];
};

static t_class *autofade_class;

static void autofade_lin(t_autofade *x){
    x->x_table = autofade_table_lin;
}

static void autofade_linsin(t_autofade *x){
    x->x_table = autofade_table_linsin;
}

static void autofade_sqrt(t_autofade *x){
    x->x_table = autofade_table_sqrt;
}

static void autofade_quartic(t_autofade *x){
    x->x_table = autofade_table_quartic;
}


static void autofade_sin(t_autofade *x){
    x->x_table = autofade_table_sin;
}

static void autofade_hannsin(t_autofade *x){
    x->x_table = autofade_table_hannsin;
}

static void autofade_hann(t_autofade *x){
    x->x_table = autofade_table_hann;
}

static void autofade_fade(t_autofade *x, t_floatarg f){
    x->x_ms = f;
}

static t_int *autofade_perform(t_int *w){
    t_autofade *x = (t_autofade *)(w[1]);
    t_int nblock = (int)(w[2]);
    t_float *in_gate = (t_float *)(w[3]);               // gate input
    t_int i, j;
    for(i = 0; i < x->x_ch; i++)
        x->x_ins[i] = (t_float *)(w[4 + i]);            // channel inputs
    for(i = 0; i < x->x_ch; i++)
        x->x_outs[i] = (t_float *)(w[4 + x->x_ch + i]); // channel outputs
    t_float last = x->x_last;
    t_float target = x->x_target;
    double incr = x->x_incr;
    t_int nleft = x->x_nleft;
    
    t_float *tab = x->x_table, *addr, f1, f2, frac;
    double dphase;
    t_int normhipart;
    union tabfudge_d tf;
    
    tf.tf_d = UNITBIT32;
    normhipart = tf.tf_i[HIOFFSET];
    
    while (nblock--){
        t_float f = *in_gate++;
        f = f > 0;  ////////////// Gate!
        t_float ms = x->x_ms;
        if (ms < 0)
            ms = 0;
        
        t_float phase;
        
        x->x_n = roundf(ms * x->x_sr_khz);
        double coef;
        if (x->x_n == 0)
            coef = 0.;
        else
            coef = 1. / (float)x->x_n;
        
        if(coef != x->x_coef){
            x->x_coef = coef;
            if(f != last){
                if(x->x_n > 1){
                    incr = (f - last) * x->x_coef;
                    nleft = x->x_n;
                    
                    phase = last += incr;
                    if (phase > 1)
                        phase = 1;
                    if (phase < 0)
                        phase = 0;
                    
                    dphase = (double)(phase * (t_float)(COSTABSIZE) * 0.99999) + UNITBIT32;
                    tf.tf_d = dphase;
                    addr = tab + (tf.tf_i[HIOFFSET] & (COSTABSIZE-1));
                    tf.tf_i[HIOFFSET] = normhipart;
                    frac = tf.tf_d - UNITBIT32;
                    f1 = addr[0];
                    f2 = addr[1];
                    
                    for(i = 0, j = x->x_ch * 2 - 1; i < x->x_ch; i++, j--)
                        *x->x_outs[j]++ = *x->x_ins[i]++ * (f1 + frac * (f2 - f1));
                    
                    continue;
                    }
                }
            incr = 0.;
            nleft = 0;
            last = f;
                    for(i = 0, j = x->x_ch * 2 - 1; i < x->x_ch; i++, j--)
                *x->x_outs[j]++ = *x->x_ins[i]++ * f;
            }
        
        else if (f != target){
            target = f;
            if (f != last){
                if (x->x_n > 1){
                    incr = (f - last) * x->x_coef;
                    nleft = x->x_n;
                    
                    phase = last += incr;
                    if (phase > 1)
                        phase = 1;
                    if (phase < 0)
                        phase = 0;
                    
                    dphase = (double)(phase * (t_float)(COSTABSIZE) * 0.99999) + UNITBIT32;
                    tf.tf_d = dphase;
                    addr = tab + (tf.tf_i[HIOFFSET] & (COSTABSIZE-1));
                    tf.tf_i[HIOFFSET] = normhipart;
                    frac = tf.tf_d - UNITBIT32;
                    f1 = addr[0];
                    f2 = addr[1];
                    
                    for(i = 0, j = x->x_ch * 2 - 1; i < x->x_ch; i++, j--)
                        *x->x_outs[j]++ = *x->x_ins[i]++ * (f1 + frac * (f2 - f1));
                    
                    continue;
                }
            }
	    incr = 0.;
	    nleft = 0;
        last = f;
                    for(i = 0, j = x->x_ch * 2 - 1; i < x->x_ch; i++, j--)
            *x->x_outs[j]++ = *x->x_ins[i]++ * f;
        }
        
        else if (nleft > 0){
            
            phase = last += incr;
            if (phase > 1)
                phase = 1;
            if (phase < 0)
                phase = 0;
            
            dphase = (double)(phase * (t_float)(COSTABSIZE) * 0.99999) + UNITBIT32;
            tf.tf_d = dphase;
            addr = tab + (tf.tf_i[HIOFFSET] & (COSTABSIZE-1));
            tf.tf_i[HIOFFSET] = normhipart;
            frac = tf.tf_d - UNITBIT32;
            f1 = addr[0];
            f2 = addr[1];
            
                    for(i = 0, j = x->x_ch * 2 - 1; i < x->x_ch; i++, j--)
                *x->x_outs[j]++ = *x->x_ins[i]++ * (f1 + frac * (f2 - f1));
            
            if (--nleft == 1){
                incr = 0.;
                last = target;
                }
            }
        else
                    for(i = 0, j = x->x_ch * 2 - 1; i < x->x_ch; i++, j--)
                *x->x_outs[j]++ = *x->x_ins[i]++ * target;
        };
    x->x_last = (PD_BIGORSMALL(last) ? 0. : last);
    x->x_target = (PD_BIGORSMALL(target) ? 0. : target);
    x->x_incr = incr;
    x->x_nleft = nleft;
    return (w + 4 + x->x_ch * 2);
}

static void autofade_dsp(t_autofade *x, t_signal **sp){
    x->x_sr_khz = sp[0]->s_sr * 0.001;
    int i, count = (x->x_ch * 2) + 3;
    t_int **sigvec;
    sigvec  = (t_int **) calloc(count, sizeof(t_int *));
    for(i = 0; i < count; i++)
        sigvec[i] = (t_int *) calloc(sizeof(t_int), 1);     // init sigvec
    sigvec[0] = (t_int *)x;                                 // 1st => object
    sigvec[1] = (t_int *)sp[0]->s_n;                        // 2nd => block (n)
    for(i = 2; i < count; i++)                              // ins/out
        sigvec[i] = (t_int *)sp[i-2]->s_vec;
    dsp_addv(autofade_perform, count, (t_int *)sigvec);
    free(sigvec);
}

static void *autofade_new(t_symbol *s, int argc, t_atom *argv){
    t_autofade *x = (t_autofade *)pd_new(autofade_class);
    x->x_sr_khz = sys_getsr() * 0.001;
    x->x_last = 0.;
    x->x_target = 0.;
    x->x_incr = 0.;
    x->x_nleft = 0;
    x->x_coef = 0.;
    x->x_table = autofade_table_quartic; // default
    t_int ch = 1; // default
    x->x_ms = 0; // default
/////////////////////////////////////////////////////////////////////////////////////
    if(argc){
        int argnum = 0;
        int sym_arg = 0;
        while(argc > 0){
            if(argv -> a_type == A_SYMBOL && !argnum){
                t_symbol *curarg = atom_getsymbolarg(0, argc, argv);
                if(!strcmp(curarg->s_name, "lin"))
                    x->x_table = autofade_table_lin;
                else if(!strcmp(curarg->s_name, "linsin"))
                    x->x_table = autofade_table_linsin;
                else if(!strcmp(curarg->s_name, "sqrt"))
                    x->x_table = autofade_table_sqrt;
                else if(!strcmp(curarg->s_name, "sin"))
                    x->x_table = autofade_table_sin;
                else if(!strcmp(curarg->s_name, "hannsin"))
                    x->x_table = autofade_table_hannsin;
                else if(!strcmp(curarg->s_name, "linsin"))
                    x->x_table = autofade_table_linsin;
                else if(!strcmp(curarg->s_name, "hann"))
                    x->x_table = autofade_table_hann;
                else
                    goto errstate;
                sym_arg = 1;
            }
            else if(argv -> a_type == A_FLOAT){ // if current argument is a float
                t_float argval = atom_getfloatarg(0, argc, argv);
                switch(argnum - sym_arg){
                    case 0:
                        ch = (int)argval;
                        break;
                    case 1:
                        x->x_ms = argval;
                        break;
                    default:
                        goto errstate;
                };
            }
            else
                goto errstate;
            argnum++;
            argc--;
            argv++;
        }
    }
/////////////////////////////////////////////////////////////////////////////////////
    if(ch < 1)
        ch = 1;
    if(ch > 64)
        ch = 64;
    x->x_ch = ch;
    t_int i;
    for(i = 0; i < x->x_ch; i++)
        inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    for(i = 0; i < x->x_ch; i++)
        outlet_new((t_object *)x, &s_signal);
    return (x);
    errstate:
        pd_error(x, "autofade~: improper args");
        return NULL;
}

static void autofade_tilde_maketable(void){
    int i;
    t_float *fp, phase1, phase2, fff;
    t_float phlinc = 1.0 / ((t_float)COSTABSIZE*0.99999);
    t_float phsinc = HALF_PI * phlinc;
    union tabfudge_d tf;
    
    if(!autofade_table_sin)
    {
        autofade_table_sin = (t_float *)getbytes(sizeof(t_float) * (COSTABSIZE+1));
        for(i=COSTABSIZE+1, fp=autofade_table_sin, phase1=0; i--; fp++, phase1+=phsinc)
            *fp = sin(phase1);
    }
    if(!autofade_table_hannsin)
    {
        autofade_table_hannsin = (t_float *)getbytes(sizeof(t_float) * (COSTABSIZE+1));
        for(i=COSTABSIZE+1, fp=autofade_table_hannsin, phase1=0; i--; fp++, phase1+=phsinc)
        {
            fff = sin(phase1);
            *fp = fff*sqrt(fff);
        }
    }
    if(!autofade_table_hann)
    {
        autofade_table_hann = (t_float *)getbytes(sizeof(t_float) * (COSTABSIZE+1));
        for(i=COSTABSIZE+1, fp=autofade_table_hann, phase1=0; i--; fp++, phase1+=phsinc)
        {
            fff = sin(phase1);
            *fp = fff*fff;
        }
    }
    if(!autofade_table_lin)
    {
        autofade_table_lin = (t_float *)getbytes(sizeof(t_float) * (COSTABSIZE+1));
        for(i=COSTABSIZE+1, fp=autofade_table_lin, phase1=0; i--; fp++, phase1+=phlinc)
            *fp = phase1;
    }
    if(!autofade_table_linsin)
    {
        autofade_table_linsin = (t_float *)getbytes(sizeof(t_float) * (COSTABSIZE+1));
        for(i=COSTABSIZE+1, fp=autofade_table_linsin, phase1=phase2=0; i--; fp++, phase1+=phlinc, phase2+=phsinc)
            *fp = sqrt(phase1 * sin(phase2));
    }
    if(!autofade_table_quartic)
    {
        autofade_table_quartic = (t_float *)getbytes(sizeof(t_float) * (COSTABSIZE+1));
        for(i=COSTABSIZE+1, fp=autofade_table_quartic, phase1=0; i--; fp++, phase1+=phlinc)
            *fp = pow(phase1, 4);
    }
    if(!autofade_table_sqrt)
    {
        autofade_table_sqrt = (t_float *)getbytes(sizeof(t_float) * (COSTABSIZE+1));
        for(i=COSTABSIZE+1, fp=autofade_table_sqrt, phase1=0; i--; fp++, phase1+=phlinc)
            *fp = sqrt(phase1);
    }
    tf.tf_d = UNITBIT32 + 0.5;
    if((unsigned)tf.tf_i[LOWOFFSET] != 0x80000000)
        bug("autofade~: unexpected machine alignment");
}


void autofade_tilde_setup(void){
    autofade_class = class_new(gensym("autofade~"), (t_newmethod)autofade_new, 0,
                    sizeof(t_autofade), 0, A_GIMME, 0);
    CLASS_MAINSIGNALIN(autofade_class, t_autofade, x_f);
    class_addmethod(autofade_class, (t_method) autofade_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(autofade_class, (t_method)autofade_lin, gensym("lin"), 0);
    class_addmethod(autofade_class, (t_method)autofade_linsin, gensym("linsin"), 0);
    class_addmethod(autofade_class, (t_method)autofade_sqrt, gensym("sqrt"), 0);
    class_addmethod(autofade_class, (t_method)autofade_sin, gensym("sin"), 0);
    class_addmethod(autofade_class, (t_method)autofade_hannsin, gensym("hannsin"), 0);
    class_addmethod(autofade_class, (t_method)autofade_hann, gensym("hann"), 0);
    class_addmethod(autofade_class, (t_method)autofade_quartic, gensym("quartic"), 0);
    class_addmethod(autofade_class, (t_method)autofade_fade, gensym("fade"), A_DEFFLOAT, 0);
    autofade_tilde_maketable();
}
