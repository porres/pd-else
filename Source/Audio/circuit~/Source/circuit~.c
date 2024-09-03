/*
 // Licenced under the GPL-v3
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 // Made by Timothy Schoen and Alexandre Porres
 */

#include "Simulator.h"
#include <libsamplerate/samplerate.h>

static t_class *circuit_tilde_class;

typedef struct _circuit_tilde {
    t_object x_obj;
    void* x_simulator;
    t_float x_f;
    
    t_outlet* x_out[8];
    t_inlet* x_in[8];
    
    SRC_STATE* x_upsamplers[8];
    SRC_STATE* x_downsamplers[8];
    
    t_float* x_process_buffer[8];
    int x_process_buffer_size;
    int x_oversample_factor;
    
    int x_numin;
    int x_numout;
    int x_enabled;

    int x_numiter;
    int x_dcblock;
    
    int x_last_sr;
    
    t_int ** x_w;
    int x_w_size;
    
} t_circuit_tilde;

void circuit_tilde_enable(t_circuit_tilde *x, t_float enable) {
    x->x_enabled = enable;
}

void circuit_tilde_iter(t_circuit_tilde *x, t_float niter) {
    simulator_set_num_iter(x->x_simulator, (int)niter);
    x->x_numiter = niter;
}


void circuit_tilde_dcblock(t_circuit_tilde *x, t_float dcblock) {
    simulator_set_dc_block(x->x_simulator, dcblock);
    x->x_dcblock = dcblock;
}

void circuit_tilde_reset(t_circuit_tilde *x) {
    x->x_simulator = simulator_reset(x->x_simulator, sys_getsr() * x->x_oversample_factor);
    x->x_last_sr = sys_getsr() * x->x_oversample_factor;
    circuit_tilde_dcblock(x, x->x_dcblock);
    circuit_tilde_iter(x, x->x_numiter);
}

void circuit_tilde_oversample(t_circuit_tilde *x, t_float factor) {
    
    unsigned int factor_power_of_2 = 1;
    while (factor_power_of_2 < factor)
        factor_power_of_2 <<= 1;

    int error = 0;
    
    int new_oversample_buffer_size = sys_getblksize() * factor_power_of_2;
    
    for(int i = 0; i < 8; i++)
    {
        x->x_upsamplers[i] = src_new(SRC_SINC_FASTEST, 1, &error);
        x->x_downsamplers[i] = src_new(SRC_SINC_FASTEST, 1, &error);
        x->x_process_buffer[i] = resizebytes(x->x_process_buffer[i], x->x_process_buffer_size * sizeof(float), new_oversample_buffer_size * sizeof(float));
        
        src_set_ratio(x->x_upsamplers[i], (double)factor_power_of_2);
        src_set_ratio(x->x_downsamplers[i], 1.0 / (double)factor_power_of_2);
    }
    
    x->x_oversample_factor = factor_power_of_2;
    x->x_process_buffer_size = new_oversample_buffer_size;
    
    circuit_tilde_reset(x);
}

void circuit_tilde_bang(t_circuit_tilde *x)
{
    circuit_tilde_reset(x);
    x->x_enabled = 1;
}

t_int *circuit_tilde_perform(t_int *w)
{
    /* the first element is a pointer to the dataspace of this object */
    t_circuit_tilde *x = (t_circuit_tilde *)(w[1]);
    int n = (int)(w[2]);
    
    // If disabled, clear output and return
    if(!x->x_enabled)
    {
        for(int i = 0; i < n; i++)
        {
            for(int out = 0; out < x->x_numout; out++)
            {
                ((t_sample *)w[x->x_numin + out + 3])[i] = 0.0f;
            }
        }
        
        return (w + x->x_numin + x->x_numout + 3);
    }
    
    if(x->x_oversample_factor > 1) {
        // Upsample input
        for(int in = 0; in < x->x_numin; in++)
        {
            SRC_DATA src_data;
            src_data.data_in       = (t_sample *)w[in + 3];
            src_data.data_out      =  x->x_process_buffer[in];
            src_data.input_frames  = n;
            src_data.output_frames = x->x_process_buffer_size;
            src_data.src_ratio     = x->x_oversample_factor;
            src_data.end_of_input  = 0;
            
            src_process(x->x_upsamplers[in], &src_data);
        }
    }
    else {
        for(int in = 0; in < x->x_numin; in++)
        {
            memcpy(x->x_process_buffer[in], (t_sample *)w[in + 3], n * sizeof(t_float));
        }
    }

    for(int i = 0; i < x->x_process_buffer_size; i++)
    {
        if(x->x_enabled)  {
            for(int in = 0; in < x->x_numin; in++)
            {
                simulator_set_input(x->x_simulator, in, x->x_process_buffer[in][i]);
            }

            simulator_tick(x->x_simulator);

            for(int out = 0; out < x->x_numout; out++)
            {
                x->x_process_buffer[out][i] = simulator_get_output(x->x_simulator, out);
            }
        }
    }
    if(x->x_oversample_factor > 1) {
        // Downsample output
        for(int out = 0; out < x->x_numout; out++)
        {
            SRC_DATA src_data;
            src_data.data_in       = x->x_process_buffer[out];
            src_data.data_out      = (t_sample *)w[x->x_numin + out + 3];
            src_data.input_frames  = x->x_process_buffer_size;
            src_data.output_frames = n;
            src_data.src_ratio     = 1.0 / x->x_oversample_factor;
            src_data.end_of_input  = 0;
            
            src_process(x->x_downsamplers[out], &src_data);
        }
    }
    else {
        for(int out = 0; out < x->x_numout; out++)
        {
            memcpy((t_sample *)w[x->x_numin + out + 3], x->x_process_buffer[out], n * sizeof(t_float));
        }
    }
    

    /* return a pointer to the dataspace for the next dsp-object */
    return (w + x->x_numin + x->x_numout + 3);
}

