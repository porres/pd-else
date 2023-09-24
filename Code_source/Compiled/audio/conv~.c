// based on convolve~ by William Brent under the GNU General Public License

#include "m_pd.h"
#include "kiss_fft.h"
#include <math.h>

#define MINSIZE 64
#define DEFSIZE 256

typedef struct _conv{
    t_object        x_obj;
    t_symbol       *x_array_name;
    t_word         *x_vec;
    t_clock        *x_clock;
    t_sample       *x_ir_signal;
    int             x_startup_flag;
    int             x_array_size;
    int             x_num_parts;
    t_float         x_n;
    int             x_dsp_tick;
    int             x_buffer_limit;
    int             x_window;
    int             x_window_double;
    t_float         x_amp_scalar;
    t_sample       *x_signal_buf;
    t_sample       *x_signal_buf_padded;
    t_sample       *x_inv_out_fft_out;
    t_sample       *x_non_overlapped_output;
    t_sample       *x_final_output;
    kiss_fft_cpx   *x_ir_freq_dom_data;
    kiss_fft_cpx   *x_live_freq_dom_data;
    kiss_fft_cpx   *x_sig_buf_pad_fft_out;
    kiss_fft_cpx   *x_inv_out_fft_in;
    kiss_fftr_cfg   x_sig_buf_pad_fft_plan;
    kiss_fftr_cfg   x_inv_out_fft_plan;
}t_conv;

static t_class *conv_class;

static t_int *conv_perform(t_int *w){
    int i, p;
    t_conv *x = (t_conv *)(w[1]);
    t_sample *in = (t_float *)(w[2]);
    t_sample *out = (t_float *)(w[3]);
    int window = x->x_window, window_double = x->x_window_double;
    int num_parts = x->x_num_parts, n = x->x_n;
    if(x->x_num_parts < 1){
        for(i = 0; i < n; i++)
            out[i] = in[i];
        return(w+4);
    }
    t_float amp_scalar = x->x_amp_scalar;
    // buffer most recent block
    for(i = 0; i < n; i++)
        x->x_signal_buf[(x->x_dsp_tick*n)+i] = in[i];
    if(++x->x_dsp_tick >= x->x_buffer_limit){
        x->x_dsp_tick = 0;
        for(i = 0; i < window; i++)  // copy signal buffer into the transform buffer
            x->x_signal_buf_padded[i] = x->x_signal_buf[i];
        for(; i < window_double; i++) // pad the rest out with zeros
            x->x_signal_buf_padded[i] = 0.0;
        // FT of most recent input, padded to double window size
        kiss_fftr(x->x_sig_buf_pad_fft_plan, x->x_signal_buf_padded, x->x_sig_buf_pad_fft_out);
        // multiply with partitioned IR spectra and sum into appropriate part of the nonoverlapped buffer
        for(p = 0; p < num_parts; p++){
            int startIdx = p*(window+1);
            for(i = 0; i < (window+1); i++){
                t_float realLive = x->x_sig_buf_pad_fft_out[i].r;
                t_float imagLive = x->x_sig_buf_pad_fft_out[i].i;
                t_float realIR = x->x_ir_freq_dom_data[startIdx+i].r;
                t_float imagIR = x->x_ir_freq_dom_data[startIdx+i].i;
                // MINUS the imag part because i^2 = -1
                t_float real = (realLive * realIR) - (imagLive * imagIR);
                t_float imag = (imagLive * realIR) + (realLive * imagIR);
                // sum into the live freq domain data buffer
                x->x_live_freq_dom_data[startIdx+i].r += real;
                x->x_live_freq_dom_data[startIdx+i].i += imag;
            }
        }
        // copy FT data from head of the complex summing buffer into the inverse FT buffer
        for(i = 0; i < (window+1); i++){
            x->x_inv_out_fft_in[i].r = x->x_live_freq_dom_data[i].r;
            x->x_inv_out_fft_in[i].i = x->x_live_freq_dom_data[i].i;
        }
        // perform ifft
        kiss_fftri(x->x_inv_out_fft_plan, x->x_inv_out_fft_in, x->x_inv_out_fft_out);
        // copy the latest IFFT time-domain result into the SECOND block of x_non_overlapped_output. The first block contains time-domain results from last time, which we will overlap-add with
        for(i = 0; i < window_double; i++)
            x->x_non_overlapped_output[window_double+i] = x->x_inv_out_fft_out[i];
        // write time domain output to x->x_final_output and reduce gain
        for(i = 0; i < window; i++){
            x->x_final_output[i] = x->x_non_overlapped_output[window+i] +  x->x_non_overlapped_output[window_double+i];
            x->x_final_output[i] *= amp_scalar;
        }
        // push the live freq domain data buffer contents backwards
        for(i = 0; i < ((num_parts*(window+1))-(window+1)); i++){
            x->x_live_freq_dom_data[i].r = x->x_live_freq_dom_data[(window+1)+i].r;
            x->x_live_freq_dom_data[i].i = x->x_live_freq_dom_data[(window+1)+i].i;
        }
        // init the newly available chunk at the end
        for(; i < (num_parts*(window+1)); i++){
            x->x_live_freq_dom_data[i].r = 0.0;
            x->x_live_freq_dom_data[i].i = 0.0;
        }
        // push remaining output buffer contents backwards
        for(i = 0; i < window_double; i++)
            x->x_non_overlapped_output[i] = x->x_non_overlapped_output[window_double+i];
        // init the newly available chunk at the end
        for(; i < (2*window_double); i++)
            x->x_non_overlapped_output[i] = 0.0;
    };
    for(i = 0; i < n; i++) // output
        out[i] = x->x_final_output[(x->x_dsp_tick*n)+i];
    return(w+4);
}

