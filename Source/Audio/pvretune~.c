// Modified version of fftease/pvtuner~ by Alex Porres, 2026
// see fftease license in LICENSE folder

#include "fftease/fftease.h"

#define MAXTONES 8192

static t_class *pvretune_class;

typedef struct{
    t_float *pitchgrid;
    int      n_freqs;
}t_pvretune_scale;

typedef struct _pvretune{
    t_object    x_obj;
    t_fftease  *x_fft;
    t_int       x_nchs, x_n, x_sr;
    t_float     x_lofreq, x_hifreq;
    int         x_bypass, x_hop;
    int         x_size, x_overlap;  // fft args
    t_float     x_transp, x_thresh, x_interp; // other args
    t_pvretune_scale *x_this_scale, *x_last_scale;
    t_symbol   *x_ignore;
}t_pvretune;

void pvretune_setFFT(t_pvretune *x){
    for(int ch = 0; ch < x->x_nchs; ch++){
        t_fftease *fft = &x->x_fft[ch];
        fft->initialized = 0;
        fft->winfac = 1;
        fft->N = x->x_size;
        fft->overlap = x->x_overlap;
        fft->MSPVectorSize = x->x_n;
        fft->R = x->x_sr;
        fftease_set_fft_buffers(fft);
        fftease_init(fft);
        fftease_oscbank_setbins(fft, x->x_lofreq, x->x_hifreq);
        x->x_hop = fft->D;
    }
}

void pvretune_copy_scale(t_pvretune *x){
    int n = x->x_last_scale->n_freqs = x->x_this_scale->n_freqs;
    for(int i = 0; i < n; i++) // Copy this scale to last
        x->x_last_scale->pitchgrid[i] = x->x_this_scale->pitchgrid[i];
}

void pvretune_list(t_pvretune *x, t_symbol *sym, short ac, t_atom *av){
    if(ac <= 1)
        return;
    x->x_ignore = sym;
    if(atom_getfloat(av) == 0.0){
        pd_error(x, "[pvretune~] scale list can't have 0 as the 1st value");
        return;
    }
    pvretune_copy_scale(x); // copy this to last scale
    for(int i = 0; i < MAXTONES; i++) // set values to maximum
        x->x_this_scale->pitchgrid[i] = x->x_sr / 2.0;
    if(ac > MAXTONES)
        ac = MAXTONES;
    for(int i = 0; i < ac; i++) // read scale (should be sorted)
        x->x_this_scale->pitchgrid[i] = atom_getfloatarg(i, ac, av);
    x->x_this_scale->n_freqs = ac;
}

void pvretune_fft(t_pvretune *x, t_floatarg n, t_floatarg o){
    x->x_size = n, x->x_overlap = o;
    pvretune_setFFT(x);
}

void pvretune_bypass(t_pvretune *x, t_floatarg f){
    x->x_bypass = (int)(f != 0) ;
}

void pvretune_interp(t_pvretune *x, t_floatarg f){
    x->x_interp = f < 0 ? 0 : f > 1 ? 1 : f; // interpolation point
}

void pvretune_transp(t_pvretune *x, t_floatarg f){
    x->x_transp = f < 0 ? 0 : f;
}

void pvretune_thresh(t_pvretune *x, t_floatarg f){
    x->x_thresh = f < 0 ? 0 : f > 1 ? 1 : f;
}

void pvretune_frange(t_pvretune *x, t_floatarg lo, t_floatarg hi){
    x->x_lofreq = lo, x->x_hifreq = hi;
    if(x->x_hifreq >= x->x_sr / 2)
        x->x_hifreq = x->x_sr / 2;
    if(lo >= hi){
        pd_error(x, "[pvretune~] low frequency must be lower than high frequency");
        return;
    }
    pvretune_setFFT(x);
}

t_float closestf(t_float test, t_float *arr){
    int i = 0;
    if(test <= arr[0])
        return arr[0];
    while(i < MAXTONES){
        if(arr[i] > test)
            break;
        ++i;
    }
    if(i >= MAXTONES - 1)
        return arr[MAXTONES - 1];
    if((test - arr[i-1]) > (arr[i] - test))
        return(arr[i]);
    else
        return(arr[i-1]);
}

