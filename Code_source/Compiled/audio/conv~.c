/*
based on convolve~ by William Brent

This program is free software: you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more details.
You should have received a copy of the GNU General Public License along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "m_pd.h"
#include "kiss_fft.h"
#include <math.h>
#define MINWIN 64
#define DEFAULTWIN 256
//#define NUMBARKBOUNDS 25

//t_float barkBounds[] = {0, 100, 200, 300, 400, 510, 630, 770, 920, 1080, 1270, 1480, 1720, 2000, 2320, 2700, 3150, 3700, 4400, 5300, 6400, 7700, 9500, 12000, 15500};

static t_class *conv_tilde_class;

typedef struct _conv_tilde{
    t_object x_obj;
    t_symbol *x_array_name;
    t_word *x_vec;
    t_clock *x_clock;
    t_sample *x_ir_signal_eq;
    int x_startup_flag;
    int x_array_size;
    int x_num_parts;
    t_float x_sr;
    t_float x_n;
    int x_dsp_tick;
    int x_buffer_limit;
    int x_window;
    int x_window_double;
    t_float x_amp_scalar;
    t_sample *x_signal_buf;
    t_sample *x_signal_buf_padded;
    t_sample *x_inv_out_fft_out;
    t_sample *x_non_overlapped_output;
    t_sample *x_final_output;

    kiss_fft_cpx *x_ir_freq_dom_data;
    kiss_fft_cpx *x_live_freq_dom_data;
    kiss_fft_cpx *x_sig_buf_pad_fft_out;
    kiss_fft_cpx *x_inv_out_fft_in;
    kiss_fftr_cfg x_sig_buf_pad_fft_plan;
    kiss_fftr_cfg x_inv_out_fft_plan;
    t_float x_f;
}t_conv_tilde;

static t_int *conv_tilde_perform(t_int *w){
    int i, p, n, window, window_double, num_parts;
    t_float amp_scalar;
    t_conv_tilde *x = (t_conv_tilde *)(w[1]);
    t_sample *in = (t_float *)(w[2]);
    t_sample *out = (t_float *)(w[3]);
    n = w[4];
    window = x->x_window;
    window_double = x->x_window_double;
    num_parts = x->x_num_parts;
    amp_scalar = x->x_amp_scalar;
    if(n != 64 || num_parts < 1){
        for(i=0; i<n; i++)
            out[i] = 0.0;

        return (w+5);
    };
    // buffer most recent block
    for(i=0; i<n; i++)
        x->x_signal_buf[(x->x_dsp_tick*n)+i] = in[i];
    if(++x->x_dsp_tick >= x->x_buffer_limit){
        x->x_dsp_tick = 0;
        // don't do anything if the IR hasn't been analyzed yet
        if(x->x_num_parts > 0){
        // copy the signal buffer into the transform IN buffer
        for(i = 0; i < window; i++)
             x->x_signal_buf_padded[i] = x->x_signal_buf[i];
        // pad the rest out with zeros
        for(; i<window_double; i++)
            x->x_signal_buf_padded[i] = 0.0;
        // take FT of the most recent input, padded to double window size
        kiss_fftr(x->x_sig_buf_pad_fft_plan, x->x_signal_buf_padded, x->x_sig_buf_pad_fft_out);
         // multiply against partitioned IR spectra. these need to be complex multiplies
         // also, sum into appropriate part of the nonoverlapped buffer
        for(p = 0; p<num_parts; p++){
            int startIdx;
            startIdx = p*(window+1);
            for(i = 0; i < (window+1); i++){
                t_float realLive, imagLive, realIR, imagIR, real, imag;
                realLive = x->x_sig_buf_pad_fft_out[i].r;
                imagLive = x->x_sig_buf_pad_fft_out[i].i;
                realIR = x->x_ir_freq_dom_data[startIdx+i].r;
                imagIR = x->x_ir_freq_dom_data[startIdx+i].i;
                // MINUS the imag part because i^2 = -1
                real = (realLive * realIR) - (imagLive * imagIR);
                imag = (imagLive * realIR) + (realLive * imagIR);
                // sum into the live freq domain data buffer
                x->x_live_freq_dom_data[startIdx+i].r += real;
                x->x_live_freq_dom_data[startIdx+i].i += imag;
            }
        }
        // copy the freq dom data from head of the complex summing buffer into the inverse FFT input buffer
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
        for(; i<(2*window_double); i++)
            x->x_non_overlapped_output[i] = 0.0;
        }
    };
    for(i = 0; i < n; i++)     // output
        out[i] = x->x_final_output[(x->x_dsp_tick*n)+i];
    return(w+5);
}

// could do a check here for whether x->x_n == sp[0]->s_n
// if not, could suspend DSP and throw an error. Block sizes must match
static void conv_tilde_dsp(t_conv_tilde *x, t_signal **sp){
    dsp_add(conv_tilde_perform, 4, x, sp[0]->s_vec, sp[1]->s_vec, sp[0]->s_n);
    // TODO: could allow for re-blocking and re-calc x_buffer_limit based on new x_n
    if(sp[0]->s_n != x->x_n)
        pd_error(x, "[conv~]: block size must be 64. DSP suspended.");
    if(sp[0]->s_sr != x->x_sr){
        x->x_sr = sp[0]->s_sr;
        post("[conv~]: sample rate updated to %0.0f", x->x_sr);
    };
    if(x->x_num_parts < 1)
        pd_error(x, "[conv~]: impulse response analysis not performed yet. output will be zero.");
};

/*static void conv_tilde_eq(t_conv_tilde *x, t_symbol *s, int argc, t_atom *argv){
    int i, j, window_triple, window_triple_half, *bark_bin_bounds;
    t_float *fft_in, *eq_array;
    kiss_fft_cpx *fft_out;
    kiss_fftr_cfg fft_forward_cfg, fft_inverse_cfg;

    // if no array has been analyzed yet, we can't do the EQ things below
    if(!strcmp(x->x_array_name->s_name, "NOARRAYSPECIFIED"))
        pd_error(x, "%s: no IR array has been analyzed", x->x_obj_symbol->s_name);
    else
    {
    // we'll pad with x->x_array_size zeros before and after the IR signal
    window_triple = x->x_array_size*3;

    // FFTW documentation says the output of a r2c forward transform is floor(N/2)+1
    window_triple_half = floor(window_triple/(t_float)2)+1;

    eq_array = (t_float *)getbytes(window_triple_half*sizeof(t_float));

    // at this point, if we took in 24 bark band scalars instead of the insane x->x_array_size*3 scalars, we could find the bin bounds for each of the Bark bands and fill eq_array with those.

    // array to hold bin number of each of the 25 bark bounds, plus one more for Nyquist
    bark_bin_bounds = (int *)getbytes((NUMBARKBOUNDS+1)*sizeof(int));

    for(i=0; i<NUMBARKBOUNDS; i++)
        bark_bin_bounds[i] = floor((barkBounds[i]*window_triple)/x->x_sr);

    // the upper bound should be the Nyquist bin
    bark_bin_bounds[NUMBARKBOUNDS] = window_triple_half-1;

    if(argc != NUMBARKBOUNDS)
        post("%s: WARNING: \"eq\" message should contain %i frequency band scalars", x->x_obj_symbol->s_name, NUMBARKBOUNDS);

    // need to check that argc == NUMBARKBOUNDS
    for(i=0; i<NUMBARKBOUNDS && i<argc; i++)
        for(j=bark_bin_bounds[i]; j<bark_bin_bounds[i+1]; j++)
            eq_array[j] = (atom_getfloat(argv+i)<0.0)?0:atom_getfloat(argv+i);

    // if there were too few arguments coming in (argc<NUMBARKBOUNDS), fill out the remaining bin scalars in eq_array with 1.0
    for(; i<NUMBARKBOUNDS; i++)
        for(j=bark_bin_bounds[i]; j<bark_bin_bounds[i+1]; j++)
            eq_array[j] = 1.0;

    // need to set the Nyquist bin too, since the j loop doesn't go to the upper bound inclusive
    eq_array[window_triple_half-1] = (atom_getfloat(argv+(NUMBARKBOUNDS-1))<0.0)?0:atom_getfloat(argv+(NUMBARKBOUNDS-1));

    // set up FFTW input buffer
    fft_in = (t_float *)t_getbytes(window_triple * sizeof(t_float));

    // set up the FFTW output buffer
    fft_out = (kiss_fft_cpx*)getbytes(window_triple * sizeof(kiss_fft_cpx));

    // allocate kissfft
    fft_forward_cfg = kiss_fftr_alloc(window_triple, 0, NULL, NULL);
    fft_inverse_cfg = kiss_fftr_alloc(window_triple, 1, NULL, NULL);

    // fill input buffer with zeros
    for(i=0; i<window_triple; i++)
        fft_in[i] = 0.0;

    // place actual IR signal in center of buffer
    for(i=0; i<x->x_array_size; i++)
        fft_in[x->x_array_size+i] = x->x_vec[i].w_float;

    // perform forward FFT
    kiss_fftr(fft_forward_cfg, fft_in, fft_out);

    // apply bin scalars
    for(i=0; i<window_triple_half; i++)
    {
        fft_out[i].r *= eq_array[i];
        fft_out[i].i *= eq_array[i];
    }

    // perform inverse FFT
    kiss_fftri(fft_inverse_cfg, fft_out, fft_in);

    // write altered signal to internal memory for analysis
    for(i=0; i<x->x_array_size; i++)
        x->x_ir_signal_eq[i] = fft_in[x->x_array_size+i]/window_triple;

    freebytes(eq_array, window_triple_half*sizeof(t_float));
    freebytes(bark_bin_bounds, (NUMBARKBOUNDS+1)*sizeof(int));
    freebytes(fft_in, window_triple*sizeof(t_float));
    freebytes(fft_out, window_triple_half*sizeof(kiss_fft_cpx));
    
    kiss_fft_free(fft_forward_cfg);
    kiss_fft_free(fft_inverse_cfg);

    post("%s: EQ scalars applied to IR array %s", x->x_obj_symbol->s_name, x->x_array_name->s_name);

    // re-run analysis
    conv_tilde_set(x, x->x_array_name);
    }
}

 static void conv_tilde_clear(t_conv_tilde *x){
     int i;
     for(i = 0; i < x->x_window; i++){ // init signal buffers
          x->x_signal_buf[i] = 0.0;
             x->x_final_output[i] = 0.0;
     }
     for(i = 0; i < x->x_window_double; i++){
         x->x_signal_buf_padded[i] = 0.0;
         x->x_inv_out_fft_out[i] = 0.0;
     }
     if(x->x_num_parts > 0){
         // clear time-domain buffer
         for(i = 0; i < 2*x->x_window_double; i++)
             x->x_non_overlapped_output[i] = 0.0;
         // clear x_live_freq_dom_data
         for(i = 0; i < x->x_num_parts*(x->x_window+1); i++){
             x->x_live_freq_dom_data[i].r = 0.0;
             x->x_live_freq_dom_data[i].i = 0.0;
         }
     }
     // reset buffering process
     x->x_dsp_tick = 0;
 }
 */