static void conv_dsp(t_conv *x, t_signal **sp){
    if(sp[0]->s_n != x->x_n){
        pd_error(x, "[conv~]: block size must be 64");
        dsp_add_zero(sp[1]->s_vec, x->x_n);
    }
    else
        dsp_add(conv_perform, 3, x, sp[0]->s_vec, sp[1]->s_vec);
};

static void conv_set(t_conv *x, t_symbol *arrayName){
    t_garray *array_ptr;
    t_float *fft_in;
    int i, j, old_non_overlapped_size, new_non_overlapped_size;
    kiss_fft_cpx *fft_out;
    kiss_fftr_cfg fft_cfg;
    if(x->x_num_parts > 0)
        old_non_overlapped_size = 2*x->x_window_double;
    else
        old_non_overlapped_size = 0;
    // if incoming arrayName doesn't match x->x_array_name, load arrayName and dump its samples into x_ir_signal
    if(arrayName->s_name != x->x_array_name->s_name || x->x_startup_flag){
        int old_array_size;
        x->x_startup_flag = 0;
        old_array_size = x->x_array_size;
        if(!(array_ptr = (t_garray *)pd_findbyclass(arrayName, garray_class))){
            if(*arrayName->s_name){
                pd_error(x, "[conv~]: no array named %s", arrayName->s_name);
                // resize x_ir_signal back to 0
                x->x_ir_signal = (t_sample *)t_resizebytes( x->x_ir_signal, old_array_size*sizeof(t_sample), 0);
                x->x_array_size = 0;
                x->x_vec = 0;
                return;
            }
        }
        else if(!garray_getfloatwords(array_ptr, &x->x_array_size, &x->x_vec)){
            pd_error(x, "[conv~]: bad template for %s", arrayName->s_name);
            // resize x_ir_signal back to 0
            x->x_ir_signal = (t_sample *)t_resizebytes(x->x_ir_signal, old_array_size*sizeof(t_sample), 0);
            x->x_array_size = 0;
            x->x_vec = 0;
            return;
        }
        else
            x->x_array_name = arrayName;
        // resize x_ir_signal
        x->x_ir_signal = (t_sample *)t_resizebytes(x->x_ir_signal, old_array_size*sizeof(t_sample), x->x_array_size*sizeof(t_sample));
        // since this is first analysis of arrayName, load it into x_ir_signal
        for(i = 0; i < x->x_array_size; i++)
            x->x_ir_signal[i] = x->x_vec[i].w_float;
    }
    else{
        t_garray *this_array_ptr;
        t_word *this_vec;
        int this_array_size;
        // if we want to assume that a 2nd call to analyze() with the same array name is safe (and we don't have to reload x_vec or update x_array_size), we have to do some careful safety checks to make sure that the size of x_array_name hasn't changed since the last time this was called. If it has, we should just abort for now.
        this_array_size = 0;
        this_array_ptr = (t_garray *)pd_findbyclass(arrayName, garray_class);
        garray_getfloatwords(this_array_ptr, &this_array_size, &this_vec);
        if(this_array_size != x->x_array_size){
            pd_error(x, "[conv~]: size of array %s has changed since previous analysis...aborting. Reload %s with previous IR contents or analyze another array.", arrayName->s_name, arrayName->s_name);
            return;
        }
    }
    // count how many partitions there will be for this IR
    x->x_num_parts = 0;
    while((x->x_num_parts*x->x_window) < x->x_array_size)
        x->x_num_parts++;
    new_non_overlapped_size = 2*x->x_window_double;
    // resize time-domain buffer
    // this can probably just be x_window_double * 2!!
    x->x_non_overlapped_output = (t_sample *)t_resizebytes(
        x->x_non_overlapped_output,
        old_non_overlapped_size*sizeof(t_sample),
        new_non_overlapped_size*sizeof(t_sample)
    );
    // clear time-domain buffer
    for(i = 0; i < new_non_overlapped_size; i++)
        x->x_non_overlapped_output[i] = 0.0;
    // free x_ir_freq_dom_data/x_live_freq_dom_data and re-alloc to new size based on x_num_parts
    free(x->x_ir_freq_dom_data);
    free(x->x_live_freq_dom_data);
    x->x_ir_freq_dom_data = (kiss_fft_cpx*)malloc(x->x_num_parts*(x->x_window+1) * sizeof(kiss_fft_cpx));
    x->x_live_freq_dom_data = (kiss_fft_cpx*)malloc(x->x_num_parts*(x->x_window+1) * sizeof(kiss_fft_cpx));
    // clear x_live_freq_dom_data
    for(i = 0; i<x->x_num_parts*(x->x_window+1); i++){
        x->x_live_freq_dom_data[i].r = 0.0;
        x->x_live_freq_dom_data[i].i = 0.0;
    }
    // set up FFTW input buffer
    fft_in = (t_float *)t_getbytes(x->x_window_double * sizeof(t_float));
    // set up the FFTW output buffer. It is N/2+1 elements long for an N-point r2c FFT
    // fft_out[i].r and fft_out[i].i refer to the real and imaginary parts of bin i
    fft_out = (kiss_fft_cpx *)t_getbytes((x->x_window+1) * sizeof(kiss_fft_cpx));
    // FFT plan
    fft_cfg = kiss_fftr_alloc(x->x_window_double, 0, NULL, NULL);
    // we're supposed to initialize the input array after we create the plan
    for(i = 0; i < x->x_window_double; i++)
        fft_in[i] = 0.0;
    // take FFTs of partitions, and store in x_ir_freq_dom_data as chunks of
    // window+1 complex arrays
    for(i = 0; i < x->x_num_parts; i++){
        int start_spec, start_vec;
        start_spec = i*(x->x_window+1);
        start_vec = i*x->x_window;
        // we are analyzing partitions of x_ir_signal, in case there has been any EQ
        for(j = 0; j < x->x_window && (start_vec+j) < x->x_array_size; j++)
            fft_in[j] = x->x_ir_signal[start_vec+j];
        // zero pad
        for(; j < x->x_window_double; j++)
            fft_in[j] = 0.0;
        // perform FFT
        kiss_fftr(fft_cfg, fft_in, fft_out);
        // copy freq domain data from fft output buffer into
        // larger IR freq domain data buffer
        for(j = 0; j < x->x_window+1; j++){
            x->x_ir_freq_dom_data[start_spec+j].r = fft_out[j].r;
            x->x_ir_freq_dom_data[start_spec+j].i = fft_out[j].i;
        }
    }
    freebytes(fft_in, (x->x_window_double)*sizeof(t_float));
    freebytes(fft_out, (x->x_window+1)*sizeof(kiss_fft_cpx));
    kiss_fft_free(fft_cfg);
}