static void do_pvretune(t_pvretune *x, t_fftease *fft){
    t_float *channel = fft->channel;
    t_float *this_grid = x->x_this_scale->pitchgrid;
    t_float *last_grid = x->x_last_scale->pitchgrid;
    fftease_fold(fft);
    fftease_rdft(fft, 1);
    fftease_convert(fft);
    for(int bin = fft->lo_bin; bin < fft->hi_bin; bin++){
        int freq = (bin * 2) + 1;
        t_float retuned;
        if(x->x_interp == 0)
            retuned = closestf(channel[freq], last_grid);
        else if(x->x_interp == 1)
            retuned = closestf(channel[freq], this_grid);
        else{ // interpolation
            t_float this_f = closestf(channel[freq], this_grid);
            t_float last_f = closestf(channel[freq], last_grid);
            retuned = last_f + (this_f - last_f) * x->x_interp;
        }
        channel[freq] = retuned;
    }
    fftease_oscbank(fft);
}

t_int *pvretune_perform(t_int *w){
    t_pvretune *x = (t_pvretune *)(w[1]);
    t_float *input = (t_float *)(w[2]);
    t_float *output = (t_float *)(w[3]);
    int n = x->x_n;
    if(x->x_bypass){
        for(int i = 0; i < n*x->x_nchs; i++)
            output[i] = input[i];
        return(w+4);
    }
    for(int ch = 0; ch < x->x_nchs; ch++){
        t_fftease *fft = &x->x_fft[ch];
        t_float *in = input + ch*n;
        t_float *out = output + ch*n;
        fft->P = x->x_transp, fft->synt = x->x_thresh;
        int size = x->x_size, hop = x->x_hop;
        int op = fft->operationCount, rep = fft->operationRepeat;
        memcpy(fft->internalInputVector + op*n, in, n*sizeof(t_float));
        memcpy(out, fft->internalOutputVector + op*n, n*sizeof(t_float));
        op = ((op + 1) % rep);
        if(op == 0){
            memcpy(fft->input, fft->input + hop, (size-hop)*sizeof(t_float));
            memcpy(fft->input + (size-hop), fft->internalInputVector, hop*sizeof(t_float));
            do_pvretune(x, fft); // DO IT!!!
            for(int i = 0; i < hop; i++)
                fft->internalOutputVector[i] = fft->output[i] * fft->mult;
            memcpy(fft->output, fft->output + hop, (size-hop)*sizeof(t_float));
            memset(fft->output + (size-hop), 0, hop*sizeof(t_float));
        }
        fft->operationCount = op;
    }
    return(w+4);
}

