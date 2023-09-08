/*
 // Licenced under the GPL-v3
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 // Made by Timothy Schoen and Alexandre Porres
 */

#include "Simulator.h"

static t_class *circuit_tilde_class;

typedef struct _circuit_tilde {
    t_object x_obj;
    void* x_simulator;
    t_float f;
    
    t_outlet* x_out[8];
    t_inlet* x_in[8];
    
    int x_numin;
    int x_numout;
    int x_enabled;
    int x_sr;
    
    t_int **w;
    
} t_circuit_tilde;

t_int *circuit_tilde_perform(t_int *w)
{
    /* the first element is a pointer to the dataspace of this object */
    t_circuit_tilde *x = (t_circuit_tilde *)(w[1]);
    int n = (int)(w[2]);
    
    for(int i = 0; i < n; i++)
    {
        if(x->x_enabled)  {
            for(int in = 0; in < x->x_numin; in++)
            {
                simulator_set_input(x->x_simulator, in, ((t_sample *)w[in + 3])[i]);
            }
            
            simulator_tick(x->x_simulator);
            
            for(int out = 0; out < x->x_numout; out++)
            {
                ((t_sample *)w[x->x_numin + out + 3])[i] = simulator_get_output(x->x_simulator, out);
            }
        }
        else {
            for(int out = 0; out < x->x_numout; out++)
            {
                ((t_sample *)w[x->x_numin + out + 3])[i] = 0.0f;
            }
        }
    }
    
    /* return a pointer to the dataspace for the next dsp-object */
    return (w + x->x_numin + x->x_numout + 3);
}

void circuit_tilde_dsp(t_circuit_tilde *x, t_signal **sp)
{
    int sum = x->x_numin + x->x_numout;
    x->w = getbytes(sizeof(t_int *) * (sum + 2));
    t_int **w = x->w;
    w[0] = (t_int*)(x);
    w[1] = (t_int*)(sp[0]->s_n);
    for (int i = 0; i < sum; i++)
        w[i + 2] = (t_int*)(sp[i]->s_vec);
    dsp_addv(circuit_tilde_perform, sum + 2, (t_int*)(w));
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
}

void *circuit_tilde_new(t_symbol *s, int argc, t_atom *argv)
{
    t_circuit_tilde *x = (t_circuit_tilde *)pd_new(circuit_tilde_class);
    x->x_simulator = simulator_create(argc, argv, sys_getblksize(), sys_getsr());
    x->x_numin = simulator_num_inlets(x->x_simulator);
    x->x_numout = simulator_num_outlets(x->x_simulator);
    x->x_enabled = 1;
    
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

void circuit_tilde_iter(t_circuit_tilde *x, t_float niter) {
    simulator_set_iter(x->x_simulator, (int)niter);
};

void circuit_tilde_enable(t_circuit_tilde *x, t_float enable) {
    x->x_enabled = enable;
};

void circuit_tilde_dcblock(t_circuit_tilde *x, t_float dcblock) {
    simulator_set_dc_block(x->x_simulator, dcblock);
};

void circuit_tilde_reset(t_circuit_tilde *x) {
    x->x_simulator = simulator_reset(x->x_simulator, sys_getblksize(), sys_getsr());
    x->x_numin = simulator_num_inlets(x->x_simulator);
    x->x_numout = simulator_num_outlets(x->x_simulator);
};

void circuit_tilde_bang(t_circuit_tilde *x)
{
    x->x_simulator = simulator_reset(x->x_simulator, sys_getblksize(), sys_getsr());
    x->x_numin = simulator_num_inlets(x->x_simulator);
    x->x_numout = simulator_num_outlets(x->x_simulator);
    x->x_enabled = 1;
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
                    (t_method)circuit_tilde_enable, gensym("enable"), A_FLOAT, 0);
    
    class_addmethod(circuit_tilde_class,
                    (t_method)circuit_tilde_dcblock, gensym("dcblock"), A_FLOAT, 0);
    
    class_addmethod(circuit_tilde_class,
                    (t_method)circuit_tilde_reset, gensym("reset"), 0);
    
    CLASS_MAINSIGNALIN(circuit_tilde_class, t_circuit_tilde, f);
}

