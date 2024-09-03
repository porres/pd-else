/*
 // Licenced under the GPL-v3
 // For information on usage and redistribution, and for a DISCLAIMER OF ALL
 // WARRANTIES, see the file, "LICENSE.txt," in this distribution.
 // Originally made by mystran, modified by Timothy Schoen
 */

#pragma once
#include <KLU/klu.h>

using NetlistDescription = std::vector<std::tuple<ComponentType, std::vector<std::string>, std::vector<int>, std::string>>;

// Circuits are built from nodes and Components, where nodes are
// simply positive integers (with 0 designating ground).
//
// Every Component has one or more pins connecting to the circuit
// nodes as well as zero or more internal nets.

struct NetList {
    typedef std::vector<std::unique_ptr<IComponent>> ComponentList;

    NetList(NetlistDescription& netlist, int nNets, double sampleRate)
        : lastNetlist(netlist)
        , nets(nNets)
        , lastNumNets(nNets)
    {
        auto isDynamicArgument = [](const std::string& arg) { return arg.rfind("$s", 0) == 0 || arg.rfind("$f", 0) == 0; };
        auto getModel = [](const std::string& componentName, const std::string& modelName) -> Model {
            
            auto& models = Models::getModelsForComponent(componentName);
            
            if(models.count(modelName))
            {
                return models.at(modelName);
            }
            else if(!modelName.empty())
            {
                pd_error(nullptr, "circuit~: unknown %s model %s", componentName.c_str(), modelName.c_str());
            }
            
            return {};
        };

        int numOut = 0;
        for (auto const& [type, args, pins, model] : netlist) {
            if(type == tProbe)
            {
                numOut++;
            }
        }
        
        if(numOut > 8)
        {
            pd_error(nullptr, "circuit~: too many probes, only 8 are allowed. Later probes will be ignored");
        }
        
        output.resize(numOut, 0.0);
        
        numOut = 0;
        
        for (auto const& [type, args, pins, model] : netlist) {
            switch (type) {
                case tResistor: {
                    if (args.size() == 1) {
                        if (isDynamicArgument(args[0])) {
                            addComponent(new VariableResistor(addDynamicArgument(args[0]), pins[0], pins[1]));
                        } else {
                            addComponent(new Resistor(getArgumentValue(args[0]), pins[0], pins[1]));
                        }
                    } else {
                        pd_error(nullptr, "circuit~: wrong number of arguments for resistor");
                    }
                    break;
                }
                case tCapacitor: {
                    if (args.size() == 1) {
                        addComponent(new Capacitor(getArgumentValue(args[0]), pins[0], pins[1]));
                    } else {
                        pd_error(nullptr, "circuit~: wrong number of arguments for capacitor");
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
                        pd_error(nullptr, "circuit~: wrong number of arguments for voltage");
                    }
                    break;
                }
                case tDiode: {
                    if (args.size() == 0) {
                        addComponent(new Diode(pins[0], pins[1], getModel("diode", model)));
                    } else {
                        pd_error(nullptr, "circuit~: wrong number of arguments for diode");
                    }
                    break;
                }
                case tBJT: {
                    if (args.size() == 1) {
                        addComponent(new BJT(pins[0], pins[1], pins[2], getModel("bjt", model), getArgumentValue(args[0])));
                    } else {
                        pd_error(nullptr, "circuit~: wrong number of arguments for bjt");
                    }
                    break;
                }
                case tMOSFET: {
                    if (args.size() == 1) {
                        addComponent(new MOSFET(getArgumentValue(args[0]), pins[0], pins[1], pins[2], getModel("mosfet", model)));
                    } else {
                        pd_error(nullptr, "circuit~: wrong number of arguments for mosfet");
                    }
                    break;
                }
                case tJFET: {
                    if (args.size() == 1) {
                        addComponent(new JFET(getArgumentValue(args[0]), pins[0], pins[1], pins[2], getModel("jfet", model)));
                    } else {
                        pd_error(nullptr, "circuit~: wrong number of arguments for jfet");
                    }
                    break;
                }
                case tTransformer: {
                    if (args.size() == 1) {
                        addComponent(new Transformer(getArgumentValue(args[0]), pins[0], pins[1], pins[2], pins[3]));
                    } else {
                        pd_error(nullptr, "circuit~: wrong number of arguments for transformer");
                    }
                    break;
                }
                case tGyrator: {
                    if (args.size() == 1) {
                        addComponent(new Gyrator(getArgumentValue(args[0]), pins[0], pins[1], pins[2], pins[3]));
                    } else {
                        pd_error(nullptr, "circuit~: wrong number of arguments for gyrator");
                    }
                    break;
                }
                case tInductor: {
                    if (args.size() == 1) {
                        addComponent(new Inductor(getArgumentValue(args[0]), pins[0], pins[1]));
                    } else {
                        pd_error(nullptr, "circuit~: wrong number of arguments for inductor");
                    }
                    break;
                }
                case tTriode: {
                    if (args.size() == 0) {
                        addComponent(new Triode(pins[0], pins[1], pins[2], getModel("triode", model)));
                    } else {
                        pd_error(nullptr, "circuit~: wrong number of arguments for triode");
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
                        pd_error(nullptr, "circuit~: wrong number of arguments for opamp");
                    }
                    
                    break;
                }
                case tOpAmp2: {
                    if (args.size() == 0) {
                        addComponent(new OpAmp2(pins[0], pins[1], pins[2], pins[3], pins[4], getModel("opamp", model)));
                    } else {
                        pd_error(nullptr, "circuit~: wrong number of arguments for opamp2");
                    }
                    
                    break;
                }
                case tPotmeter: {
                    if (args.size() == 2) {
                        if (isDynamicArgument(args[0])) {
                            addComponent(new Potentiometer(addDynamicArgument(args[0]), getArgumentValue(args[1]), pins[0], pins[1], pins[2]));
                        } else {
                            addComponent(new StaticPotentiometer(getArgumentValue(args[0]), getArgumentValue(args[1]), pins[0], pins[1], pins[2]));
                        }
                    } else {
                        pd_error(nullptr, "circuit~: wrong number of arguments for potmeter");
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
                        pd_error(nullptr, "circuit~: wrong number of arguments for current");
                    }
                    break;
                }
                /* Experimental components
                case tBuffer: {
                    if (args.size() == 0) {
                        addComponent(new VoltageFollower(pins[0], pins[1]));
                    } else {
                        pd_error(nullptr, "circuit~: wrong number of arguments for buffer");
                    }
                    break;
                }
                case tDelayBuffer: {
                    if (args.size() == 1) {
                        addComponent(new DelayBuffer(pins[0], pins[1]));
                    } else {
                        pd_error(nullptr, "circuit~: wrong number of arguments for delaybuffer");
                    }
                    break;
                }
                 case tTappedTransformer: {
                     if (args.size() == 1) {
                         addComponent(new TappedTransformer(getArgumentValue(args[0]), pins[0], pins[1], pins[2], pins[3],  pins[4]));
                     } else {
                         pd_error(nullptr, "circuit~: wrong number of arguments for taptransformer");
                     }
                     break;
                 }*/
                 case tPentode: {
                     if (args.size() == 0) {
                         addComponent(new Pentode(pins[0], pins[1], pins[2], pins[3], getModel("pentode", model)));
                     } else {
                         pd_error(nullptr, "circuit~: wrong number of arguments for triode");
                     }
                     break;
                 }
                 
                 
                case tSPST: {
                    if (args.size() == 1) {
                        if (isDynamicArgument(args[0])) {
                            addComponent(new Switch<1>(&addDynamicArgument(args[0]), pins));
                        } else {
                            addComponent(new Switch<1>(getArgumentValue(args[0]), pins));
                        }
                    } else {
                        pd_error(nullptr, "circuit~: wrong number of arguments for spst");
                    }
                    break;
                }

                case tSPDT: {
                    if (args.size() == 1) {
                        if (isDynamicArgument(args[0])) {
                            addComponent(new Switch<2>(&addDynamicArgument(args[0]), pins));
                        } else {
                            addComponent(new Switch<2>(getArgumentValue(args[0]), pins));
                        }
                    } else {
                        pd_error(nullptr, "circuit~: wrong number of arguments for spdt");
                    }
                    break;
                }
                case tProbe: {
                    if(numOut < 8) {
                        addComponent(new Probe(pins[0], pins[1], output[numOut]));
                    }
                    break;
                }
            }
        }
                
        auto AMatrix = MNAMatrix(nets, MNAVector(nets));
        auto xVector = MNAVector(nets);

        for (auto& c : components) {
            c->stamp(AMatrix, xVector);
        }

        initialiseMatrix(AMatrix, xVector);
        
        // Refactor KLU for DC solution
        refactorKLU();
        
        // get DC solution
        simulateTick();

        // Set time step size
        // Since we only ever change from step size 0 to a real samplerate, we shouldn't have to apply any time scaling for capacitors etc.
        for (int nz = 0; nz < timedA.size(); nz++) {
            staticA[nz] += timedA[nz] * sampleRate;
        }
        
        for (auto& c : components) {
            c->initTransientAnalysis(1.0 / sampleRate);
        }
        
        // Refactor KLU again for transient analysis
        refactorKLU();
        
        // this isn't pretty, but it helps against clicks
        for(int i = 0; i < 5; i++)
        {
            simulateTick();
        }
    }

