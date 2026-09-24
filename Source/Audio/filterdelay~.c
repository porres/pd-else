// porres 2023, inspired by and modified from Tom Erbe's [+delay~]

#include <math.h>
#include <string.h>
#include <m_pd.h>
#include <buffer.h>

#define DELSIZE 1048576

static t_class *fdelay_class;

typedef struct _fdelay{
    t_object    x_obj;
    t_inlet    *x_delt_in;
    t_inlet    *x_fb_in;
    float       x_sr;
    long       *x_readPos;
    long       *x_writePos;
    long        x_delaySize;
    long        x_delayMask;
    float      *x_delay;
    float       x_cutoff;
    float       x_reson;
    float       x_xfade;
    int         x_freeze;
    float      *X1;
    float      *X2;
    float      *Y1;
    float      *Y2;
    float      *dcIn;
    float      *dcOut;
    int         x_nchans;
    int         x_deltchans;
    int         x_fbchans;
}t_fdelay;

static void fdelay_cutoff(t_fdelay *x, t_floatarg f){
    x->x_cutoff = f < 20.0 ? 20.0 : f > 20000.0 ? 20000.0 : f;
}

static void fdelay_reson(t_fdelay *x, t_floatarg f){
    x->x_reson = f < 0 ? 0 : f > 1 ? 1 : f;
}

static void fdelay_freeze(t_fdelay *x, t_floatarg f){
    x->x_freeze = (int)(f != 0);
}

static void fdelay_wet(t_fdelay *x, t_floatarg f){
    x->x_xfade = (f < 0 ? 0 : f > 1 ? 1 : f) * M_PI * 0.5;
}

static void fdelay_clear(t_fdelay *x){
    memset(x->x_delay, 0, x->x_nchans * DELSIZE * sizeof(*x->x_delay));
    for(int j = 0; j < x->x_nchans; j++){
        x->x_readPos[j] = x->x_writePos[j] = 0;
        x->X1[j] = x->X2[j] = x->Y1[j] = x->Y2[j] = 0.0;
        x->dcOut[j] = x->dcIn[j] = 0.0f;
    }
}

static t_int *fdelay_perform(t_int *w){
    t_fdelay *x = (t_fdelay *)(w[1]);
    t_float *input = (t_float *)(w[2]);
    t_float *tin = (t_float *)(w[3]);
    t_float *fbin = (t_float *)(w[4]);
    t_float *out = (t_float *)(w[5]);
    int n = (int)(w[6]);
    float dcR = 1.0f - (126.0f / x->x_sr);
    float cutoff = x->x_cutoff;
    float filterQ = (1.0 - x->x_reson) * (sqrt(2.0) - 0.1) + 0.1;
    if(cutoff > x->x_sr * 0.5)
        cutoff = x->x_sr * 0.5;
    float f0 = cutoff / x->x_sr;
    float C = (f0 < 0.1) ? 1.0 / (f0 * M_PI) : tan((0.5 - f0) * M_PI);
    float A1 = 1.0 / (1.0 + filterQ * C + C * C);
    float A2 = 2.0 * A1;
    float A3 = A1;
    float B1 = 2.0 * (1.0 - C * C) * A1;
    float B2 = (1.0 - filterQ * C + C * C) * A1;
    for(int j = 0; j < x->x_nchans; j++){
        t_float *in = input + j*n;
        t_float *timein = x->x_deltchans == 1 ? tin : tin + j*n;
        t_float *fbinch = x->x_fbchans == 1 ? fbin : fbin + j*n;
        t_float *outch = out + j*n;
        float *delay = x->x_delay + j*DELSIZE;
        long readPos = x->x_readPos[j];
        long writePos = x->x_writePos[j];
        float X1 = x->X1[j];
        float X2 = x->X2[j];
        float Y1 = x->Y1[j];
        float Y2 = x->Y2[j];
        float dcIn = x->dcIn[j];
        float dcOut = x->dcOut[j];
        for(int i = 0; i < n; i++){
            t_float dry = in[i];
            t_float input_sample = dry;
            t_float time = timein[i];
            t_float fb = fbinch[i];
            if(x->x_freeze){
                input_sample = 0;
                fb = fb >= 0 ? 1 : -1;
            }
            float delayTime = time * x->x_sr / 1000;
            if(delayTime < 1)
                delayTime = 1;
            if(delayTime >= DELSIZE - 2)
                delayTime = DELSIZE - 2;
            float delayTimeL = delayTime + 2.0;
            long delayLong = (long)delayTimeL;
            float frac = 1.0 - (delayTimeL - (float)delayLong);
            readPos = writePos - delayLong;
            readPos &= x->x_delayMask;
            float a = delay[(readPos - 1) & x->x_delayMask];
            float b = delay[(readPos + 0) & x->x_delayMask];
            float c = delay[(readPos + 1) & x->x_delayMask];
            float d = delay[(readPos + 2) & x->x_delayMask];
            float del = interp_spline(frac, a, b, c, d);
            float filter = A1*del + A2*X1 + A3*X2 - B1*Y1 - B2*Y2;
            X2 = X1;
            X1 = del;
            Y2 = Y1;
            Y1 = filter;
            if(x->x_cutoff >= 20000)
                filter = del;
            if(fabs(fb) > 1.0f){
                float dcOut0 = filter - dcIn + dcR * dcOut;
                dcIn = filter;
                dcOut = filter = dcOut0;
            }
            float wet = atan(filter);
            delay[writePos] = input_sample + wet * fb;
            outch[i] = dry * cos(x->x_xfade) + wet * sin(x->x_xfade);
            writePos++;
            writePos &= x->x_delayMask;
        }
        x->x_readPos[j] = readPos;
        x->x_writePos[j] = writePos;
        x->X1[j] = X1;
        x->X2[j] = X2;
        x->Y1[j] = Y1;
        x->Y2[j] = Y2;
        x->dcIn[j] = dcIn;
        x->dcOut[j] = dcOut;
    }
    return(w + 7);
}

