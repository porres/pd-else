//
//  CircuitSimulator.hpp
//  pd-circuit
//
//  Created by Timothy Schoen on 05/09/2023.
//
#include "m_pd.h"

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

void* create(int argc, t_atom* argv, double sampleRate);

void set_input(void* netlist, int idx, double input);
double get_output(void* netlist, int idx);
void process(void* net);

int num_inlets(void* net);
int num_outlets(void* netlist);

void set_dc_block(void* netlist, int block);
void set_iter(void* netlist, int iter);

#ifdef __cplusplus
}
#endif
