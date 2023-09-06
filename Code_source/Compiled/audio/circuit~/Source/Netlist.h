#pragma once
#include <KLU/klu.h>


using NetlistDescription = std::vector<std::tuple<std::string, std::vector<std::string>, std::vector<int>>>;

struct NetList
{
    typedef std::vector<IComponent*> ComponentList;
    
    NetList(NetlistDescription& description, std::map<int, int> pins) : nets(static_cast<int>(pins.size())), pinAssignment(pins), netlistDescription(description)
    {
        int numOut = 0;
        for(auto& [name, args, pins] : netlistDescription)
        {
            if(name == "resistor")
            {
                if (args[0].rfind("$s", 0) == 0) {
                    addComponent(new VariableResistor(addDynamicArgument(args[0]), pinAssignment[pins[0]], pinAssignment[pins[1]]));
                }
                else {
                    addComponent(new Resistor(getArgumentValue(args[0]), pinAssignment[pins[0]], pinAssignment[pins[1]]));
                }
            }
            else if(name == "capacitor")
            {
                addComponent(new Capacitor(getArgumentValue(args[0]), pinAssignment[pins[0]], pinAssignment[pins[1]]));
            }
            else if(name == "voltage")
            {
                if (args[0].rfind("$s", 0) == 0) {
                    addComponent(new VariableVoltage(addDynamicArgument(args[0]), pinAssignment[pins[0]], pinAssignment[pins[1]]));
                }
                else {
                    addComponent(new Voltage(getArgumentValue(args[0]), pinAssignment[pins[0]], pinAssignment[pins[1]]));
                }
            }
            else if(name == "diode")
            {
                addComponent(new Diode(pinAssignment[pins[0]], pinAssignment[pins[1]]));
            }
            else if(name == "bjt")
            {
                addComponent(new BJT(pinAssignment[pins[0]], pinAssignment[pins[1]], pinAssignment[pins[2]], getArgumentValue(args[0])));
            }
            else if(name == "transformer")
            {
                addComponent(new Transformer(getArgumentValue(args[0]), pinAssignment[pins[0]], pinAssignment[pins[1]], pinAssignment[pins[2]], pinAssignment[pins[3]]));
            }
            if(name == "gyrator")
            {
                addComponent(new Gyrator(getArgumentValue(args[0]), pinAssignment[pins[0]], pinAssignment[pins[1]], pinAssignment[pins[2]], pinAssignment[pins[3]]));
            }
            else if(name == "inductor")
            {
                addComponent(new Inductor(getArgumentValue(args[0]), pinAssignment[pins[0]], pinAssignment[pins[1]]));
            }
            else if(name == "opamp")
            {
                addComponent(new OpAmp(getArgumentValue(args[0]), getArgumentValue(args[1]), pinAssignment[pins[0]], pinAssignment[pins[1]], pinAssignment[pins[2]]));
            }
            else if(name == "potmeter")
            {
                addComponent(new Potentiometer(addDynamicArgument(args[0]), getArgumentValue(args[1]), pinAssignment[pins[0]], pinAssignment[pins[1]], pinAssignment[pins[2]]));
            }
            else if(name == "probe")
            {
                addComponent(new Probe(pinAssignment[pins[0]], pinAssignment[pins[1]], numOut++));
            }
        }
    }
    
    void addComponent(IComponent* c)
    {
        c->setupNets(nets, c->getPinLocs());
        components.push_back(c);
    }
    
    void buildSystem()
    {
        system.setSize(nets);
        for(int i = 0; i < components.size(); ++i)
        {
            components[i]->stamp(system);
        }
        
        setStepScale(0);
        system.tStep = 0;
        
        updatePre();
        updateMatrix();
        
        initKLU();
    }
    