void pvretune_dsp(t_pvretune *x, t_signal **sp){
    int n = sp[0]->s_n, sr = sp[0]->s_sr, chs = sp[0]->s_nchans;
    int reset = (x->x_sr != sr || x->x_n != n);
    x->x_sr = sr, x->x_n = n;
    if(x->x_nchs != chs){
        x->x_fft = (t_fftease *)resizebytes(x->x_fft,
            x->x_nchs * sizeof(t_fftease), chs * sizeof(t_fftease));
        x->x_nchs = chs, reset = 1;
    }
    if(reset)
        pvretune_setFFT(x);
    signal_setmultiout(&sp[1], x->x_nchs);
    if(x->x_n > x->x_hop){
        pd_error(x, "[pvretune~]: hop size can't be smaller than the block size");
        dsp_add_zero(sp[1]->s_vec, x->x_nchs*n);
    }
    dsp_add(pvretune_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
}

void pvretune_free(t_pvretune *x){
    free(x->x_this_scale->pitchgrid);
    free(x->x_last_scale->pitchgrid);
    free(x->x_this_scale);
    free(x->x_last_scale);
    for(int ch = 0; ch < x->x_nchs; ch++)
        fftease_free(&x->x_fft[ch]);
    free(x->x_fft);
}

void pvretune_def_scale(t_pvretune *x){ // EQ12
    int i, j, steps = 12, octaves = 10;
    t_float *pitchgrid = x->x_this_scale->pitchgrid;
    pitchgrid[0] = 27.5; // low A
    for(i = 1; i < steps; i++)
        pitchgrid[i] = pitchgrid[0] * pow(2.0, (float)i / (float)steps);
    for(i = 1; i < octaves; i++){
        for(j = 0; j < steps; j++)
            pitchgrid[i*steps + j] = pitchgrid[j] * pow(2.0, (float)i);
    }
    x->x_this_scale->n_freqs = steps * octaves;
}

void *pvretune_new(t_symbol *s, int ac, t_atom *av){
    t_pvretune *x = (t_pvretune *)pd_new(pvretune_class);
    x->x_ignore = s;
    x->x_fft = (t_fftease *)calloc(1, sizeof(t_fftease));
    x->x_lofreq = 0, x->x_hifreq = 18000;
    x->x_size = 2048;   // DEFAULT_FFTEASE_FFTSIZE
    x->x_overlap = 8;   // FFTEASE_DEFAULT_OVERLAP
    x->x_hop = x->x_size / x->x_overlap;
    x->x_n = 64;
    x->x_nchs = 1;
    x->x_sr = sys_getsr();
    t_float transp = 1, thresh = 0, interp = 1;
    x->x_bypass = 0;
    if(ac > 0)
        x->x_size = atom_getint(av);
    if(ac > 1)
        x->x_overlap = atom_getintarg(1, ac, av);
    if(ac > 2)
        transp = atom_getfloatarg(2, ac, av);
    if(ac > 3)
        thresh = atom_getfloatarg(3, ac, av);
    if(ac > 4)
        interp = atom_getfloatarg(4, ac, av);
    int mem = (MAXTONES+1) * sizeof(float);
    x->x_this_scale = (t_pvretune_scale *)calloc(1, sizeof(t_pvretune_scale));
    x->x_last_scale = (t_pvretune_scale *)calloc(1, sizeof(t_pvretune_scale));
    x->x_this_scale->pitchgrid = (t_float*)calloc(1, mem);
    x->x_last_scale->pitchgrid = (t_float*)calloc(1, mem);
    pvretune_def_scale(x);  // set EQ12 as the default scale
    pvretune_copy_scale(x); // copy default to last scale
    pvretune_setFFT(x);
    pvretune_transp(x, transp);
    pvretune_thresh(x, thresh);
    pvretune_interp(x, interp);
    outlet_new((t_object *)x, &s_signal);
    return(x);
}

void pvretune_tilde_setup(void){
    pvretune_class = class_new(gensym("pvretune~"), (t_newmethod)(void*)pvretune_new,
        (t_method)pvretune_free, sizeof(t_pvretune), CLASS_MULTICHANNEL, A_GIMME, 0);
    class_addmethod(pvretune_class, nullfn, gensym("signal"), 0);
    class_addmethod(pvretune_class,(t_method)pvretune_dsp, gensym("dsp"), A_CANT, 0);
    class_addmethod(pvretune_class,(t_method)pvretune_list, gensym("list"), A_GIMME, 0);
    class_addmethod(pvretune_class,(t_method)pvretune_frange, gensym("frange"),
        A_FLOAT, A_FLOAT, 0);
    class_addmethod(pvretune_class,(t_method)pvretune_fft, gensym("fft"), A_FLOAT, A_FLOAT, 0);
    class_addmethod(pvretune_class,(t_method)pvretune_bypass, gensym("bypass"), A_FLOAT, 0);
    class_addmethod(pvretune_class,(t_method)pvretune_transp, gensym("transp"), A_FLOAT, 0);
    class_addmethod(pvretune_class,(t_method)pvretune_thresh, gensym("thresh"), A_FLOAT, 0);
    class_addmethod(pvretune_class,(t_method)pvretune_interp, gensym("interp"), A_FLOAT, 0);
}