static void conv_size(t_conv *x, t_float w){
    int i, old_window = x->x_window, old_window_double = x->x_window_double;
    x->x_window = w < MINSIZE ? MINSIZE : (int)w;
    if((x->x_window % 64) != 0){ // window size is not a multiple of 64
        x->x_window = DEFSIZE;
        pd_error(x, "[conv~]: window not a multiple of 64, using default (%i) instead", x->x_window);
    }
    // resize time-domain buffer to zero bytes
    x->x_non_overlapped_output = (t_sample *)t_resizebytes(x->x_non_overlapped_output, (2*x->x_window_double)*sizeof(t_sample), 0);
    // update window-based terms
    x->x_window_double = x->x_window*2;
    x->x_amp_scalar = 1.0f/x->x_window_double;
    x->x_buffer_limit = x->x_window/x->x_n;
    // update window-based memory
    free(x->x_ir_freq_dom_data);
    free(x->x_live_freq_dom_data);
    free(x->x_sig_buf_pad_fft_out);
    free(x->x_inv_out_fft_in);
    x->x_ir_freq_dom_data = (kiss_fft_cpx*)malloc((x->x_window+1) * sizeof(kiss_fft_cpx));
    x->x_live_freq_dom_data = (kiss_fft_cpx*)malloc((x->x_window+1) * sizeof(kiss_fft_cpx));
    x->x_sig_buf_pad_fft_out = (kiss_fft_cpx*)malloc((x->x_window+1) * sizeof(kiss_fft_cpx));
    x->x_inv_out_fft_in = (kiss_fft_cpx*)malloc((x->x_window+1) * sizeof(kiss_fft_cpx));
    x->x_signal_buf = (t_sample *)t_resizebytes(x->x_signal_buf, old_window*sizeof(t_sample), x->x_window*sizeof(t_sample));
    x->x_final_output = (t_sample *)t_resizebytes(x->x_final_output, old_window*sizeof(t_sample), x->x_window*sizeof(t_sample));
    x->x_signal_buf_padded = (t_sample *)t_resizebytes(x->x_signal_buf_padded, old_window_double*sizeof(t_sample), x->x_window_double*sizeof(t_sample));
    x->x_inv_out_fft_out = (t_sample *)t_resizebytes(x->x_inv_out_fft_out, old_window_double*sizeof(t_sample), x->x_window_double*sizeof(t_sample));
    // signalBufPadded plan
    kiss_fft_free(x->x_sig_buf_pad_fft_plan);
    kiss_fft_free(x->x_inv_out_fft_plan);
    x->x_sig_buf_pad_fft_plan = kiss_fftr_alloc(x->x_window_double, 0, NULL, NULL);
    x->x_inv_out_fft_plan = kiss_fftr_alloc(x->x_window_double, 1, NULL, NULL);
    // init signal buffers
    for(i = 0; i < x->x_window; i++){
         x->x_signal_buf[i] = 0.0;
        x->x_final_output[i] = 0.0;
    }
    for(i = 0; i < x->x_window_double; i++){
         x->x_signal_buf_padded[i] = 0.0;
         x->x_inv_out_fft_out[i] = 0.0;
    }
    // set num_parts back to zero so that the IR analysis routine is initialized as if it's the first call
    x->x_num_parts = 0;
    // reset DSP ticks since we're clearing a new buffer and starting to fill it with signal
    x->x_dsp_tick = 0;
    // re-run IR analysis routine, but only IF x->arrayName exists
    if(x->x_array_name != gensym("NOARRAYSPECIFIED"))
        conv_set(x, x->x_array_name);
}