    void setTimeStep(double tStepSize)
    {
        for(int i = 0; i < components.size(); ++i)
        {
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
        for(int i = 0; i < nets; ++i)
        {
            system.b[i].initLU(stepScale);
            for(int j = 0; j < nets; ++j)
            {
                system.A[i][j].initLU(stepScale);
            }
        }
    }
    
    void simulateTick()
    {
        solve();
        system.time += system.tStep;
        update();
    }
    
    const MNASystem& getMNA() {
        return system;
    }
    
    void clearOutput() {
        std::fill(system.output.begin(), system.output.end(), 0.0f);
    }
    
    double getOutput(int idx) {
        return system.output[idx];
    }
    
    void setBlockDC(bool block_dc) {
        system.block_dc = block_dc;
    }
    
    double& addDynamicArgument(std::string arg)
    {
        size_t delimiterPos = arg.find("$s");
        
        int idx = 0;
        // Check if the delimiter was found
        if (delimiterPos != std::string::npos) {
            idx = std::stoi(arg.substr(delimiterPos + 2));
        }
        
        variableArgs[idx] = 0.0f;
        return variableArgs[idx];
    }
    
    int getNumDynamicArguments()
    {
        return static_cast<int>(variableArgs.size());
    }
    
    void setDynamicArgument(int idx, double value)
    {
        variableArgs[idx] = value;
    }
    
    void setMaxIter(int iter)
    {
        maxiter = iter;
    }
    
protected:
        
    int nets;
    ComponentList components;
    
    MNASystem system;
    
    // parameters
    int maxiter         = 20;
    double solvertol    = 1e-9;
    bool nochecking     = false;
    
    klu_symbolic* Symbolic;
    klu_numeric* Numeric;
    klu_common Common;
    
    std::vector<double> b;
    std::vector<double> AVal;
    std::vector<int> AI;
    std::vector<int> AJ;
    std::vector<MNACell*> nzpointers;
    
    std::map<int, double> variableArgs;
    
    void update()
    {
        for(int i = 0; i < components.size(); ++i)
        {
            components[i]->update(system);
        }
    }
    
    // return true if we're done
    bool newton()
    {
        bool done = true;
        for(int i = 0; i < components.size(); ++i)
        {
            done = done && components[i]->newton(system);
        }
        return done;
    }
    
    
    void updatePre()
    {
        for(int i = 0; i < nets; ++i)
        {
            system.b[i].updatePre();
        }
        
        for(int i = 0; i < nzpointers.size(); ++i)
        {
            nzpointers[i]->updatePre();
        }
    }
    
    void initKLU()
    {
        klu_defaults (&Common);
        Common.tol = solvertol;
        Common.scale = nochecking ? -1 : 2;
        
        // Symbolic analysis
        Symbolic = klu_analyze ((int)nets - 1, &AI[0], &AJ[0], &Common);
        // Full numeric factorization: Only needed once!
        Numeric = klu_factor (&AI[0], &AJ[0], getAValues(), Symbolic, &Common);
    }
    
    void refactorKLU()
    {
        klu_free_symbolic(&Symbolic, &Common);
        klu_free_numeric(&Numeric, &Common);
        
        // Symbolic analysis
        Symbolic = klu_analyze (nets - 1, &AI[0], &AJ[0], &Common);
        
        // Full numeric factorization: Only needed once!
        Numeric = klu_factor (&AI[0], &AJ[0], getAValues(), Symbolic, &Common);
    }
    
    void solve()
    {
        // Iterate junctions
        for(int iter = 0; iter < std::max(maxiter, 1); iter++)
        {
            // restore matrix state and add dynamic values
            updatePre();
            
            solveKLU();
            
            if(newton()) break;
        }
    }
    
    void solveKLU()
    {
        // Update our factorization or refactor if the last factorization failed
        klu_refactor (&AI[0], &AJ[0], getAValues(), Symbolic, Numeric, &Common);
        
        // Solve the system!
        klu_solve (Symbolic, Numeric, nets - 1, 1, getBValues(), &Common);
        
        for (size_t i = 1; i < nets; i++)
        {
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
        
        for (size_t i = 1; i < nets; i++)
        {
            for (int j = 1; j < nets; j++)
            {
                if(system.A[j][i].g != 0 || system.A[j][i].prelu != 0 || system.A[j][i].gdyn.size() || system.A[j][i].gtimed != 0)
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
        for (size_t i = 1; i < nets; i++)
        {
            b[i - 1] = system.b[i].lu;
            
            for (size_t j = 1; j < nets; j++)
            {
                system.A[j][i].nonzero = false;
                bool notzero = system.A[j][i].prelu != 0 || system.A[j][i].gdyn.size() || system.A[j][i].gtimed != 0;
                
                if (notzero)
                {
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
        for (size_t i = 0; i < nzpointers.size(); i++)
        {
            AVal[i] = (*nzpointers[i]).lu;
        }
        
        return &AVal[0];
    }
    
    // List of all values in the B matrix
    double* getBValues()
    {
        for (size_t i = 1; i < nets; i++)
        {
            b[i - 1] = system.b[i].lu;
        }
        
        return &b[0];
    }
    
    
    double getArgumentValue(std::string arg) {
        double result = 0.0f;
        
        auto replaceString = [](std::string subject, const std::string& search,
                           const std::string& replace) {
            size_t pos = 0;
            while((pos = subject.find(search, pos)) != std::string::npos) {
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
            pd_error(NULL, "Invalid circuit description argument");
        }
        
        return result;
    }
    
public:
    // Netlist state for resetting
    NetlistDescription netlistDescription;
    std::map<int, int> pinAssignment;
};