    ~NetList()
    {
        if (symbolic)
            klu_free_symbolic(&symbolic, &common);
        if (numeric)
            klu_free_numeric(&numeric, &common);
    }

    void addComponent(IComponent* c)
    {
        c->setupNets(nets);
        components.push_back(std::unique_ptr<IComponent>(c));
    }

    void simulateTick()
    {
        // Clear last output
        std::fill(output.begin(), output.end(), 0.0);

        solve();
        update();
    }

    int getNumOutputs() const
    {
        return static_cast<int>(output.size());
    }

    double getOutput(int idx) const
    {
        return output[idx];
    }

    void setInput(int idx, double value)
    {
        input[idx] = value;
    }

    void setBlockDC(bool blockDC)
    {
        blockDC = blockDC;
        
        // Enable dcblock on all probe components
        for (auto& c : components) {
            c->setDCBlock(blockDC);
        }
    }

    int getMaxDynamicArgument() const
    {
        return maximumInputIndex;
    }

    void setMaxIter(int iter)
    {
        maxIter = iter;
    }

    int getMaxIter() const
    {
        return maxIter;
    }
    
    int getLastNumNets() const
    {
        return lastNumNets;
    }

    NetlistDescription getLastNetlist() const
    {
        return lastNetlist;
    }

private:

