/*
 // Licenced under the GPL-v3
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 // Originally made by mystran, modified by Timothy Schoen
 */

#pragma once
#include <KLU/klu.h>

using NetlistDescription = std::vector<std::tuple<ComponentType, std::vector<std::string>, std::vector<int>>>;

struct NetList {
    typedef std::vector<IComponent*> ComponentList;

    NetList(NetlistDescription& netlist, int nNets, int numSamples, double sampleRate)
        : lastNetlist(netlist)
        , nets(nNets)
        , lastNumNets(nNets)
        , blockSize(numSamples)
    {
        auto isDynamicArgument = [](std::string arg){ return arg.rfind("$s", 0) == 0 || arg.rfind("$f", 0) == 0; };
        
        int numOut = 0;
        for (const auto& [type, args, pins] : netlist) {
            switch (type) {
            case tResistor: {
                if (args.size() == 1) {
                    if (isDynamicArgument(args[0])) {
                        addComponent(new VariableResistor(addDynamicArgument(args[0]), pins[0], pins[1]));
                    } else {
                        addComponent(new Resistor(getArgumentValue(args[0]), pins[0], pins[1]));
                    }
                } else {
                    pd_error(NULL, "circuit~: wrong number of arguments for resistor");
                }
                break;
            }
            case tCapacitor: {
                if (args.size() == 1) {
                    addComponent(new Capacitor(getArgumentValue(args[0]), pins[0], pins[1]));
                } else {
                    pd_error(NULL, "circuit~: wrong number of arguments for capacitor");
                }
                break;
            }
            case tVoltage: {
                if (args.size() == 1) {
                    if (isDynamicArgument(args[0])) {
                        addComponent(new VariableVoltage(addDynamicArgument(args[0]), pins[0], pins[1]));
                    } else {
                        addComponent(new Voltage(getArgumentValue(args[0]), pins[0], pins[1]));
                    }
                } else {
                    pd_error(NULL, "circuit~: wrong number of arguments for voltage");
                }
                break;
            }
            case tDiode: {
                if (args.size() == 0) {
                    addComponent(new Diode(pins[0], pins[1]));
                } else {
                    pd_error(NULL, "circuit~: wrong number of arguments for diode");
                }
                break;
            }
            case tBJT: {
                if (args.size() == 1) {
                    addComponent(new BJT(pins[0], pins[1], pins[2], getArgumentValue(args[0])));
                } else {
                    pd_error(NULL, "circuit~: wrong number of arguments for bjt");
                }
                break;
            }
            case tTransformer: {
                if (args.size() == 1) {
                    addComponent(new Transformer(getArgumentValue(args[0]), pins[0], pins[1], pins[2], pins[3]));
                } else {
                    pd_error(NULL, "circuit~: wrong number of arguments for transformer");
                }
                break;
            }
            case tGyrator: {
                if (args.size() == 1) {
                    addComponent(new Gyrator(getArgumentValue(args[0]), pins[0], pins[1], pins[2], pins[3]));
                } else {
                    pd_error(NULL, "circuit~: wrong number of arguments for gyrator");
                }
                break;
            }
            case tInductor: {
                if (args.size() == 1) {
                    addComponent(new Inductor(getArgumentValue(args[0]), pins[0], pins[1]));
                } else {
                    pd_error(NULL, "circuit~: wrong number of arguments for inductor");
                }
                break;
            }
            case tOpAmp: {
                if (args.size() == 0) {
                    addComponent(new OpAmp(10, 15, pins[0], pins[1], pins[2]));
                } else if (args.size() == 1) {
                    addComponent(new OpAmp(getArgumentValue(args[0]), 15, pins[0], pins[1], pins[2]));
                } else if (args.size() == 2) {
                    addComponent(new OpAmp(getArgumentValue(args[0]), getArgumentValue(args[1]), pins[0], pins[1], pins[2]));
                } else {
                    pd_error(NULL, "circuit~: wrong number of arguments for opamp");
                }

                break;
            }
            case tPotmeter: {
                if (args.size() == 2) {
                    addComponent(new Potentiometer(addDynamicArgument(args[0]), getArgumentValue(args[1]), pins[0], pins[1], pins[2]));
                } else {
                    pd_error(NULL, "circuit~: wrong number of arguments for potmeter");
                }
                break;
            }
            case tCurrent: {
                if (args.size() == 1) {
                    if (isDynamicArgument(args[0])) {
                        addComponent(new VariableCurrent(addDynamicArgument(args[0]), pins[0], pins[1]));
                    } else {
                        addComponent(new Current(getArgumentValue(args[0]), pins[0], pins[1]));
                    }
                } else {
                    pd_error(NULL, "circuit~: wrong number of arguments for current");
                }
            }
            case tProbe: {
                addComponent(new Probe(pins[0], pins[1], numOut++));
                break;
            }
            case tIter: {
                maxiter = getArgumentValue(args[0]);
                break;
            }
            }
        }

        system.setSize(nets);

        for (auto& c : components) {
            c->stamp(system);
        }

        setStepScale(0);
        system.tStep = 0;

        updatePre();
        updateMatrix();

        initKLU();

        system.output.resize(numOut);

        // get DC solution
        simulateTick();
        setTimeStep(1.0 / sampleRate);
    }