static void fdelay_dsp(t_fdelay *x, t_signal **sp){
    int chs = sp[0]->s_nchans;
    x->x_deltchans = sp[1]->s_nchans;
    x->x_fbchans = sp[2]->s_nchans;
    if(x->x_nchans != chs){
        x->x_delay = (float *)resizebytes(x->x_delay,
            x->x_nchans * DELSIZE * sizeof(*x->x_delay),
            chs * DELSIZE * sizeof(*x->x_delay));
        x->x_readPos = (long *)resizebytes(x->x_readPos,
            x->x_nchans * sizeof(*x->x_readPos),
            chs * sizeof(*x->x_readPos));
        x->x_writePos = (long *)resizebytes(x->x_writePos,
            x->x_nchans * sizeof(*x->x_writePos),
            chs * sizeof(*x->x_writePos));
        x->X1 = (float *)resizebytes(x->X1,
            x->x_nchans * sizeof(*x->X1), chs * sizeof(*x->X1));
        x->X2 = (float *)resizebytes(x->X2,
            x->x_nchans * sizeof(*x->X2), chs * sizeof(*x->X2));
        x->Y1 = (float *)resizebytes(x->Y1,
            x->x_nchans * sizeof(*x->Y1), chs * sizeof(*x->Y1));
        x->Y2 = (float *)resizebytes(x->Y2,
            x->x_nchans * sizeof(*x->Y2), chs * sizeof(*x->Y2));
        x->dcIn = (float *)resizebytes(x->dcIn,
            x->x_nchans * sizeof(*x->dcIn), chs * sizeof(*x->dcIn));
        x->dcOut = (float *)resizebytes(x->dcOut,
            x->x_nchans * sizeof(*x->dcOut), chs * sizeof(*x->dcOut));
        for(int j = x->x_nchans; j < chs; j++){
            x->x_readPos[j] = x->x_writePos[j] = 0;
            x->X1[j] = x->X2[j] = x->Y1[j] = x->Y2[j] = 0;
            x->dcIn[j] = x->dcOut[j] = 0;
        }
        x->x_nchans = chs;
        fdelay_clear(x);
    }
    x->x_sr = sp[0]->s_sr;
    signal_setmultiout(&sp[3], x->x_nchans);
    if((x->x_deltchans > 1 && x->x_deltchans != x->x_nchans)
    || (x->x_fbchans > 1 && x->x_fbchans != x->x_nchans)){
        dsp_add_zero(sp[3]->s_vec, x->x_nchans * sp[3]->s_n);
        pd_error(x, "[filterdelay~]: channel sizes mismatch");
        return;
    }
    dsp_add(fdelay_perform, 6, x, sp[0]->s_vec, sp[1]->s_vec,
        sp[2]->s_vec, sp[3]->s_vec, sp[0]->s_n);
}