void circuit_tilde_dsp(t_circuit_tilde *x, t_signal **sp)
{
    int sum = x->x_numin + x->x_numout;
    x->x_w = resizebytes(x->x_w, x->x_w_size, sizeof(t_int *) * (sum + 2));

    // Reset simulator if samplerate has changed
    if(x->x_last_sr != sys_getsr() * x->x_oversample_factor) {
        circuit_tilde_reset(x);
    }

    x->x_w_size = sizeof(t_int *) * (sum + 2);
    x->x_w[0] = (t_int*)(x);
    x->x_w[1] = (t_int*)(sp[0]->s_n);
    for (int i = 0; i < sum; i++)
        x->x_w[i + 2] = (t_int*)(sp[i]->s_vec);
    dsp_addv(circuit_tilde_perform, sum + 2, (t_int*)(x->x_w));
}

void circuit_tilde_free(t_circuit_tilde *x)
{
    for(int i = 1; i < x->x_numin; i++)
    {
        inlet_free(x->x_in[i-1]);
    }
    for(int i = 0; i < x->x_numout; i++)
    {
        outlet_free(x->x_out[i]);
    }

    simulator_free(x->x_simulator);
    freebytes(x->x_w, x->x_w_size);
}

void *circuit_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
    t_circuit_tilde *x = (t_circuit_tilde *)pd_new(circuit_tilde_class);
    
    int numiter = 8;
    int oversample_factor = 2;
    int dcblock = 1;
    
    for(int i = 0; i < argc - 1; i++)
    {
        if(argv[i].a_type == A_SYMBOL && argv[i].a_w.w_symbol == gensym("-iter") && argv[i + 1].a_type == A_FLOAT)
        {
            numiter = argv[i + 1].a_w.w_float;
        }
        else if(argv[i].a_type == A_SYMBOL && argv[i].a_w.w_symbol == gensym("-oversample") && argv[i + 1].a_type == A_FLOAT)
        {
            oversample_factor = argv[i + 1].a_w.w_float;
        }
        else if(argv[i].a_type == A_SYMBOL && argv[i].a_w.w_symbol == gensym("-dcblock") && argv[i + 1].a_type == A_FLOAT)
        {
            dcblock = argv[i + 1].a_w.w_float;
        }
    }
    
    x->x_simulator = simulator_create(argc, argv, sys_getsr() * oversample_factor);
    x->x_numin = simulator_num_inlets(x->x_simulator);
    x->x_numout = simulator_num_outlets(x->x_simulator);
    x->x_last_sr = sys_getsr() * oversample_factor;
    x->x_enabled = 1;
    x->x_w_size = 0;
    x->x_w = NULL;
    
    circuit_tilde_dcblock(x, dcblock);
    circuit_tilde_iter(x, numiter);
    circuit_tilde_oversample(x, oversample_factor);
    
    for(int i = 1; i < x->x_numin; i++)
    {
        x->x_in[i-1] = inlet_new(&x->x_obj, &x->x_obj.ob_pd, &s_signal, &s_signal);
    }
    
    for(int i = 0; i < x->x_numout; i++)
    {
        x->x_out[i] = outlet_new(&x->x_obj, &s_signal);
    }
    
    return (void *)x;
}

void circuit_tilde_setup(void) {
    circuit_tilde_class = class_new(gensym("circuit~"),
                                    (t_newmethod)circuit_tilde_new,
                                    (t_method)circuit_tilde_free,
                                    sizeof(t_circuit_tilde),
                                    CLASS_DEFAULT,
                                    A_GIMME, 0);
    
    // Bang will reset and start the simulation
    class_addbang(circuit_tilde_class, (t_method)circuit_tilde_bang);
    
    class_addmethod(circuit_tilde_class,
                    (t_method)circuit_tilde_dsp, gensym("dsp"), A_CANT, 0);
    
    class_addmethod(circuit_tilde_class,
                    (t_method)circuit_tilde_iter, gensym("iter"), A_FLOAT, 0);
    
    class_addmethod(circuit_tilde_class,
                    (t_method)circuit_tilde_oversample, gensym("oversample"), A_FLOAT, 0);
    
    class_addmethod(circuit_tilde_class,
                    (t_method)circuit_tilde_enable, gensym("enable"), A_FLOAT, 0);
    
    class_addmethod(circuit_tilde_class,
                    (t_method)circuit_tilde_dcblock, gensym("dcblock"), A_FLOAT, 0);
    
    class_addmethod(circuit_tilde_class,
                    (t_method)circuit_tilde_reset, gensym("reset"), 0);
    
    CLASS_MAINSIGNALIN(circuit_tilde_class, t_circuit_tilde, x_f);
}
