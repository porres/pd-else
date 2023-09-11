/*
 // Licenced under the GPL-v3
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 // Originally made by mystran, modified by Timothy Schoen
 */
#pragma once

//
// General overview
// ----------------
//
// Circuits are built from nodes and Components, where nodes are
// simply positive integers (with 0 designating ground).
//
// Every Component has one or more pins connecting to the circuit
// nodes as well as zero or more internal nets.
//
// While we map pins directly to nets here, the separation would
// be useful if the solver implemented stuff like net-reordering.
//
// MNACell represents a single entry in the solution matrix,
// where we store constants and time-step dependent constants
// separately, plus collect pointers to dynamic variables.
//
// We track enough information here that we only need to stamp once.

struct MNACell {
    double g;      // simple values (eg. resistor conductance)
    double gtimed; // time-scaled values (eg. capacitor conductance)
    bool nonzero = false;

    // pointers to dynamic variables, added in once per solve
    std::vector<double*> gdyn;

    double lu, prelu; // lu-solver values and matrix pre-LU cache

    void clear()
    {
        g = 0;
        gtimed = 0;
    }

    void initLU(double stepScale)
    {
        prelu = g + gtimed * stepScale;
    }

    // restore matrix state and update dynamic values
    void updatePre()
    {
        lu = prelu;
        for (int i = 0; i < gdyn.size(); ++i) {
            lu += *(gdyn[i]);
        }
    }
};

// Stores A and b for A*x - b = 0, where x is the solution.
//
// A is stored as a vector of rows, for easy in-place pivots
//
struct MNASystem {
    using MNAVector = std::vector<MNACell>;
    using MNAMatrix = std::vector<MNAVector>;

    MNAMatrix A;
    MNAVector b;

    double tStep = 0.0f;
    std::vector<double> output;
    std::map<int, double> input;
    bool blockDC = true;

    void setSize(int n)
    {
        A.resize(n);
        b.resize(n);

        for (unsigned i = 0; i < n; ++i) {
            b[i].clear();
            A[i].resize(n);

            for (unsigned j = 0; j < n; ++j) {
                A[i][j].clear();
            }
        }
    }

    void stampTimed(double g, int r, int c)
    {
        A[r][c].gtimed += g;
    }

    void stampStatic(double g, int r, int c)
    {
        A[r][c].g += g;
    }
};