static void *fdelay_free(t_fdelay *x){
    freebytes(x->x_delay, x->x_nchans * DELSIZE * sizeof(*x->x_delay));
    freebytes(x->x_readPos, x->x_nchans * sizeof(*x->x_readPos));
    freebytes(x->x_writePos, x->x_nchans * sizeof(*x->x_writePos));
    freebytes(x->X1, x->x_nchans * sizeof(*x->X1));
    freebytes(x->X2, x->x_nchans * sizeof(*x->X2));
    freebytes(x->Y1, x->x_nchans * sizeof(*x->Y1));
    freebytes(x->Y2, x->x_nchans * sizeof(*x->Y2));
    freebytes(x->dcIn, x->x_nchans * sizeof(*x->dcIn));
    freebytes(x->dcOut, x->x_nchans * sizeof(*x->dcOut));
    inlet_free(x->x_delt_in);
    inlet_free(x->x_fb_in);
    return(void *)x;
}

static void *fdelay_new(t_symbol *s, int ac, t_atom *av){
    t_fdelay *x = (t_fdelay *)pd_new(fdelay_class);
    float time = 0, fb = 0, cutoff = 20000.0, reson = 0.0, wet = 0.5;
    x->x_freeze = 0;
    x->x_sr = sys_getsr();
    if(ac){
        while(av->a_type == A_SYMBOL){
            s = atom_getsymbol(av);
            if(s == gensym("-cutoff")){
                ac--, av++;
                cutoff = atom_getfloat(av);
            }
            else if(s == gensym("-reson")){
                ac--, av++;
                reson = atom_getfloat(av);
            }
            else if(s == gensym("-wet")){
                ac--, av++;
                wet = atom_getfloat(av);
            }
            ac--, av++;
        }
        time = atom_getfloat(av);
        ac--, av++;
        fb = atom_getfloat(av);
    }
    x->x_cutoff = cutoff < 20 ? 20 : cutoff > 20000 ? 20000 : cutoff;
    x->x_reson = reson < 0 ? 0 : reson > 1 ? 1 : reson;
    fdelay_wet(x, wet);
    x->x_delaySize = DELSIZE;
    x->x_delayMask = DELSIZE - 1;
    x->x_nchans = 1;
    x->x_deltchans = 1;
    x->x_fbchans = 1;
    x->x_delay = (float *)getbytes(DELSIZE * sizeof(*x->x_delay));
    x->x_readPos = (long *)getbytes(sizeof(*x->x_readPos));
    x->x_writePos = (long *)getbytes(sizeof(*x->x_writePos));
    x->X1 = (float *)getbytes(sizeof(*x->X1));
    x->X2 = (float *)getbytes(sizeof(*x->X2));
    x->Y1 = (float *)getbytes(sizeof(*x->Y1));
    x->Y2 = (float *)getbytes(sizeof(*x->Y2));
    x->dcIn = (float *)getbytes(sizeof(*x->dcIn));
    x->dcOut = (float *)getbytes(sizeof(*x->dcOut));
    x->x_delt_in = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_delt_in, time);
    x->x_fb_in = inlet_new((t_object *)x, (t_pd *)x, &s_signal, &s_signal);
    pd_float((t_pd *)x->x_fb_in, fb);
    outlet_new(&x->x_obj, &s_signal);
    fdelay_clear(x);
    return(void *)x;
}

void filterdelay_tilde_setup(void){
    fdelay_class = class_new(gensym("filterdelay~"), (t_newmethod)fdelay_new,
        (t_method)fdelay_free, sizeof(t_fdelay), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(fdelay_class, nullfn, gensym("signal"), 0);
    class_addmethod(fdelay_class, (t_method)fdelay_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(fdelay_class, (t_method)fdelay_clear, gensym("clear"), 0);
    class_addmethod(fdelay_class, (t_method)fdelay_cutoff, gensym("cutoff"), A_DEFFLOAT, 0);
    class_addmethod(fdelay_class, (t_method)fdelay_reson, gensym("reson"), A_DEFFLOAT, 0);
    class_addmethod(fdelay_class, (t_method)fdelay_freeze, gensym("freeze"), A_DEFFLOAT, 0);
    class_addmethod(fdelay_class, (t_method)fdelay_wet, gensym("wet"), A_DEFFLOAT, 0);
}