static void conv_print(t_conv *x){
    if(x->x_array_name == gensym("NOARRAYSPECIFIED"))
        post("[conv~]: no IR array set");
    else{
        post("[conv~]: IR array: %s", x->x_array_name->s_name);
        post("[conv~]: array length: %i", x->x_array_size);
        post("[conv~]: number of partitions: %i", x->x_num_parts);
    }
    post("[conv~]: partition size: %i", x->x_window);
}

static void conv_initClock(t_conv *x){
    x->x_startup_flag = 1;
    // try analyzing at creation if there was a table specified
    if(x->x_array_name != gensym("NOARRAYSPECIFIED"))
        conv_set(x, x->x_array_name);
}

static void conv_free(t_conv *x){
    t_freebytes(x->x_ir_signal, x->x_array_size*sizeof(t_sample));
    t_freebytes(x->x_signal_buf, x->x_window*sizeof(t_sample));
    t_freebytes(x->x_signal_buf_padded, x->x_window_double*sizeof(t_sample));
    t_freebytes(x->x_inv_out_fft_out, x->x_window_double*sizeof(t_sample));
    t_freebytes(x->x_final_output, x->x_window*sizeof(t_sample));
    // free FFT stuff
    free(x->x_ir_freq_dom_data);
    free(x->x_live_freq_dom_data);
    free(x->x_sig_buf_pad_fft_out);
    free(x->x_inv_out_fft_in);
    kiss_fftr_free(x->x_sig_buf_pad_fft_plan);
    kiss_fftr_free(x->x_inv_out_fft_plan);
    if(x->x_num_parts > 0)
        t_freebytes(x->x_non_overlapped_output, 2*x->x_window_double*sizeof(t_sample));
    else
        t_freebytes(x->x_non_overlapped_output, 0);
    clock_free(x->x_clock);
};