    // Creates a sparse representation of the matrix, this is what KLU takes as input. We also swap rows with columns here.
    void initialiseMatrix(MNAMatrix AMatrix, MNAVector xVector)
    {
        b.clear();
        AI.clear();
        AJ.clear();
        A.clear();
        int nonzero = 0;
        
        // count nonzero matrix values
        for (int i = 1; i < nets; i++) {
            for (int j = 1; j < nets; j++) {
                if (AMatrix[j][i].nonzero())
                    nonzero++;
            }
        }
        
        // Allocate memory for CSR format
        b.resize(nets, 0.0);
        AI.resize(nets);
        AJ.resize(nonzero);
        A.resize(nonzero, 0.0);

        // Copy g, gTimed and dyn to a more compact format that allows us to copy the static values over all at once
        staticA.resize(nonzero);
        timedA.resize(nonzero);
        dynamicA.resize(nonzero);
        
        // Do the same for the B vector
        staticB.resize(nets - 1);
        dynamicB.resize(nets - 1);
        
        // Reset nonzeros
        nonzero = 0;
        AI[0] = 0;
        
        // Create the CSR format that KLU requires
        for (int i = 1; i < nets; i++) {
            staticB[i - 1] = xVector[i].g;
            dynamicB[i - 1].insert(dynamicB[i - 1].end(), xVector[i].gdyn.begin(), xVector[i].gdyn.end());
            
            for (int j = 1; j < nets; j++) {
                if (AMatrix[j][i].nonzero()) {
                    AJ[nonzero] = static_cast<int>(j - 1);
                    staticA[nonzero] = AMatrix[j][i].g;
                    timedA[nonzero] = AMatrix[j][i].gtimed;
                    dynamicA[nonzero].insert(dynamicA[nonzero].end(), AMatrix[j][i].gdyn.begin(), AMatrix[j][i].gdyn.end());
                    nonzero++;
                }
            }
            
            AI[i] = nonzero;
        }
    }
    