static void conv_tilde_set(t_conv_tilde *x, t_symbol *arrayName){
    t_garray *array_ptr;
    int i, j, old_non_overlapped_size, new_non_overlapped_size;
    t_float *fft_in;
    kiss_fft_cpx *fft_out;
    kiss_fftr_cfg fft_cfg;
    if(x->x_num_parts > 0)
        old_non_overlapped_size = 2*x->x_window_double;
    else
        old_non_overlapped_size = 0;
    // if this call to _analyze() is issued from _eq(), the incoming arrayName will match x->x_array_name.
    // if incoming arrayName doesn't match x->x_array_name, load arrayName and dump its samples into x_ir_signal_eq
    if(arrayName->s_name != x->x_array_name->s_name || x->x_startup_flag){
        int old_array_size;
        x->x_startup_flag = 0;
        old_array_size = x->x_array_size;
        if(!(array_ptr = (t_garray *)pd_findbyclass(arrayName, garray_class))){
            if(*arrayName->s_name){
                pd_error(x, "[conv~]: no array named %s", arrayName->s_name);
                // resize x_ir_signal_eq back to 0
                x->x_ir_signal_eq = (t_sample *)t_resizebytes(
                    x->x_ir_signal_eq,
                    old_array_size*sizeof(t_sample),
                    0
                );
                x->x_array_size = 0;
                x->x_vec = 0;
                return;
            }
        }
        else if(!garray_getfloatwords(array_ptr, &x->x_array_size, &x->x_vec)){
            pd_error(x, "[conv~]: bad template for %s", arrayName->s_name);
            // resize x_ir_signal_eq back to 0
            x->x_ir_signal_eq = (t_sample *)t_resizebytes(
                x->x_ir_signal_eq,
                old_array_size*sizeof(t_sample),
                0
            );
            x->x_array_size = 0;
            x->x_vec = 0;
            return;
        }
        else
            x->x_array_name = arrayName;
        // resize x_ir_signal_eq
        x->x_ir_signal_eq = (t_sample *)t_resizebytes(
            x->x_ir_signal_eq,
            old_array_size*sizeof(t_sample),
            x->x_array_size*sizeof(t_sample)
        );
        // since this is first analysis of arrayName, load it into x_ir_signal_eq
        for(i=0; i<x->x_array_size; i++)
            x->x_ir_signal_eq[i] = x->x_vec[i].w_float;
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
        // we are analyzing partitions of x_ir_signal_eq, in case there has been any EQ
        for(j = 0; j < x->x_window && (start_vec+j)<x->x_array_size; j++)
            fft_in[j] = x->x_ir_signal_eq[start_vec+j];
        // zero pad
        for(; j<x->x_window_double; j++)
            fft_in[j] = 0.0;
        // perform FFT
        kiss_fftr(fft_cfg, fft_in, fft_out);
        // copy freq domain data from fft output buffer into
        // larger IR freq domain data buffer
        for(j = 0; j<x->x_window+1; j++){
            x->x_ir_freq_dom_data[start_spec+j].r = fft_out[j].r;
            x->x_ir_freq_dom_data[start_spec+j].i = fft_out[j].i;
        }
    }
    freebytes(fft_in, (x->x_window_double)*sizeof(t_float));
    freebytes(fft_out, (x->x_window+1)*sizeof(kiss_fft_cpx));
    kiss_fft_free(fft_cfg);
    // TS: I'm not sure we want to print this message every time, seems excessive
    //post("%s: analysis of IR array %s complete. Array size: %i. Partitions: %i.", x->x_obj_symbol->s_name, x->x_array_name->s_name, x->x_array_size, x->x_num_parts);
}