static void *conv_new(t_symbol *s, int ac, t_atom *av){
    s = NULL;
    t_conv *x = (t_conv *)pd_new(conv_class);
    x->x_window = DEFSIZE;
    x->x_array_name = gensym("NOARRAYSPECIFIED");
    // store the pointer to the symbol containing the object name. Can access it for error and post functions via s->s_name
    if(ac && av->a_type == A_FLOAT){
        x->x_window = atom_getfloat(av);
        ac--, av++;
    }
    if(ac && av->a_type == A_SYMBOL){
        x->x_array_name = atom_getsymbol(av);
        ac--, av++;
    }
    if((x->x_window % 64) == 0){ // window size is a multiple of 64
        if(x->x_window < MINSIZE){
            x->x_window = MINSIZE;
            pd_error(x, "[conv~]: requested window size too small. minimum value of %i used instead", x->x_window);
        }
    }
    else{ // window size is NOT a multiple of 64
        x->x_window = DEFSIZE;
        pd_error(x, "[conv~]: window size is not a multiple of 64. default value of %i used instead", x->x_window);
    }
    x->x_clock = clock_new(x, (t_method)conv_initClock);
    x->x_array_size = 0;
    x->x_num_parts = 0;
    x->x_n = 64;
    x->x_dsp_tick = 0;
    x->x_window_double = x->x_window*2;
    x->x_amp_scalar = 1.0f/x->x_window_double;
    x->x_buffer_limit = x->x_window/x->x_n;
    // these will be resized when analysis occurs
    x->x_non_overlapped_output = (t_sample *)getbytes(0);
    // this can probably just be x_window_double * 2!!
    x->x_ir_signal = (t_sample *)getbytes(0);
    x->x_ir_freq_dom_data = (kiss_fft_cpx*)getbytes((x->x_window+1) * sizeof(kiss_fft_cpx));
    x->x_live_freq_dom_data = (kiss_fft_cpx*)getbytes((x->x_window+1) * sizeof(kiss_fft_cpx));
    x->x_signal_buf = (t_sample *)getbytes(x->x_window*sizeof(t_sample));
    x->x_signal_buf_padded = (t_sample *)getbytes(x->x_window_double*sizeof(t_sample));
    x->x_inv_out_fft_out = (t_sample *)getbytes(x->x_window_double*sizeof(t_sample));
    x->x_final_output = (t_sample *)getbytes(x->x_window*sizeof(t_sample));
    // set up the FFTW output buffer for signalBufPadded
    x->x_sig_buf_pad_fft_out = (kiss_fft_cpx*)getbytes((x->x_window+1) * sizeof(kiss_fft_cpx));
    x->x_sig_buf_pad_fft_plan = kiss_fftr_alloc(x->x_window_double, 0, NULL, NULL);
    x->x_inv_out_fft_in = (kiss_fft_cpx*)getbytes((x->x_window+1) * sizeof(kiss_fft_cpx));
    x->x_inv_out_fft_plan = kiss_fftr_alloc(x->x_window_double, 1, NULL, NULL);
    // init signal buffers
    int i;
    for(i = 0; i < x->x_window; i++){
         x->x_signal_buf[i] = 0.0;
        x->x_final_output[i] = 0.0;
    }
    for(i = 0; i < x->x_window_double; i++){
         x->x_signal_buf_padded[i] = 0.0;
         x->x_inv_out_fft_out[i] = 0.0;
    }
    clock_delay(x->x_clock, 0); // wait 0ms before IR analysis to give a control cycle for IR samples to be loaded
    outlet_new(&x->x_obj, &s_signal);
    return(x);
}

void conv_tilde_setup(void){
    conv_class = class_new(gensym("conv~"), (t_newmethod)conv_new,
        (t_method)conv_free, sizeof(t_conv), CLASS_DEFAULT, A_GIMME, 0);
    class_addmethod(conv_class, nullfn, gensym("signal"), 0);
    class_addmethod(conv_class, (t_method)conv_dsp, gensym("dsp"), 0);
    class_addmethod(conv_class, (t_method)conv_size, gensym("size"), A_DEFFLOAT, 0);
    class_addmethod(conv_class, (t_method)conv_set, gensym("set"), A_SYMBOL, 0);
    class_addmethod(conv_class, (t_method)conv_print, gensym("print"), 0);
}
