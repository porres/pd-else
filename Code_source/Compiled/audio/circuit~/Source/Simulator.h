/*
 // Licenced under the GPL-v3
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 // Made by Timothy Schoen
 */

#include <memory.h>
#include "m_pd.h"

#pragma once

#ifdef __cplusplus

enum ComponentType {
    tResistor,
    tCapacitor,
    tVoltage,
    tDiode,
    tBJT,
    tTransformer,
    tGyrator,
    tInductor,
    tOpAmp,
    tPotmeter,
    tCurrent,
    tProbe,
    tIter // Special identifier to set number of iterations
};

extern "C" {
#endif

void* simulator_create(int argc, t_atom* argv, int blockSize, double sampleRate);
void simulator_free(void* netlist);

void simulator_set_input(void* netlist, int idx, double input);
double simulator_get_output(void* netlist, int idx);
void simulator_tick(void* net);

int simulator_num_inlets(void* net);
int simulator_num_outlets(void* netlist);

void* simulator_reset(void* netlist, int blockSize, double sampleRate);
void simulator_set_dc_block(void* netlist, int block);
void simulator_set_iter(void* netlist, int iter);

#ifdef __cplusplus
}
#endif