static void conv_tilde_size(t_conv_tilde *x, t_float w){
    int i, i_win, is64, old_window, old_window_double;
    i_win = w;
    is64 = (i_win%64)==0;
    old_window = x->x_window;
    old_window_double = x->x_window_double;
    if(is64){
        if(i_win >= MINWIN)
            x->x_window = i_win;
        else{
            x->x_window = MINWIN;
            pd_error(x, "[conv~]: requested window size too small. minimum value of %i used instead", x->x_window);
        }
    }
    else{
        x->x_window = DEFAULTWIN;
        pd_error(x, "[conv~]: window not a multiple of 64. default value of %i used instead", x->x_window);
    }
    // resize time-domain buffer to zero bytes
    x->x_non_overlapped_output =
        (t_sample *)t_resizebytes(x->x_non_overlapped_output, (2*x->x_window_double)*sizeof(t_sample), 0);
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
//    post("[conv~]: partition size: %i", x->x_window);
    // set num_parts back to zero so that the IR analysis routine is initialized as if it's the first call
    x->x_num_parts = 0;
    // reset DSP ticks since we're clearing a new buffer and starting to fill it with signal
    x->x_dsp_tick = 0;
    // re-run IR analysis routine, but only IF x->arrayName exists
    if(x->x_array_name != gensym("NOARRAYSPECIFIED"))
        conv_tilde_set(x, x->x_array_name);
}