/* DISTORTION
 circuit~;
 voltage 12 1 0;
 resistor 100000 2 1;
 resistor 11000 2 0;
 capacitor 1e-07 2 5;
 resistor 1000 5 6;
 input 6 0;
 resistor 10000 1 3;
 resistor 680 4 0;
 bjt 2 3 4 0;
 bjt 3 1 7 0;
 resistor 100000 7 0;
 probe 7 0;
 */

/* MOOG LPF
 circuit~;
 probe 1 0;
 voltage $s0 2 0;
 opamp 3 4 1;
 resistor 2300 4 1;
 resistor 1000 4 0;
 capacitor 0.000001 5 1;
 capacitor 0.000001 3 0;
 resistor $s1 2 5;
 resistor $s1 5 3;
 */

/* FUZZFACE
 circuit~;
 probe 1 0;
 voltage $s0 2 0;
 capacitor 2.2e-6 2 3;
 resistor 100e3 3 4;
 voltage 9 5 0;
 resistor 330 5 6;
 resistor 33e3 5 7;
 resistor 8200 6 8;
 capacitor 10e-9 6 9;
 bjt 7 6 4 0;
 bjt 3 7 0 0;
 resistor 250e3 9 1;
 resistor 250e3 1 0;
 resistor 500 4 10;
 resistor 500 10 0;
 capacitor 20e-6 10 0;
 
 */

/* VOX WAH
 circuit~;
 probe 1 0;
 voltage $s0 2 0;
 resistor 22e3 3 4;
 bjt 5 7 6 0;
 bjt 8 4 9 0;
 resistor 470e3 4 10;
 resistor 470 9 0;
 resistor 1.5e3 8 11;
 resistor 82e3 12 0;
 resistor 68e3 2 13;
 capacitor 1e-08 13 8;
 voltage 9 3 0;
 resistor 33e3 11 10;
 capacitor 4.7e-6 10 0;
 inductor 500e-3 11 10;
 capacitor 0.01e-6 11 6;
 capacitor 0.22 4 1;
 capacitor 0.22 5 14;
 resistor 1e3 3 7;
 potmeter $s1 10e3 14 1 0;
 resistor 10e3 6 0;
 resistor 470e3 4 5;
 */

/*      BIG MUFF
 circuit~;
 probe 1 0;
 voltage #s0 0.5 2 0;
 voltage 9 3 0;
 bjt 4 5 6 0;
 bjt 7 8 9 0;
 resistor 10e3 3 8;
 resistor 39e3 2 10;
 capacitor 1e-6 10 7;
 resistor 47e3 7 0;
 resistor 100 9 0;
 resistor 470e3 7 8;
 capacitor 470e-12 7 8;
 capacitor 1e-6 8 11;
 resistor 50e3 11 12;
 resistor 50e3 12 13;
 resistor 1e3 13 0;
 capacitor 100e-9 12 14;
 resistor 10e3 14 4;
 resistor 100e3 4 0;
 resistor 150 6 0;
 resistor 10e3 3 5;
 capacitor 470e-12 4 5;
 diode 15 5;
 capacitor 1e-6 4 15;
 diode 5 15;
 capacitor 100e-9 5 16;
 resistor 10e3 16 17;
 resistor 100e3 17 0;
 resistor 470e3 4 5;
 bjt 17 18 19 0;
 resistor 150 19 0;
 resistor 470e3 17 18;
 capacitor 470e-12 17 18;
 capacitor 1e-6 17 20;
 diode 18 20;
 capacitor 4e-9 18 21;
 diode 20 18;
 resistor 39e3 18 22;
 resistor 10e3 3 18;
 capacitor 10e-6 22 0;
 resistor 22e3 21 0;
 resistor 50e3 22 23;
 resistor 50e3 21 23;
 capacitor 100e-9 23 24;
 resistor 100e3 24 0;
 resistor 430e3 3 24;
 resistor 15e3 3 25;
 bjt 24 25 26 0;
 resistor 3300 26 0;
 capacitor 100e-9 25 27;
 resistor 50e3 27 1;
 resistor 50e3 1 0;
 */

/* DIODE OCTAVER
 probe 1 0;
 voltage $s0 2 3;
 diode 2 1;
 diode 0 3;
 diode 0 2;
 diode 3 1;
 */