    void addComponent(IComponent* c)
    {
        c->setupNets(nets, c->getPinLocs());
        components.push_back(c);
    }

    void setTimeStep(double tStepSize)
    {
        for (int i = 0; i < components.size(); ++i) {
            components[i]->scaleTime(system.tStep / tStepSize);
        }

        system.tStep = tStepSize;
        double stepScale = 1. / system.tStep;
        setStepScale(stepScale);

        refactorKLU();
    }

    void setStepScale(double stepScale)
    {
        // initialize matrix for LU and save it to cache
        for (int i = 0; i < nets; ++i) {
            system.b[i].initLU(stepScale);
            for (int j = 0; j < nets; ++j) {
                system.A[i][j].initLU(stepScale);
            }
        }
    }

    void simulateTick()
    {
        // Clear last output
        std::fill(system.output.begin(), system.output.end(), 0.0f);

        solve();
        update();
    }

    MNASystem const& getMNA() const
    {
        return system;
    }

    double getOutput(int idx) const
    {
        return system.output[idx];
    }

    void setInput(int idx, double value)
    {
        system.input[idx] = value;
    }

    void setBlockDC(bool blockDC)
    {
        system.blockDC = blockDC;
    }

    double& addDynamicArgument(std::string arg)
    {
        int idx = 0;
        try {
            idx = std::stoi(arg.substr(2)) - 1;
            if (idx < 0)
                throw std::range_error("index out of range");
            system.input[idx] = 0.0f;
            return system.input[idx];
        } catch (...) {
            auto errorMessage = "circuit~: malformed dynamic argument: \"" + arg + "\"";
            pd_error(NULL, errorMessage.c_str());
            system.input[0] = 0.0f;
            return system.input[0];
        }
    }

    int getMaxDynamicArgument() const
    {
        int max = 0;
        for (const auto& [idx, value] : system.input) {
            max = std::max(idx + 1, max);
        }

        return max;
    }

    void setMaxIter(int iter)
    {
        maxiter = iter;
    }

    int getLastNumNets() const
    {
        return lastNumNets;
    }

    NetlistDescription getLastNetlist() const
    {
        return lastNetlist;
    }

protected:
    int nets;
    int blockSize;
    ComponentList components;

    MNASystem system;

    // parameters
    int maxiter = 20;
    double solvertol = 1e-7;
    bool nochecking = true;

    klu_symbolic* Symbolic;
    klu_numeric* Numeric;
    klu_common Common;

    std::vector<double> b;
    std::vector<double> AVal;
    std::vector<int> AI;
    std::vector<int> AJ;
    std::vector<MNACell*> nzpointers;

    // Netlist state for resetting
    const NetlistDescription lastNetlist;
    const int lastNumNets;
    
    void update()
    {
        for (int i = 0; i < components.size(); ++i) {
            components[i]->update(system);
        }
    }

    // return true if we're done
    bool newton()
    {
        bool done = true;
        for (int i = 0; i < components.size(); ++i) {
            done = done && components[i]->newton(system);
        }
        return done;
    }

    void updatePre()
    {
        for (int i = 0; i < nets; ++i) {
            system.b[i].updatePre();
        }

        for (int i = 0; i < nzpointers.size(); ++i) {
            nzpointers[i]->updatePre();
        }
    }

