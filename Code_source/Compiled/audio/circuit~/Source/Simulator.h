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

void* netlist_create(int argc, t_atom* argv, double sampleRate);
void netlist_free(void* netlist);

void netlist_set_input(void* netlist, int idx, double input);
double netlist_get_output(void* netlist, int idx);
void netlist_tick(void* net);

int netlist_num_inlets(void* net);
int netlist_num_outlets(void* netlist);

void* netlist_reset(void* netlist, double sampleRate);
void netlist_set_dc_block(void* netlist, int block);
void netlist_set_iter(void* netlist, int iter);

#ifdef __cplusplus
}
#endif