static void conv_tilde_print(t_conv_tilde *x){
    post("[conv~]: IR array: %s", x->x_array_name->s_name);
    post("[conv~]: array length: %i", x->x_array_size);
    post("[conv~]: number of partitions: %i", x->x_num_parts);
    post("[conv~]: partition size: %i", x->x_window);
}

static void conv_tilde_initClock(t_conv_tilde *x){
    x->x_startup_flag = 1;
    // try analyzing at creation if there was a table specified
    if(x->x_array_name != gensym("NOARRAYSPECIFIED"))
        conv_tilde_set(x, x->x_array_name);
}

static void conv_tilde_free(t_conv_tilde *x){
    t_freebytes(x->x_ir_signal_eq, x->x_array_size*sizeof(t_sample));
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
    if(x->x_num_parts>0)
        t_freebytes(x->x_non_overlapped_output, 2*x->x_window_double*sizeof(t_sample));
    else
        t_freebytes(x->x_non_overlapped_output, 0);
    clock_free(x->x_clock);
};

static void *conv_tilde_new(t_symbol *s, int argc, t_atom *argv){
    s = NULL;
    t_conv_tilde *x = (t_conv_tilde *)pd_new(conv_tilde_class);
    int i, is64, window;
    outlet_new(&x->x_obj, &s_signal);
    // store the pointer to the symbol containing the object name. Can access it for error and post functions via s->s_name
    switch(argc){
        case 0:
            window = DEFAULTWIN;
            x->x_array_name = gensym("NOARRAYSPECIFIED");
            break;
        case 1:
            window = atom_getfloat(argv);
            x->x_array_name = gensym("NOARRAYSPECIFIED");
            break;
        case 2:
            window = atom_getfloat(argv);
            x->x_array_name = atom_getsymbol(argv+1);
            break;
        default:
            pd_error(x, "[conv~]: the only creation argument should be the window/partition size in samples");
            window = DEFAULTWIN;
            x->x_array_name = gensym("NOARRAYSPECIFIED");
            break;
    }
    is64 = (window % 64) == 0;
    if(is64){
        if(window >= MINWIN)
            x->x_window = window;
        else{
            x->x_window = MINWIN;
            pd_error(x, "[conv~]: requested window size too small. minimum value of %i used instead", x->x_window);
        }
    }
    else{
        x->x_window = DEFAULTWIN;
        pd_error(x, "[conv~]: window not a multiple of 64. default value of %i used instead", x->x_window);
    }
    x->x_clock = clock_new(x, (t_method)conv_tilde_initClock);
    x->x_array_size = 0;
    x->x_num_parts = 0;
    x->x_sr = 44100;
    x->x_n = 64;
    x->x_dsp_tick = 0;
    x->x_window_double = x->x_window*2;
    x->x_amp_scalar = 1.0f/x->x_window_double;

    x->x_buffer_limit = x->x_window/x->x_n;

    // these will be resized when analysis occurs
    x->x_non_overlapped_output = (t_sample *)getbytes(0);
    // this can probably just be x_window_double * 2!!
    x->x_ir_signal_eq = (t_sample *)getbytes(0);
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
    for(i = 0; i < x->x_window; i++){
         x->x_signal_buf[i] = 0.0;
        x->x_final_output[i] = 0.0;
    }
    for(i = 0; i<x->x_window_double; i++){
         x->x_signal_buf_padded[i] = 0.0;
         x->x_inv_out_fft_out[i] = 0.0;
    }
    clock_delay(x->x_clock, 0); // wait 0ms before IR analysis to give a control cycle for IR samples to be loaded
    return(x);
}

void conv_tilde_setup(void){
  conv_tilde_class = class_new(gensym("conv~"), (t_newmethod)conv_tilde_new,
        (t_method)conv_tilde_free, sizeof(t_conv_tilde), CLASS_DEFAULT, A_GIMME, 0);
  CLASS_MAINSIGNALIN(conv_tilde_class, t_conv_tilde, x_f);
  class_addmethod(conv_tilde_class, (t_method)conv_tilde_dsp, gensym("dsp"), 0);
  class_addmethod(conv_tilde_class, (t_method)conv_tilde_size, gensym("size"), A_DEFFLOAT, 0);
  class_addmethod(conv_tilde_class, (t_method)conv_tilde_set, gensym("set"), A_SYMBOL, 0);
  class_addmethod(conv_tilde_class, (t_method)conv_tilde_print, gensym("print"), 0);
  //  class_addmethod(conv_tilde_class, (t_method)conv_tilde_clear, gensym("clear"), 0);
//  class_addmethod(conv_tilde_class, (t_method)conv_tilde_eq, gensym("eq"), A_GIMME, 0);
}