    // Updates the A matrix and B vector pre-solve
    void updatePre()
    {
        // Copy static and dynamic values for B vector
        std::copy(staticB.begin(), staticB.end(), b.begin() + 1);
        for (int i = 0; i < dynamicB.size(); i++) {
            for (auto* dynamicBVal : dynamicB[i]) {
                b[i + 1] += *dynamicBVal;
            }
        }

        // Copy static and dynamic nonzero values for A vector
        std::copy(staticA.begin(), staticA.end(), A.begin());
        for (int nz = 0; nz < dynamicA.size(); nz++) {
            for (auto* dynamicNzVal : dynamicA[nz]) {
                A[nz] += *dynamicNzVal;
            }
        }
    }
    
    // Called for every time step
    void update()
    {
        for (const auto& c : components) {
            c->update(b);
        }
    }

    // Called for every Newton-Raphson iteration, returns true if we're done
    bool newton()
    {
        bool done = true;
        for (const auto& c : components) {
            done &= c->newton(b);
        }
        return done;
    }

    void refactorKLU()
    {
        if(symbolic || numeric)
        {
            klu_free_symbolic(&symbolic, &common);
            klu_free_numeric(&numeric, &common);
        }
        else {
            // Initialise KLU
            const bool noChecking = true;
            klu_defaults(&common);
            common.scale = noChecking ? -1 : 2;
        }

        // symbolic analysis
        symbolic = klu_analyze(nets - 1, AI.data(), AJ.data(), &common);

        // Full numeric factorization: Only needed once!
        numeric = klu_factor(AI.data(), AJ.data(), A.data(), symbolic, &common);
    }

    void solve()
    {
        for (int iter = 0; iter < std::max(maxIter, 1); iter++) {
            
            // restore matrix state and add dynamic values
            updatePre();

            // Update our factorization or refactor if the last factorization failed
            klu_refactor(AI.data(), AJ.data(), A.data(), symbolic, numeric, &common);

            // Solve the system!
            klu_solve(symbolic, numeric, nets - 1, 1, b.data() + 1, &common);

            // Check if all NR iterated objects have converged
            if (newton())
                break;
        }
    }
    
    // Pass in a value like $s1, $f1, $s2, and it returns the double reference that belongs to this argument
    double& addDynamicArgument(const std::string& arg)
    {
        try {
            int idx = std::stoi(arg.substr(2)) - 1;
            if (idx < 0) {
                throw std::runtime_error("dynamic argument index needs to be larger than 0");
            }
            
            if(idx >= 8)
            {
                throw std::runtime_error("too many dynamic arguments, only 8 are supported");
            }
            
            maximumInputIndex = std::max(idx + 1, maximumInputIndex);
            input[idx] = 0.0;
            return input[idx];
        } catch (std::runtime_error& e) {
            pd_error(nullptr, "circuit~: %s", e.what());
            input[0] = 0.0;
            return input[0];
        }
        catch (const std::invalid_argument & e) {
            pd_error(nullptr, "circuit~: malformed dynamic argument: \"%s\"", arg.c_str());
            input[0] = 0.0;
            return input[0];
        }
        catch (const std::out_of_range & e) {
            pd_error(nullptr, "circuit~: malformed dynamic argument: \"%s\"", arg.c_str());
            input[0] = 0.0;
            return input[0];
        }
    }

    // Read a simple int or double argument value
    static double getArgumentValue(std::string arg)
    {
        double result = 0.0;

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
        arg = replaceString(arg, "M", "e6");
        
        try {
            result = std::stod(arg);
        } catch (...) {
            pd_error(nullptr, "circuit~: invalid circuit description argument \%s\"", arg.c_str());
        }

        return result;
    }
    
    int nets;
    ComponentList components;

    // parameters
    int maxIter = 8;

    klu_symbolic* symbolic = nullptr;
    klu_numeric* numeric = nullptr;
    klu_common common;
    
    std::vector<int> AI;
    std::vector<int> AJ;
    
    std::vector<double> b;
    std::vector<double> A;
        
    std::vector<double> staticB;
    std::vector<std::vector<double*>> dynamicB;
    
    std::vector<double> staticA;
    std::vector<std::vector<double*>> dynamicA;
    std::vector<double> timedA;

    std::vector<double> output; // Probe object writes its output here
    std::array<double, 8> input; // Dynamic input argument values are stored here
    int maximumInputIndex = 1;
        
    // Netlist state that we can reset to
    const NetlistDescription lastNetlist;
    int const lastNumNets;
};