    void initKLU()
    {
        klu_defaults(&Common);
        Common.tol = solvertol;
        Common.scale = nochecking ? -1 : 2;

        // Symbolic analysis
        Symbolic = klu_analyze((int)nets - 1, &AI[0], &AJ[0], &Common);
        // Full numeric factorization: Only needed once!
        Numeric = klu_factor(&AI[0], &AJ[0], getAValues(), Symbolic, &Common);
    }

    void refactorKLU()
    {
        klu_free_symbolic(&Symbolic, &Common);
        klu_free_numeric(&Numeric, &Common);

        // Symbolic analysis
        Symbolic = klu_analyze(nets - 1, &AI[0], &AJ[0], &Common);

        // Full numeric factorization: Only needed once!
        Numeric = klu_factor(&AI[0], &AJ[0], getAValues(), Symbolic, &Common);
    }

    void solve()
    {
        // Iterate junctions
        for (int iter = 0; iter < std::max(maxiter, 1); iter++) {
            // restore matrix state and add dynamic values
            updatePre();

            solveKLU();

            if (newton())
                break;
        }
    }

    void solveKLU()
    {
        // Update our factorization or refactor if the last factorization failed
        klu_refactor(&AI[0], &AJ[0], getAValues(), Symbolic, Numeric, &Common);

        // Solve the system!
        klu_solve(Symbolic, Numeric, nets - 1, 1, getBValues(), &Common);

        for (size_t i = 1; i < nets; i++) {
            system.b[i].lu = b[i - 1];
        }
    }

    // Creates a sparse representation of the matrix, this is what KLU takes as input. We also swap rows with columns here.
    void updateMatrix()
    {
        b.clear();
        AI.clear();
        AJ.clear();
        AVal.clear();
        nzpointers.clear();
        int nonzero = 0;

        for (size_t i = 1; i < nets; i++) {
            for (int j = 1; j < nets; j++) {
                if (system.A[j][i].g != 0 || system.A[j][i].prelu != 0 || system.A[j][i].gdyn.size() || system.A[j][i].gtimed != 0)
                    nonzero++;
            }
        }

        // Allocate memory for CSR format
        b.resize(nets - 1);
        AI.resize(nets);
        AJ.resize(nonzero);
        AVal.resize(nonzero);
        nzpointers.resize(nonzero);
        // Reset nonzeros
        nonzero = 0;
        AI[0] = 0;

        // Create our CSR format
        for (size_t i = 1; i < nets; i++) {
            b[i - 1] = system.b[i].lu;

            for (size_t j = 1; j < nets; j++) {
                system.A[j][i].nonzero = false;
                bool notzero = system.A[j][i].prelu != 0 || system.A[j][i].gdyn.size() || system.A[j][i].gtimed != 0;

                if (notzero) {
                    system.A[j][i].nonzero = true;
                    AVal[nonzero] = system.A[j][i].lu;
                    AJ[nonzero] = static_cast<int>(j - 1);
                    nzpointers[nonzero] = &(system.A[j][i]);
                    nonzero++;
                }
            }

            AI[i] = nonzero;
        }
    }

    // List of all non-zero values in the A matrix
    double* getAValues()
    {
        for (size_t i = 0; i < nzpointers.size(); i++) {
            AVal[i] = (*nzpointers[i]).lu;
        }

        return &AVal[0];
    }

    // List of all values in the B matrix
    double* getBValues()
    {
        for (size_t i = 1; i < nets; i++) {
            b[i - 1] = system.b[i].lu;
        }

        return &b[0];
    }

    double getArgumentValue(std::string arg) const
    {
        double result = 0.0f;

        auto replaceString = [](std::string subject, std::string const& search,
                                 std::string const& replace) {
            size_t pos = 0;
            while ((pos = subject.find(search, pos)) != std::string::npos) {
                subject.replace(pos, search.length(), replace);
                pos += replace.length();
            }
            return subject;
        };

        // Allow writing values as "100u" instead of "100e-6"
        // This is convenient when describing circuits
        arg = replaceString(arg, "p", "e-12");
        arg = replaceString(arg, "n", "e-9");
        arg = replaceString(arg, "u", "e-6");
        arg = replaceString(arg, "m", "e-3");
        arg = replaceString(arg, "k", "e3");

        try {
            result = std::stod(arg);
        } catch (...) {
            auto errorMessage = "circuit~: invalid circuit description argument \"" + arg + "\"";
            pd_error(NULL, errorMessage.c_str());
        }

        return result;
    }
};
