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

// MNACell represents a single entry in the solution matrix,
// where we store constants and time-step dependent constants
// separately, plus collect pointers to dynamic variables.

// This is only used to stamp the matrix values, after that a more efficient representation is used
// that only keeps track of non-zero values.
struct MNACell {
    double g;      // simple values (eg. resistor conductance)
    double gtimed; // time-scaled values (eg. capacitor conductance)
    std::vector<double*> gdyn;     // pointers to dynamic variables, added in once per solve

    bool nonzero() const
    {
        return g != 0 || gtimed != 0 || !gdyn.empty();
    }
};

using MNAVector = std::vector<MNACell>;
using MNAResultVector = std::vector<double>;
using MNAMatrix = std::vector<MNAVector>;


enum ComponentType {
    tResistor,
    tCapacitor,
    tVoltage,
    tDiode,
    tBJT,
    tMOSFET,
    tJFET,
    tTransformer,
    tGyrator,
    tInductor,
    tOpAmp,
    tOpAmp2,
    tSPST,
    tSPDT,
    tPotmeter,
    tCurrent,
    tProbe,
    tTriode,
    tPentode,/*
    tTappedTransformer,
     Still experimental...
    tBuffer,
    tDelayBuffer */
};

extern "C" {
#endif

void* simulator_create(int argc, t_atom* argv, double sampleRate);
void simulator_free(void* netlist);

void simulator_set_input(void* netlist, int idx, double input);
double simulator_get_output(void* netlist, int idx);
void simulator_tick(void* net);

int simulator_num_inlets(void* net);
int simulator_num_outlets(void* netlist);

void* simulator_reset(void* netlist, double sampleRate);

void simulator_set_dc_block(void* netlist, int block);
void simulator_set_num_iter(void* netlist, int iter);

#ifdef __cplusplus
}
#endif
